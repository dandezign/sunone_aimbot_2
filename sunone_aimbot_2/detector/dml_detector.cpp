#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>
#include <dml_provider_factory.h>
#include <wrl/client.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <limits>
#include <algorithm>
#include <dxgi.h>

#include "dml_detector.h"
#include "sunone_aimbot_2.h"
// postProcess.h removed - YOLO26 doesn't use NMS
#include "capture.h"
#include "other_tools.h"
#ifdef USE_CUDA
#include "depth/depth_mask.h"
#endif

extern std::atomic<bool> detector_model_changed;
extern std::atomic<bool> detection_resolution_changed;
extern std::atomic<bool> detectionPaused;

namespace
{
bool tryInt64ToInt(int64_t value, int* out)
{
    if (!out)
    {
        return false;
    }

    if (value < static_cast<int64_t>(std::numeric_limits<int>::min()) ||
        value > static_cast<int64_t>(std::numeric_limits<int>::max()))
    {
        return false;
    }

    *out = static_cast<int>(value);
    return true;
}

#ifdef USE_CUDA
bool intersectsDepthMask(const cv::Rect& box, const cv::Mat& mask)
{
    if (box.width <= 0 || box.height <= 0 || mask.empty() || mask.type() != CV_8UC1)
        return false;

    const cv::Rect imageBounds(0, 0, mask.cols, mask.rows);
    const cv::Rect clipped = box & imageBounds;
    if (clipped.width <= 0 || clipped.height <= 0)
        return false;

    const int cx = clipped.x + clipped.width / 2;
    const int cy = clipped.y + clipped.height / 2;
    if (mask.at<uint8_t>(cy, cx) != 0)
        return true;

    const cv::Mat roi = mask(clipped);
    return cv::countNonZero(roi) > 0;
}

void filterDetectionsByDepthMask(std::vector<Detection>& detections)
{
    static cv::Mat holdTtl;

    if (detections.empty())
        return;

    if (!config.depth_inference_enabled || !config.depth_mask_enabled)
    {
        holdTtl.release();
        return;
    }

    const int holdFrames = std::clamp(config.depth_mask_hold_frames, 0, 120);
    cv::Mat currentMask = getCurrentDetectionSuppressionMask();
    cv::Mat suppressionMask;

    if (holdFrames <= 0)
    {
        holdTtl.release();
        suppressionMask = currentMask;
    }
    else
    {
        if (!currentMask.empty() && currentMask.type() == CV_8UC1)
        {
            if (holdTtl.empty() || holdTtl.size() != currentMask.size())
                holdTtl = cv::Mat::zeros(currentMask.size(), CV_16UC1);
            cv::subtract(holdTtl, cv::Scalar(1), holdTtl);
            holdTtl.setTo(cv::Scalar(static_cast<uint16_t>(holdFrames)), currentMask);
        }
        else if (!holdTtl.empty())
        {
            cv::subtract(holdTtl, cv::Scalar(1), holdTtl);
        }

        if (!holdTtl.empty() && cv::countNonZero(holdTtl) > 0)
        {
            cv::compare(holdTtl, cv::Scalar(0), suppressionMask, cv::CMP_GT);
        }
        else
        {
            suppressionMask.release();
        }
    }

    if (suppressionMask.empty() || suppressionMask.type() != CV_8UC1)
        return;

    detections.erase(
        std::remove_if(detections.begin(), detections.end(),
            [&suppressionMask](const Detection& det) { return intersectsDepthMask(det.box, suppressionMask); }),
        detections.end());
}
#else
void filterDetectionsByDepthMask(std::vector<Detection>&)
{
}
#endif

// YOLO26 decoder - replaces postProcessYoloDML for NMS-free inference
// Outputs are in model space (640x640), need to scale to game space
std::vector<Detection> decodeYolo26Outputs(
    const float* output,
    const std::vector<int64_t>& shape,
    int numClasses,
    float confThreshold,
    float x_gain,  // Scale from model X to game X
    float y_gain   // Scale from model Y to game Y
)
{
    std::vector<Detection> detections;
    
    // Validate input pointer
    if (!output) {
        return detections;
    }
    
    // Validate output shape: expect (N, 300, 6) or flat (1800,)
    const int64_t expectedSize = 300 * 6;
    const int64_t actualSize = shape.empty() ? 1800 : 
                               (shape.size() == 1 ? shape[0] : 
                                shape[0] * shape[1]);
    
    if (actualSize != expectedSize) {
        std::cerr << "[YOLO26] Warning: unexpected output shape " 
                  << "(expected " << expectedSize << ", got " << actualSize << ")" << std::endl;
        return detections;
    }
    
    // YOLO26 output: (N, 300, 6)
    // Format: [x, y, w, h, conf, class_id] in MODEL SPACE (640x640)
    
    for (int i = 0; i < 300; i++) {
        float conf = output[i * 6 + 4];
        
        // Skip low-confidence detections
        if (conf < confThreshold || std::isnan(conf) || std::isinf(conf)) {
            continue;
        }
        
        // Coordinates in model space (640x640)
        float model_x = output[i * 6 + 0];
        float model_y = output[i * 6 + 1];
        float model_w = output[i * 6 + 2];
        float model_h = output[i * 6 + 3];
        float classIdFloat = output[i * 6 + 5];
        
        // Validate coordinates (reject NaN/inf/negative)
        if (std::isnan(model_x) || std::isnan(model_y) || std::isnan(model_w) || std::isnan(model_h) ||
            std::isinf(model_x) || std::isinf(model_y) || std::isinf(model_w) || std::isinf(model_h) ||
            model_x < 0 || model_y < 0 || model_w <= 0 || model_h <= 0) {
            continue;
        }
        
        // Validate classId bounds
        int classId = static_cast<int>(classIdFloat);
        if (classId < 0 || classId >= numClasses) {
            continue;
        }
        
        // SCALE FROM MODEL SPACE TO GAME SPACE
        const int game_x = static_cast<int>(model_x * x_gain);
        const int game_y = static_cast<int>(model_y * y_gain);
        const int game_w = static_cast<int>(model_w * x_gain);
        const int game_h = static_cast<int>(model_h * y_gain);
        
        detections.push_back({
            cv::Rect(game_x, game_y, game_w, game_h),
            conf,
            classId
        });
    }
    
    return detections;
}
}

std::string GetDMLDeviceName(int deviceId)
{
    Microsoft::WRL::ComPtr<IDXGIFactory1> dxgiFactory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory))))
        return "Unknown";

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(dxgiFactory->EnumAdapters1(deviceId, &adapter)))
        return "Invalid device ID";

    DXGI_ADAPTER_DESC1 desc;
    if (FAILED(adapter->GetDesc1(&desc)))
        return "Failed to get description";

    std::wstring wname(desc.Description);
    return WideToUtf8(wname);
}

DirectMLDetector::DirectMLDetector(const std::string& model_path)
    :
    env(ORT_LOGGING_LEVEL_WARNING, "DML_Detector"),
    memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
{
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    session_options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    session_options.SetIntraOpNumThreads(1);
    session_options.SetInterOpNumThreads(1);

    Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(session_options, config.dml_device_id));

    if (config.verbose)
        std::cout << "[DirectML] Using adapter: " << GetDMLDeviceName(config.dml_device_id) << std::endl;

    initializeModel(model_path);
}

DirectMLDetector::~DirectMLDetector()
{
    shouldExit = true;
    inferenceCV.notify_all();
}

void DirectMLDetector::initializeModel(const std::string& model_path)
{
    std::wstring model_path_wide(model_path.begin(), model_path.end());
    session = Ort::Session(env, model_path_wide.c_str(), session_options);

    input_name = session.GetInputNameAllocated(0, allocator).get();
    output_name = session.GetOutputNameAllocated(0, allocator).get();

    Ort::TypeInfo input_type_info = session.GetInputTypeInfo(0);
    auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
    input_shape = input_tensor_info.GetShape();

    bool isStatic = true;
    for (auto d : input_shape) if (d <= 0) isStatic = false;

    if (isStatic != config.fixed_input_size)
    {
        config.fixed_input_size = isStatic;
        detector_model_changed.store(true);
        std::cout << "[DML] Automatically set fixed_input_size = " << (isStatic ? "true" : "false") << std::endl;
    }
}

std::vector<Detection> DirectMLDetector::detect(const cv::Mat& input_frame)
{
    std::vector<cv::Mat> batch = { input_frame };
    auto batchResult = detectBatch(batch);
    if (!batchResult.empty())
        return batchResult[0];
    else
        return {};
}

std::vector<std::vector<Detection>> DirectMLDetector::detectBatch(const std::vector<cv::Mat>& frames)
{
    std::vector<std::vector<Detection>> empty;
    if (frames.empty()) return empty;
    const size_t batch_size = frames.size();

    int model_h = -1;
    int model_w = -1;
    if (input_shape.size() > 2)
    {
        int converted = 0;
        if (tryInt64ToInt(input_shape[2], &converted))
        {
            model_h = converted;
        }
    }
    if (input_shape.size() > 3)
    {
        int converted = 0;
        if (tryInt64ToInt(input_shape[3], &converted))
        {
            model_w = converted;
        }
    }
    const bool useFixed = config.fixed_input_size && model_h > 0 && model_w > 0;

    const int target_h = useFixed ? model_h : config.detection_resolution;
    const int target_w = useFixed ? model_w : config.detection_resolution;

    auto t0 = std::chrono::steady_clock::now();
    std::vector<float> input_tensor_values(
        batch_size * static_cast<size_t>(3) * static_cast<size_t>(target_h) * static_cast<size_t>(target_w));

    for (size_t b = 0; b < batch_size; ++b)
    {
        cv::Mat bgrFrame;
        switch (frames[b].channels())
        {
        case 4:
            cv::cvtColor(frames[b], bgrFrame, cv::COLOR_BGRA2BGR);
            break;
        case 3:
            bgrFrame = frames[b];
            break;
        case 1:
            cv::cvtColor(frames[b], bgrFrame, cv::COLOR_GRAY2BGR);
            break;
        default:
            bgrFrame = cv::Mat::zeros(frames[b].size(), CV_8UC3);
            break;
        }

        cv::Mat resized;
        cv::resize(bgrFrame, resized, cv::Size(target_w, target_h));
        cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
        resized.convertTo(resized, CV_32FC3, 1.0f / 255.0f);

        const float* src = reinterpret_cast<const float*>(resized.data);
        for (int h = 0; h < target_h; ++h)
            for (int w = 0; w < target_w; ++w)
                for (int c = 0; c < 3; ++c)
                {
                    size_t dstIdx = b * static_cast<size_t>(3) * static_cast<size_t>(target_h) * static_cast<size_t>(target_w)
                        + static_cast<size_t>(c) * static_cast<size_t>(target_h) * static_cast<size_t>(target_w)
                        + static_cast<size_t>(h) * static_cast<size_t>(target_w)
                        + static_cast<size_t>(w);
                    input_tensor_values[dstIdx] = src[(h * target_w + w) * 3 + c];
                }
    }
    auto t1 = std::chrono::steady_clock::now();

    std::vector<int64_t> ort_input_shape{
        static_cast<int64_t>(batch_size),
        3,
        static_cast<int64_t>(target_h),
        static_cast<int64_t>(target_w)
    };
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_tensor_values.data(), input_tensor_values.size(),
        ort_input_shape.data(), ort_input_shape.size());

    const char* input_names[] = { input_name.c_str() };
    const char* output_names[] = { output_name.c_str() };

    auto t2 = std::chrono::steady_clock::now();
    auto output_tensors = session.Run(Ort::RunOptions{ nullptr },
        input_names, &input_tensor, 1,
        output_names, 1);
    auto t3 = std::chrono::steady_clock::now();

    float* outData = output_tensors.front().GetTensorMutableData<float>();
    Ort::TensorTypeAndShapeInfo outInfo = output_tensors.front().GetTensorTypeAndShapeInfo();
    std::vector<int64_t> outShape = outInfo.GetShape(); // [B, rows, cols]
    if (outShape.size() < 3)
    {
        std::cerr << "[DirectMLDetector] Unexpected output tensor rank." << std::endl;
        return empty;
    }

    int rows = 0;
    int cols = 0;
    if (!tryInt64ToInt(outShape[1], &rows) || !tryInt64ToInt(outShape[2], &cols) || rows <= 0 || cols <= 0)
    {
        std::cerr << "[DirectMLDetector] Output tensor dimensions are invalid." << std::endl;
        return empty;
    }
    const int num_classes = rows - 4;

    std::vector<std::vector<Detection>> batchDetections(batch_size);
    float conf_thr = config.confidence_threshold;

    auto t4 = std::chrono::steady_clock::now();

    for (size_t b = 0; b < batch_size; ++b)
    {
        const float* ptr = outData + b * rows * cols;
        std::vector<Detection> detections;

        std::vector<int64_t> shp = { static_cast<int64_t>(rows), static_cast<int64_t>(cols) };
        
        // Scale from model space (640x640) to game space
        const int gameWidth = config.detection_resolution;
        const int gameHeight = config.detection_resolution;
        const float x_gain = static_cast<float>(gameWidth) / 640.0f;
        const float y_gain = static_cast<float>(gameHeight) / 640.0f;
        
        detections = decodeYolo26Outputs(ptr, shp, num_classes, conf_thr, x_gain, y_gain);
        
        // No additional scaling needed - already scaled in decoder

        batchDetections[b] = std::move(detections);
    }
    auto t5 = std::chrono::steady_clock::now();

    lastPreprocessTimeDML = t1 - t0;
    lastInferenceTimeDML = t3 - t2;
    lastCopyTimeDML = t4 - t3;
    lastPostprocessTimeDML = t5 - t4;
    // lastNmsTimeDML removed - YOLO26 doesn't use NMS

    return batchDetections;
}

int DirectMLDetector::getNumberOfClasses()
{
    Ort::TypeInfo output_type_info = session.GetOutputTypeInfo(0);
    auto tensor_info = output_type_info.GetTensorTypeAndShapeInfo();
    std::vector<int64_t> output_shape = tensor_info.GetShape();

    if (output_shape.size() == 3)
    {
        int channels = 0;
        if (!tryInt64ToInt(output_shape[1], &channels))
        {
            std::cerr << "[DirectMLDetector] Output tensor channel dimension is invalid." << std::endl;
            return -1;
        }
        int num_classes = channels - 4;
        return num_classes;
    }
    else
    {
        std::cerr << "[DirectMLDetector] Unexpected output tensor shape." << std::endl;
        return -1;
    }
}

void DirectMLDetector::processFrame(const cv::Mat& frame)
{
    if (detectionPaused)
    {
        std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
        detectionBuffer.boxes.clear();
        detectionBuffer.classes.clear();
        detectionBuffer.version++;
        detectionBuffer.cv.notify_all();
        return;
    }
    std::unique_lock<std::mutex> lock(inferenceMutex);
    currentFrame = frame;
    frameReady = true;
    inferenceCV.notify_one();
}

void DirectMLDetector::dmlInferenceThread()
{
    try
    {
        while (!shouldExit)
        {
            if (detector_model_changed.load())
            {
                initializeModel("models/" + config.ai_model);
                detection_resolution_changed.store(true);
                detector_model_changed.store(false);
                std::cout << "[DML] Detector reloaded: " << config.ai_model << std::endl;
            }

            if (detectionPaused)
            {
                std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
                detectionBuffer.boxes.clear();
                detectionBuffer.classes.clear();
                detectionBuffer.version++;
                detectionBuffer.cv.notify_all();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            cv::Mat frame;
            bool hasNewFrame = false;
            {
                std::unique_lock<std::mutex> lock(inferenceMutex);
                if (!frameReady && !shouldExit)
                    inferenceCV.wait(lock, [this] { return frameReady || shouldExit; });

                if (shouldExit) break;

                if (frameReady)
                {
                    frame = std::move(currentFrame);
                    frameReady = false;
                    hasNewFrame = true;
                }
            }

            if (hasNewFrame && !frame.empty())
            {
                std::vector<cv::Mat> batchFrames = { frame };
                auto detectionsBatch = detectBatch(batchFrames);
                if (detectionsBatch.empty())
                {
                    continue;
                }
                const std::vector<Detection>& detections = detectionsBatch.back();
                std::vector<Detection> filteredDetections = detections;
                filterDetectionsByDepthMask(filteredDetections);

                std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
                detectionBuffer.boxes.clear();
                detectionBuffer.classes.clear();
                for (const auto& d : filteredDetections) {
                    detectionBuffer.boxes.push_back(d.box);
                    detectionBuffer.classes.push_back(d.classId);
                }
                detectionBuffer.version++;
                detectionBuffer.cv.notify_all();
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[DML] Inference thread crashed: " << e.what() << std::endl;
        shouldExit = true;
        inferenceCV.notify_all();
        detectionBuffer.cv.notify_all();
    }
    catch (...)
    {
        std::cerr << "[DML] Inference thread crashed: unknown exception." << std::endl;
        shouldExit = true;
        inferenceCV.notify_all();
        detectionBuffer.cv.notify_all();
    }
}
