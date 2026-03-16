#include "sam3.cuh"
#include <fstream>
#include <iomanip>

void SAM3_PCS::allocate_bbox_buffers() {
    const int num_instances = SAM3_MAX_INSTANCES;
    const size_t bbox_bytes = num_instances * SAM3_BBOX_COORDS * sizeof(int);
    const size_t score_bytes = num_instances * sizeof(float);
    
    cuda_check(cudaMalloc(&d_mask_bboxes, bbox_bytes),
               "allocating d_mask_bboxes");
    cuda_check(cudaMalloc(&d_image_bboxes, bbox_bytes),
               "allocating d_image_bboxes");
    cuda_check(cudaMalloc(&d_instance_scores, score_bytes),
               "allocating d_instance_scores");
    
    cuda_check(cudaHostAlloc(&h_bbox_buffer, bbox_bytes, cudaHostAllocDefault),
               "allocating h_bbox_buffer");
    cuda_check(cudaHostAlloc(&h_score_buffer, score_bytes, cudaHostAllocDefault),
               "allocating h_score_buffer");
    
    cudaMemset(d_mask_bboxes, -1, bbox_bytes);
    cudaMemset(d_image_bboxes, -1, bbox_bytes);
    cudaMemset(d_instance_scores, 0, score_bytes);
    memset(h_bbox_buffer, -1, bbox_bytes);
    memset(h_score_buffer, 0, score_bytes);
}

static size_t trt_element_size(nvinfer1::DataType dtype) {
  switch (dtype) {
  case nvinfer1::DataType::kFLOAT:
  case nvinfer1::DataType::kINT32:
    return 4;
  case nvinfer1::DataType::kHALF:
  case nvinfer1::DataType::kBF16:
    return 2;
  case nvinfer1::DataType::kINT8:
  case nvinfer1::DataType::kFP8:
    return 1;
  case nvinfer1::DataType::kINT64:
    return 8;
  case nvinfer1::DataType::kINT4:
    return 1; // packed type, rounded up by tensor volume; sufficient for this
              // app's bindings
  default:
    throw std::runtime_error(
        "Unsupported TensorRT data type in IO buffer allocation");
  }
}

SAM3_PCS::SAM3_PCS(const std::string engine_path, const float vis_alpha,
                   const float prob_threshold)
    : _engine_path(engine_path), _overlay_alpha(vis_alpha),
      _probability_threshold(prob_threshold),
      d_mask_bboxes(nullptr), d_image_bboxes(nullptr),
      d_instance_scores(nullptr), h_bbox_buffer(nullptr),
      h_score_buffer(nullptr), _current_class_id(0) {

  cuda_check(cudaStreamCreate(&sam3_stream), "creating CUDA stream for SAM3");

  trt_runtime = std::unique_ptr<nvinfer1::IRuntime>(
      nvinfer1::createInferRuntime(trt_logger));
  load_engine();
  trt_ctx = std::unique_ptr<nvinfer1::IExecutionContext>(
      trt_engine->createExecutionContext());

  check_zero_copy();
  allocate_io_buffers();
  allocate_bbox_buffers();
  setup_color_palette();

  bsize.x = 16;
  bsize.y = 16;
}

void SAM3_PCS::pin_opencv_matrices(cv::Mat &input_mat, cv::Mat &result_mat) {
  opencv_inbytes = input_mat.total() * input_mat.elemSize();

  cuda_check(
      cudaHostRegister(input_mat.data, opencv_inbytes, cudaHostRegisterDefault),
      " pinning opencv input Mat on host");
  // for most purposes the default flag is good enough, in my benchmarking
  // using others say readonly flag did not improve performance

  if (is_zerocopy) {
    cuda_check(cudaHostRegister(result_mat.data, opencv_inbytes,
                                cudaHostRegisterDefault),
               " pinning opencv result Mat on host");

    cuda_check(cudaHostGetDevicePointer(&zc_input, input_mat.data, 0),
               " getting GPU pointer for input Mat");

    cuda_check(cudaHostGetDevicePointer(&gpu_result, result_mat.data, 0),
               " getting GPU pointer for result Mat");
  } else {
    // on dGPU allocate additional memory for input
    cuda_check(cudaMalloc(&opencv_input, opencv_inbytes),
               " allocating opencv input memory on a dGPU system");
    cuda_check(cudaMalloc((void **)&gpu_result, opencv_inbytes),
               " allocating result memory on a dGPU system");
    cudaMemset(opencv_input, 0, opencv_inbytes);
    cudaMemset((void *)gpu_result, 0, opencv_inbytes);
  }
}

void SAM3_PCS::visualize_on_dGPU(const cv::Mat &input, cv::Mat &result,
                                 SAM3_VISUALIZATION vis_type) {
  if (is_zerocopy) {
    input_ptr = zc_input;
  } else {
    input_ptr = static_cast<uint8_t *>(opencv_input);
  }

  if (vis_type == SAM3_VISUALIZATION::VIS_SEMANTIC_SEGMENTATION) {
    dim3 sbsize(16, 16);
    dim3 sgsize;
    sgsize.x = (input.cols + THREAD_COARSENING_FACTOR * sbsize.x - 1) /
               (THREAD_COARSENING_FACTOR * sbsize.x);
    sgsize.y = (input.rows + THREAD_COARSENING_FACTOR * sbsize.y - 1) /
               (THREAD_COARSENING_FACTOR * sbsize.y);

    draw_semantic_seg_mask<<<sgsize, sbsize, 0, sam3_stream>>>(
        input_ptr, static_cast<float *>(output_gpu[1]), gpu_result, input.cols,
        input.rows, input.channels(), SAM3_OUTMASK_WIDTH, SAM3_OUTMASK_HEIGHT,
        _overlay_alpha, _probability_threshold, make_float3(0, 185, 118));
  } else if (vis_type == SAM3_VISUALIZATION::VIS_INSTANCE_SEGMENTATION) {
    dim3 ibsize(8, 8, 8); // 3D block
    dim3 igsize;

    igsize.x = (input.cols + THREAD_COARSENING_FACTOR * ibsize.x - 1) /
               (THREAD_COARSENING_FACTOR * ibsize.x);
    igsize.y = (input.rows + THREAD_COARSENING_FACTOR * ibsize.y - 1) /
               (THREAD_COARSENING_FACTOR * ibsize.y);
    // 2D grid

    cuda_check(cudaMemcpyAsync((void *)gpu_result, (void *)input_ptr,
                               opencv_inbytes, cudaMemcpyDeviceToDevice,
                               sam3_stream),
               " async memcpy for result during instance seg visualization");

    for (int _mask_channel_idx = 0; _mask_channel_idx < 200;
         _mask_channel_idx += ibsize.z) {
      draw_instance_seg_mask<<<igsize, ibsize, 0, sam3_stream>>>(
          input_ptr, static_cast<float *>(output_gpu[0]), gpu_result,
          input.cols, input.rows, input.channels(), SAM3_OUTMASK_WIDTH,
          SAM3_OUTMASK_HEIGHT, _mask_channel_idx, _overlay_alpha,
          _probability_threshold, gpu_colpal);
    }
  }

  if (!is_zerocopy && vis_type == SAM3_VISUALIZATION::VIS_NONE) {
    cudaMemcpyAsync(output_cpu[0], output_gpu[0], output_sizes[0],
                    cudaMemcpyDeviceToHost, sam3_stream);
    cudaMemcpyAsync(output_cpu[1], output_gpu[1], output_sizes[1],
                    cudaMemcpyDeviceToHost, sam3_stream);
  } else if (!is_zerocopy) {
    cudaMemcpyAsync((void *)result.data, (void *)gpu_result, opencv_inbytes,
                    cudaMemcpyDeviceToHost, sam3_stream);
  }

  // if is_zerocopy, there is no need to do any synchronization/copy
  // to make the result visible to the CPU
}

bool SAM3_PCS::infer_on_dGPU(const cv::Mat &input, cv::Mat &result,
                             SAM3_VISUALIZATION vis_type) {
  gsize.x = (in_width + bsize.x - 1) / (THREAD_COARSENING_FACTOR * bsize.x);
  gsize.y = (in_height + bsize.y - 1) / (THREAD_COARSENING_FACTOR * bsize.y);

  cuda_check(cudaMemcpyAsync(opencv_input, input.data, opencv_inbytes,
                             cudaMemcpyHostToDevice, sam3_stream),
             " async memcpy of opencv image");

  pre_process_sam3<<<gsize, bsize, 0, sam3_stream>>>(
      static_cast<uint8_t *>(opencv_input),
      static_cast<float *>(input_gpu[image_index]), input.cols, input.rows,
      input.channels(), in_width, in_height);

  bool res = trt_ctx->enqueueV3(sam3_stream);

  visualize_on_dGPU(input, result, vis_type);
  cudaStreamSynchronize(sam3_stream);
  return res;
}

bool SAM3_PCS::infer_on_iGPU(const cv::Mat &input, cv::Mat &result,
                             SAM3_VISUALIZATION vis_type) {
  gsize.x = (in_width + bsize.x - 1) / (THREAD_COARSENING_FACTOR * bsize.x);
  gsize.y = (in_height + bsize.y - 1) / (THREAD_COARSENING_FACTOR * bsize.y);

  pre_process_sam3<<<gsize, bsize, 0, sam3_stream>>>(
      zc_input, static_cast<float *>(input_gpu[image_index]), input.cols,
      input.rows, input.channels(), in_width, in_height);

  bool res = trt_ctx->enqueueV3(sam3_stream);
  visualize_on_dGPU(input, result, vis_type);
  cudaStreamSynchronize(sam3_stream);

  return res;
}

bool SAM3_PCS::infer_on_image(const cv::Mat &input, cv::Mat &result,
                              SAM3_VISUALIZATION vis_type) {
  if (is_zerocopy) {
    return infer_on_iGPU(input, result, vis_type);
  }

  return infer_on_dGPU(input, result, vis_type);
}

// only for engine benchmarking purposes
bool SAM3_PCS::run_blind_inference() {
  bool res = trt_ctx->enqueueV3(sam3_stream);
  cudaStreamSynchronize(sam3_stream);
  return res;
}

void SAM3_PCS::load_engine() {
  size_t file_size = std::filesystem::file_size(_engine_path);

  if (file_size == 0) {
    std::stringstream err;
    err << "Engine file is empty";
    throw std::runtime_error(err.str());
  }

  char *engine_data = (char *)malloc(file_size);
  std::ifstream file(_engine_path, std::ios::binary);

  if (!file.is_open()) {
    std::stringstream err;
    err << "File " << _engine_path
        << " could not be opened. Please check permissions\n";
    throw std::runtime_error(err.str());
  }

  file.read(engine_data, file_size);
  file.close();

  trt_engine = std::unique_ptr<nvinfer1::ICudaEngine>(
      trt_runtime->deserializeCudaEngine(engine_data, file_size));

  free((void *)engine_data);
}

void SAM3_PCS::check_zero_copy() {
  // ToDo: support multi GPU for x86
  int gpu, is_integrated;
  cuda_check(cudaGetDevice(&gpu), " getting GPU device");

  cuda_check(cudaDeviceGetAttribute(&is_integrated, cudaDevAttrIntegrated, gpu),
             " checking for zero-copy property");

  is_zerocopy = (is_integrated > 0);

  if (is_zerocopy) {
    std::cout
        << "Running on a zero-copy platform. I/O binding buffers will be shared"
        << std::endl;
  } else {
    std::cout << "Running on dGPU. I/O binding buffers will be copied"
              << std::endl;
  }
}

void SAM3_PCS::allocate_io_buffers() {
  int nb_io = trt_engine->getNbIOTensors();

  for (int io_idx = 0; io_idx <= nb_io - 1; io_idx++) {
    const char *name = trt_engine->getIOTensorName(io_idx);
    nvinfer1::TensorIOMode mode = trt_engine->getTensorIOMode(name);

    nvinfer1::Dims dims = trt_engine->getTensorShape(name);
    size_t nbytes = trt_element_size(trt_engine->getTensorDataType(name));

    for (int idx = 0; idx < dims.nbDims; idx++) {
      nbytes *= static_cast<size_t>(std::max(1, static_cast<int>(dims.d[idx])));
    }

    void *cpu_buf, *gpu_buf;

    cuda_check(cudaHostAlloc(&cpu_buf, nbytes, cudaHostAllocMapped),
               " allocating io CPU buffer");

    std::memset(cpu_buf, 0, nbytes);

    if (is_zerocopy) {
      cuda_check(cudaHostGetDevicePointer(&gpu_buf, cpu_buf, 0),
                 " accessing shared CPU/GPU pointer on zero-copy platform");
    } else {
      // most likely x86
      cuda_check(cudaMalloc(&gpu_buf, nbytes),
                 " allocating GPU memory on dGPU system");
      cudaMemset(gpu_buf, 0, nbytes);
    }

    trt_ctx->setTensorAddress(name, gpu_buf);

    if (mode == nvinfer1::TensorIOMode::kINPUT) {
      std::cout << "Found input tensor " << name << std::endl;
      _input_names.push_back(std::string(name));
      input_cpu.push_back(cpu_buf);
      input_gpu.push_back(gpu_buf);

      if (dims.d[1] == 3) // typically 3 input channels for SAM like models
      {
        in_width = dims.d[3];
        in_height = dims.d[2];
        image_index = input_cpu.size() - 1;
        std::cout << "Input image has dimensions " << in_width << " x "
                  << in_height << std::endl;
      }
    } else if (mode == nvinfer1::TensorIOMode::kOUTPUT) {
      std::cout << "Found output tensor " << name << std::endl;
      _output_names.push_back(std::string(name));
      output_cpu.push_back(cpu_buf);
      output_gpu.push_back(gpu_buf);
      output_sizes.push_back(nbytes);
    } else {
      std::cout << "I/O binding " << name
                << " has an unknown mode: " << static_cast<int>(mode)
                << ", this is most likely a bug in the application"
                << std::endl;
    }
  }
}

void SAM3_PCS::set_prompt(std::vector<int64_t> &input_ids,
                          std::vector<int64_t> &input_attention_mask) {
  auto iid_obj =
      std::find(_input_names.begin(), _input_names.end(), "input_ids");
  auto iam_obj =
      std::find(_input_names.begin(), _input_names.end(), "attention_mask");

  if (iid_obj == _input_names.end()) {
    throw std::runtime_error(
        "input_ids not found in input names, seems to be a bug");
  }

  if (iam_obj == _input_names.end()) {
    throw std::runtime_error(
        "attention_mask not found in input names, seems to be a bug");
  }

  int iid_idx = std::distance(_input_names.begin(), iid_obj);
  int iam_idx = std::distance(_input_names.begin(), iam_obj);

  std::memcpy((int64_t *)input_cpu[iid_idx], input_ids.data(),
              input_ids.size() * sizeof(int64_t));

  std::memcpy((int64_t *)input_cpu[iam_idx], input_attention_mask.data(),
              input_attention_mask.size() * sizeof(int64_t));

  if (!is_zerocopy) {
    cudaMemcpyAsync(input_gpu[iid_idx], input_cpu[iid_idx],
                    input_ids.size() * sizeof(int64_t), cudaMemcpyHostToDevice,
                    sam3_stream);

    cudaMemcpyAsync(input_gpu[iam_idx], input_cpu[iam_idx],
                    input_attention_mask.size() * sizeof(int64_t),
                    cudaMemcpyHostToDevice, sam3_stream);

    cudaStreamSynchronize(sam3_stream);
  }
}

void SAM3_PCS::set_class_id(int class_id) {
    _current_class_id = class_id;
}

void SAM3_PCS::setup_color_palette() {
  cuda_check(cudaMalloc(&gpu_colpal, colpal.size() * sizeof(float3)),
             " allocating color palette on GPU");

  cuda_check(cudaMemcpyAsync((void *)gpu_colpal, (void *)colpal.data(),
                             colpal.size() * sizeof(float3),
                             cudaMemcpyHostToDevice, sam3_stream),
             " async memcpy for color pallete");

  cudaStreamSynchronize(sam3_stream);
}

std::vector<SAM3_PCS_RESULT> SAM3_PCS::extract_bboxes_cuda_kernel(
    const cv::Size& original_size,
    const SAM3_BBOX_OPTIONS& options)
{
    std::vector<SAM3_PCS_RESULT> results;
    
    if (output_gpu.empty() || !output_gpu[0]) {
        std::cerr << "Warning: No inference results available. Call infer_on_image() first."
                  << std::endl;
        return results;
    }
    
    const int num_instances = SAM3_MAX_INSTANCES;
    const int mask_width = SAM3_OUTMASK_WIDTH;
    const int mask_height = SAM3_OUTMASK_HEIGHT;
    
    compute_bboxes_from_masks<<<num_instances, mask_height, 0, sam3_stream>>>(
        static_cast<float*>(output_gpu[0]),
        d_mask_bboxes,
        d_instance_scores,
        num_instances,
        mask_width,
        mask_height,
        _probability_threshold
    );
    
    int block_size = 256;
    int grid_size = (num_instances + block_size - 1) / block_size;
    scale_bboxes_to_image<<<grid_size, block_size, 0, sam3_stream>>>(
        d_mask_bboxes,
        d_image_bboxes,
        num_instances,
        mask_width,
        mask_height,
        original_size.width,
        original_size.height
    );
    
    const size_t bbox_bytes = num_instances * SAM3_BBOX_COORDS * sizeof(int);
    const size_t score_bytes = num_instances * sizeof(float);
    
    cudaMemcpyAsync(h_bbox_buffer, d_image_bboxes, bbox_bytes,
                    cudaMemcpyDeviceToHost, sam3_stream);
    cudaMemcpyAsync(h_score_buffer, d_instance_scores, score_bytes,
                    cudaMemcpyDeviceToHost, sam3_stream);
    
    int* h_mask_bbox_temp;
    cudaHostAlloc(&h_mask_bbox_temp, bbox_bytes, cudaHostAllocDefault);
    cudaMemcpyAsync(h_mask_bbox_temp, d_mask_bboxes, bbox_bytes,
                    cudaMemcpyDeviceToHost, sam3_stream);
    
    cudaStreamSynchronize(sam3_stream);
    
    for (int i = 0; i < num_instances; ++i) {
        float score = h_score_buffer[i];
        
        if (score < options.score_threshold) continue;
        
        int mask_x_min = h_mask_bbox_temp[i * 4 + 0];
        int mask_y_min = h_mask_bbox_temp[i * 4 + 1];
        int mask_x_max = h_mask_bbox_temp[i * 4 + 2];
        int mask_y_max = h_mask_bbox_temp[i * 4 + 3];
        
        if (mask_x_min < 0) continue;
        
        int img_x_min = h_bbox_buffer[i * 4 + 0];
        int img_y_min = h_bbox_buffer[i * 4 + 1];
        int img_x_max = h_bbox_buffer[i * 4 + 2];
        int img_y_max = h_bbox_buffer[i * 4 + 3];
        
        int mask_w = mask_x_max - mask_x_min + 1;
        int mask_h = mask_y_max - mask_y_min + 1;
        int img_w = img_x_max - img_x_min + 1;
        int img_h = img_y_max - img_y_min + 1;
        
        if (img_w * img_h < options.min_box_area) continue;
        
        SAM3_PCS_RESULT result;
        result.score = score;
        result.class_id = _current_class_id;
        
        result.box_x = img_x_min;
        result.box_y = img_y_min;
        result.box_w = img_w;
        result.box_h = img_h;
        
        result.mask_x = mask_x_min;
        result.mask_y = mask_y_min;
        result.mask_w = mask_w;
        result.mask_h = mask_h;
        
        results.push_back(result);
    }
    
    cudaFreeHost(h_mask_bbox_temp);
    
    return results;
}

SAM3_PCS::~SAM3_PCS() {
  for (auto &ptr : input_gpu) {
    if (ptr) {
      cudaFree(ptr);
    }
  }

  for (auto &ptr : output_gpu) {
    if (ptr) {
      cudaFree(ptr);
    }
  }

  if (d_mask_bboxes) cudaFree(d_mask_bboxes);
  if (d_image_bboxes) cudaFree(d_image_bboxes);
  if (d_instance_scores) cudaFree(d_instance_scores);
  if (h_bbox_buffer) cudaFreeHost(h_bbox_buffer);
  if (h_score_buffer) cudaFreeHost(h_score_buffer);
}