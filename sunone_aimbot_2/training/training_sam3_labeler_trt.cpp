#define _CRT_SECURE_NO_WARNINGS
#include "sunone_aimbot_2/training/training_sam3_labeler.h"

#ifdef USE_CUDA

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cuda_fp16.h>
#include <cuda_runtime_api.h>
#include <NvInfer.h>

namespace training {
namespace {

constexpr const char* kDefaultNotInitializedMessage = "SAM3 TensorRT not initialized";
constexpr const char* kImageTensorName = "pixel_values";
constexpr const char* kInputIdsTensorName = "input_ids";
constexpr const char* kAttentionMaskTensorName = "attention_mask";
constexpr const char* kInstanceMasksTensorName = "instance_masks";
constexpr const char* kSemanticSegTensorName = "semantic_seg";
constexpr int kDefaultInputSize = 1008;
constexpr int kDefaultTokenCount = 32;
template <typename T>
struct TensorRtDeleter {
    void operator()(T* ptr) const {
        delete ptr;
    }
};

template <typename T>
using TensorRtPtr = std::unique_ptr<T, TensorRtDeleter<T>>;

struct TensorBuffer {
    std::string name;
    nvinfer1::DataType type = nvinfer1::DataType::kFLOAT;
    nvinfer1::Dims dims{};
    size_t elementCount = 0;
    size_t byteCount = 0;
    void* device = nullptr;
};

class Sam3Logger final : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            OutputDebugStringA(msg);
            OutputDebugStringA("\n");
        }
    }
};

Sam3Logger gSam3Logger;

bool CheckCuda(cudaError_t error, const char* action, std::string& message) {
    if (error == cudaSuccess) {
        return true;
    }

    message = std::string(action) + ": " + cudaGetErrorString(error);
    return false;
}

size_t GetElementSize(nvinfer1::DataType type) {
    switch (type) {
    case nvinfer1::DataType::kFLOAT:
        return sizeof(float);
    case nvinfer1::DataType::kHALF:
        return sizeof(__half);
    case nvinfer1::DataType::kINT64:
        return sizeof(int64_t);
    case nvinfer1::DataType::kINT32:
        return sizeof(int32_t);
    case nvinfer1::DataType::kINT8:
        return sizeof(int8_t);
    case nvinfer1::DataType::kBOOL:
        return sizeof(bool);
    case nvinfer1::DataType::kUINT8:
        return sizeof(uint8_t);
    default:
        return 0;
    }
}

bool HasDynamicDims(const nvinfer1::Dims& dims) {
    for (int i = 0; i < dims.nbDims; ++i) {
        if (dims.d[i] <= 0) {
            return true;
        }
    }

    return false;
}

size_t GetElementCount(const nvinfer1::Dims& dims) {
    size_t count = 1;
    for (int i = 0; i < dims.nbDims; ++i) {
        const int64_t dim = dims.d[i] > 0 ? dims.d[i] : 1;
        count *= static_cast<size_t>(dim);
    }
    return count;
}

float Sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

cv::Rect ScaleMaskBoundsToFrame(int minX,
                                int minY,
                                int maxX,
                                int maxY,
                                int maskWidth,
                                int maskHeight,
                                int frameWidth,
                                int frameHeight) {
    const float scaleX = static_cast<float>(frameWidth) / static_cast<float>(maskWidth);
    const float scaleY = static_cast<float>(frameHeight) / static_cast<float>(maskHeight);

    const int left = std::clamp(static_cast<int>(std::floor(minX * scaleX)), 0, frameWidth - 1);
    const int top = std::clamp(static_cast<int>(std::floor(minY * scaleY)), 0, frameHeight - 1);
    const int right = std::clamp(static_cast<int>(std::ceil((maxX + 1) * scaleX)), 1, frameWidth);
    const int bottom = std::clamp(static_cast<int>(std::ceil((maxY + 1) * scaleY)), 1, frameHeight);
    return cv::Rect(left, top, std::max(0, right - left), std::max(0, bottom - top));
}

Sam3LabelResult DecodeBoxesFromInstanceMasks(const std::vector<float>& masks,
                                             const TensorBuffer& masksBuffer,
                                             int frameWidth,
                                             int frameHeight,
                                             const Sam3PostprocessSettings& settings) {
    Sam3LabelResult result;
    if (masksBuffer.dims.nbDims != 4) {
        return result;
    }

    const int batch = static_cast<int>(masksBuffer.dims.d[0]);
    const int maskCount = static_cast<int>(masksBuffer.dims.d[1]);
    const int maskHeight = static_cast<int>(masksBuffer.dims.d[2]);
    const int maskWidth = static_cast<int>(masksBuffer.dims.d[3]);
    result.counters.rawMaskSlots = maskCount;
    if (batch != 1 || maskCount <= 0 || maskHeight <= 0 || maskWidth <= 0) {
        return result;
    }

    const size_t planeSize = static_cast<size_t>(maskWidth) * static_cast<size_t>(maskHeight);
    std::vector<DetectionBox> survivors;
    
    for (int maskIndex = 0; maskIndex < maskCount; ++maskIndex) {
        const size_t base = static_cast<size_t>(maskIndex) * planeSize;
        if (base + planeSize > masks.size()) {
            break;
        }

        int minX = maskWidth;
        int minY = maskHeight;
        int maxX = -1;
        int maxY = -1;
        int positivePixels = 0;
        float totalProbability = 0.0f;

        for (int y = 0; y < maskHeight; ++y) {
            for (int x = 0; x < maskWidth; ++x) {
                const float probability = Sigmoid(masks[base + static_cast<size_t>(y) * maskWidth + x]);
                if (probability < settings.maskThreshold) {
                    continue;
                }

                ++positivePixels;
                totalProbability += probability;
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }

        if (positivePixels <= 0) {
            continue;
        }

        ++result.counters.nonEmptyMasks;

        if (maxX < minX || maxY < minY) {
            continue;
        }

        if (positivePixels < settings.minMaskPixels) {
            continue;
        }

        ++result.counters.afterMinMaskPixels;

        const cv::Rect rect = ScaleMaskBoundsToFrame(
            minX, minY, maxX, maxY, maskWidth, maskHeight, frameWidth, frameHeight);
        if (rect.width <= 0 || rect.height <= 0) {
            continue;
        }

        if (rect.width < settings.minBoxWidth || rect.height < settings.minBoxHeight) {
            continue;
        }

        ++result.counters.afterMinBoxWidthHeight;

        const float avgProbability = totalProbability / static_cast<float>(positivePixels);
        const int boxArea = rect.width * rect.height;
        const int maskArea = (maxX - minX + 1) * (maxY - minY + 1);
        const float fillRatio = maskArea > 0 
            ? static_cast<float>(positivePixels) / static_cast<float>(maskArea)
            : 0.0f;

        if (fillRatio < settings.minMaskFillRatio) {
            continue;
        }

        ++result.counters.afterMinMaskFillRatio;

        const float confidence = avgProbability * std::min(fillRatio / std::max(settings.minMaskFillRatio, 0.0001f), 1.0f);

        if (confidence < settings.minConfidence) {
            continue;
        }

        ++result.counters.afterMinConfidence;

        survivors.push_back(DetectionBox{0, rect, confidence});
    }

    result.counters.finalBoxesBeforeCap = static_cast<int>(survivors.size());

    std::sort(survivors.begin(), survivors.end(), 
        [](const DetectionBox& a, const DetectionBox& b) {
            return a.confidence > b.confidence;
        });

    if (static_cast<int>(survivors.size()) > settings.maxDetections) {
        survivors.resize(static_cast<size_t>(settings.maxDetections));
    }

    result.counters.finalBoxesRendered = static_cast<int>(survivors.size());
    result.boxes = std::move(survivors);
    result.success = true;

    return result;
}

std::vector<float> ReadFloatTensor(const TensorBuffer& buffer, cudaStream_t stream, std::string& error) {
    std::vector<float> values;
    values.resize(buffer.elementCount);

    if (buffer.type == nvinfer1::DataType::kFLOAT) {
        if (!CheckCuda(cudaMemcpyAsync(values.data(), buffer.device, buffer.byteCount,
                                       cudaMemcpyDeviceToHost, stream),
                       ("cudaMemcpyAsync " + buffer.name).c_str(), error)) {
            return {};
        }
        if (!CheckCuda(cudaStreamSynchronize(stream), "cudaStreamSynchronize", error)) {
            return {};
        }
        return values;
    }

    if (buffer.type == nvinfer1::DataType::kHALF) {
        std::vector<__half> halves(buffer.elementCount);
        if (!CheckCuda(cudaMemcpyAsync(halves.data(), buffer.device, buffer.byteCount,
                                       cudaMemcpyDeviceToHost, stream),
                       ("cudaMemcpyAsync " + buffer.name).c_str(), error)) {
            return {};
        }
        if (!CheckCuda(cudaStreamSynchronize(stream), "cudaStreamSynchronize", error)) {
            return {};
        }
        for (size_t i = 0; i < halves.size(); ++i) {
            values[i] = __half2float(halves[i]);
        }
        return values;
    }

    error = "Unsupported output dtype for tensor '" + buffer.name + "'";
    return {};
}

cv::Mat ToBgrFrame(const cv::Mat& frame) {
    if (frame.channels() == 3) {
        return frame;
    }

    cv::Mat converted;
    if (frame.channels() == 4) {
        cv::cvtColor(frame, converted, cv::COLOR_BGRA2BGR);
    } else if (frame.channels() == 1) {
        cv::cvtColor(frame, converted, cv::COLOR_GRAY2BGR);
    }
    return converted;
}

}  // namespace

struct Sam3Labeler::Impl {
    std::atomic<bool> stopRequested{false};
    bool initialized = false;
    std::string availabilityMessage = kDefaultNotInitializedMessage;
    std::string enginePath;
    int inputWidth = 0;
    int inputHeight = 0;
    int tokenCount = kDefaultTokenCount;
    TensorRtPtr<nvinfer1::IRuntime> runtime;
    TensorRtPtr<nvinfer1::ICudaEngine> engine;
    TensorRtPtr<nvinfer1::IExecutionContext> context;
    cudaStream_t stream = nullptr;
    std::unordered_map<std::string, TensorBuffer> buffers;
};

Sam3Labeler::Sam3Labeler() : impl_(new Impl()) {
}

Sam3Labeler::~Sam3Labeler() {
    Shutdown();
    delete impl_;
}

#include <iostream>

bool Sam3Labeler::Initialize(const std::string& enginePath) {
    std::cerr << "[SAM3] Initialize() started" << std::endl;
    Shutdown();
    impl_->stopRequested.store(false);

    const auto cleanup = [&]() {
        if (impl_->stream != nullptr) {
            cudaStreamDestroy(impl_->stream);
            impl_->stream = nullptr;
        }
        for (auto& entry : impl_->buffers) {
            if (entry.second.device != nullptr) {
                cudaFree(entry.second.device);
                entry.second.device = nullptr;
            }
        }
        impl_->buffers.clear();
        impl_->context.reset();
        impl_->engine.reset();
        impl_->runtime.reset();
        impl_->enginePath.clear();
        impl_->initialized = false;
        impl_->inputWidth = 0;
        impl_->inputHeight = 0;
        impl_->tokenCount = kDefaultTokenCount;
    };

    const auto fail = [&](const std::string& message) {
        std::cerr << "[SAM3] Initialize() FAILED: " << message << std::endl;
        cleanup();
        impl_->availabilityMessage = message;
        return false;
    };

    if (enginePath.empty()) {
        return fail("SAM3 engine path is empty");
    }
    if (impl_->stopRequested.load()) {
        return fail("SAM3 initialization canceled");
    }

    const std::filesystem::path path(enginePath);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return fail("SAM3 engine file does not exist: " + enginePath);
    }
    if (impl_->stopRequested.load()) {
        return fail("SAM3 initialization canceled");
    }

    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return fail("Failed to read SAM3 engine size: " + enginePath);
    }
    if (size == 0) {
        return fail("SAM3 engine file is empty: " + enginePath);
    }
    std::cerr << "[SAM3] Engine file size: " << size << " bytes" << std::endl;

    std::ifstream stream(enginePath, std::ios::binary);
    if (!stream.is_open()) {
        return fail("Failed to open SAM3 engine file: " + enginePath);
    }

    std::vector<char> engineBytes(static_cast<size_t>(size));
    stream.read(engineBytes.data(), static_cast<std::streamsize>(engineBytes.size()));
    const bool readOk = stream.good() || stream.eof();
    stream.close();
    if (!readOk) {
        return fail("Failed to read SAM3 engine file: " + enginePath);
    }
    std::cerr << "[SAM3] Engine file read successfully" << std::endl;

    std::cerr << "[SAM3] Creating TensorRT runtime..." << std::endl;
    impl_->runtime.reset(nvinfer1::createInferRuntime(gSam3Logger));
    if (!impl_->runtime) {
        return fail("Failed to create SAM3 TensorRT runtime");
    }
    std::cerr << "[SAM3] TensorRT runtime created" << std::endl;
    
    if (impl_->stopRequested.load()) {
        return fail("SAM3 initialization canceled");
    }

    std::cerr << "[SAM3] Deserializing CUDA engine (this may take 30-60 seconds)..." << std::endl;
    impl_->engine.reset(impl_->runtime->deserializeCudaEngine(engineBytes.data(), engineBytes.size()));
    if (!impl_->engine) {
        return fail("Failed to deserialize SAM3 engine: " + enginePath);
    }
    std::cerr << "[SAM3] CUDA engine deserialized successfully" << std::endl;

    std::cerr << "[SAM3] Creating execution context..." << std::endl;
    impl_->context.reset(impl_->engine->createExecutionContext());
    if (!impl_->context) {
        return fail("Failed to create SAM3 execution context");
    }
    std::cerr << "[SAM3] Execution context created" << std::endl;
    
    if (impl_->stopRequested.load()) {
        return fail("SAM3 initialization canceled");
    }

    std::string error;
    std::cerr << "[SAM3] Creating CUDA stream..." << std::endl;
    if (!CheckCuda(cudaStreamCreate(&impl_->stream), "cudaStreamCreate", error)) {
        return fail(error);
    }
    std::cerr << "[SAM3] CUDA stream created" << std::endl;

    const auto requireInputTensor = [&](const char* name) -> bool {
        if (!impl_->engine->getTensorShape(name).nbDims) {
            return fail(std::string("Required SAM3 tensor missing: ") + name);
        }
        if (impl_->engine->getTensorIOMode(name) != nvinfer1::TensorIOMode::kINPUT) {
            return fail(std::string("SAM3 tensor has unexpected mode: ") + name);
        }
        return true;
    };

    if (!requireInputTensor(kImageTensorName)
        || !requireInputTensor(kInputIdsTensorName)
        || !requireInputTensor(kAttentionMaskTensorName)) {
        return false;
    }

    const auto requireOutputTensor = [&](const char* name) -> bool {
        if (!impl_->engine->getTensorShape(name).nbDims) {
            return fail(std::string("Required SAM3 tensor missing: ") + name);
        }
        if (impl_->engine->getTensorIOMode(name) != nvinfer1::TensorIOMode::kOUTPUT) {
            return fail(std::string("SAM3 tensor has unexpected mode: ") + name);
        }
        return true;
    };

    if (!requireOutputTensor(kInstanceMasksTensorName)
        || !requireOutputTensor(kSemanticSegTensorName)) {
        return false;
    }

    auto imageDims = impl_->context->getTensorShape(kImageTensorName);
    if (HasDynamicDims(imageDims)) {
        if (!impl_->context->setInputShape(kImageTensorName,
                                           nvinfer1::Dims4{1, 3, kDefaultInputSize, kDefaultInputSize})) {
            return fail("Failed to set SAM3 image input shape");
        }
        imageDims = impl_->context->getTensorShape(kImageTensorName);
    }

    auto inputIdsDims = impl_->context->getTensorShape(kInputIdsTensorName);
    if (HasDynamicDims(inputIdsDims)) {
        if (!impl_->context->setInputShape(kInputIdsTensorName, nvinfer1::Dims2{1, kDefaultTokenCount})) {
            return fail("Failed to set SAM3 input_ids shape");
        }
        inputIdsDims = impl_->context->getTensorShape(kInputIdsTensorName);
    }

    auto attentionDims = impl_->context->getTensorShape(kAttentionMaskTensorName);
    if (HasDynamicDims(attentionDims)) {
        if (!impl_->context->setInputShape(kAttentionMaskTensorName, nvinfer1::Dims2{1, kDefaultTokenCount})) {
            return fail("Failed to set SAM3 attention_mask shape");
        }
        attentionDims = impl_->context->getTensorShape(kAttentionMaskTensorName);
    }

    if (!impl_->context->allInputDimensionsSpecified()) {
        return fail("SAM3 input dimensions are not fully specified");
    }

    if (imageDims.nbDims != 4 || imageDims.d[1] != 3 || imageDims.d[2] <= 0 || imageDims.d[3] <= 0) {
        return fail("SAM3 pixel_values tensor has invalid shape");
    }
    if (inputIdsDims.nbDims != 2 || inputIdsDims.d[0] != 1 || inputIdsDims.d[1] <= 0) {
        return fail("SAM3 input_ids tensor has invalid shape");
    }
    if (attentionDims.nbDims != 2 || attentionDims.d[0] != 1 || attentionDims.d[1] != inputIdsDims.d[1]) {
        return fail("SAM3 attention_mask tensor has invalid shape");
    }

    impl_->inputHeight = static_cast<int>(imageDims.d[2]);
    impl_->inputWidth = static_cast<int>(imageDims.d[3]);
    impl_->tokenCount = static_cast<int>(inputIdsDims.d[1]);

    if (impl_->engine->getTensorDataType(kImageTensorName) != nvinfer1::DataType::kFLOAT) {
        return fail("SAM3 pixel_values tensor must be float32");
    }
    if (impl_->engine->getTensorDataType(kInputIdsTensorName) != nvinfer1::DataType::kINT64
        || impl_->engine->getTensorDataType(kAttentionMaskTensorName) != nvinfer1::DataType::kINT64) {
        return fail("SAM3 prompt tensors must be int64");
    }
    if (impl_->engine->getTensorDataType(kInstanceMasksTensorName) != nvinfer1::DataType::kFLOAT
        && impl_->engine->getTensorDataType(kInstanceMasksTensorName) != nvinfer1::DataType::kHALF) {
        return fail("SAM3 instance_masks tensor must be float or half");
    }
    if (impl_->engine->getTensorDataType(kSemanticSegTensorName) != nvinfer1::DataType::kFLOAT
        && impl_->engine->getTensorDataType(kSemanticSegTensorName) != nvinfer1::DataType::kHALF) {
        return fail("SAM3 semantic_seg tensor must be float or half");
    }

    const int tensorCount = impl_->engine->getNbIOTensors();
    for (int tensorIndex = 0; tensorIndex < tensorCount; ++tensorIndex) {
        if (impl_->stopRequested.load()) {
            return fail("SAM3 initialization canceled");
        }
        const char* tensorName = impl_->engine->getIOTensorName(tensorIndex);
        if (tensorName == nullptr || tensorName[0] == '\0') {
            return fail("SAM3 engine reported an unnamed tensor");
        }

        const std::string name(tensorName);
        TensorBuffer buffer;
        buffer.name = name;
        buffer.type = impl_->engine->getTensorDataType(name.c_str());
        buffer.dims = impl_->context->getTensorShape(name.c_str());
        if (HasDynamicDims(buffer.dims)) {
            return fail("SAM3 tensor still has unresolved dimensions: " + name);
        }

        buffer.elementCount = GetElementCount(buffer.dims);
        const size_t elementSize = GetElementSize(buffer.type);
        if (elementSize == 0 || buffer.elementCount == 0) {
            return fail("SAM3 tensor has unsupported layout: " + name);
        }
        buffer.byteCount = buffer.elementCount * elementSize;

        if (!CheckCuda(cudaMalloc(&buffer.device, buffer.byteCount),
                       ("cudaMalloc " + name).c_str(), error)) {
            return fail(error);
        }
        if (!impl_->context->setTensorAddress(name.c_str(), buffer.device)) {
            return fail("Failed to bind SAM3 tensor address: " + name);
        }

        impl_->buffers.emplace(name, buffer);
    }

    const auto& instanceMasksBuffer = impl_->buffers.at(kInstanceMasksTensorName);
    if (instanceMasksBuffer.dims.nbDims != 4
        || instanceMasksBuffer.dims.d[0] != 1
        || instanceMasksBuffer.dims.d[1] <= 0
        || instanceMasksBuffer.dims.d[2] <= 0
        || instanceMasksBuffer.dims.d[3] <= 0) {
        return fail("SAM3 instance_masks tensor has invalid shape");
    }

    const auto& semanticSegBuffer = impl_->buffers.at(kSemanticSegTensorName);
    if (semanticSegBuffer.dims.nbDims != 4
        || semanticSegBuffer.dims.d[0] != 1
        || semanticSegBuffer.dims.d[1] != 1
        || semanticSegBuffer.dims.d[2] <= 0
        || semanticSegBuffer.dims.d[3] <= 0) {
        return fail("SAM3 semantic_seg tensor has invalid shape");
    }

    impl_->initialized = true;
    impl_->enginePath = enginePath;
    impl_->availabilityMessage = "SAM3 TensorRT ready";
    std::cerr << "[SAM3] Initialize() SUCCESS: " << impl_->availabilityMessage << std::endl;
    return true;
}

void Sam3Labeler::RequestStop() {
    impl_->stopRequested.store(true);
}

void Sam3Labeler::Shutdown() {
    impl_->stopRequested.store(true);
    if (impl_->stream != nullptr) {
        cudaStreamDestroy(impl_->stream);
        impl_->stream = nullptr;
    }

    for (auto& entry : impl_->buffers) {
        if (entry.second.device != nullptr) {
            cudaFree(entry.second.device);
            entry.second.device = nullptr;
        }
    }
    impl_->buffers.clear();
    impl_->context.reset();
    impl_->engine.reset();
    impl_->runtime.reset();
    impl_->enginePath.clear();
    impl_->initialized = false;
    impl_->inputWidth = 0;
    impl_->inputHeight = 0;
    impl_->tokenCount = kDefaultTokenCount;
    impl_->availabilityMessage = kDefaultNotInitializedMessage;
}

Sam3Availability Sam3Labeler::GetAvailability() const {
    return Sam3Availability{impl_->initialized, impl_->availabilityMessage};
}

Sam3LabelResult Sam3Labeler::LabelFrame(const Sam3InferenceRequest& request) {
    Sam3LabelResult result{};
    result.success = false;

    const cv::Mat& frame = request.frame;
    const std::string& prompt = request.prompt;

    if (impl_->stopRequested.load()) {
        result.error = "SAM3 preview canceled";
        return result;
    }
    if (!impl_->initialized) {
        result.error = impl_->availabilityMessage;
        return result;
    }
    if (frame.empty()) {
        result.error = "SAM3 preview received an empty frame";
        return result;
    }
    if (prompt.empty()) {
        result.error = "SAM3 preview prompt is empty";
        return result;
    }

    cv::Mat bgr = ToBgrFrame(frame);
    if (bgr.empty()) {
        result.error = "SAM3 preview frame must be 1, 3, or 4 channels";
        return result;
    }

    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(impl_->inputWidth, impl_->inputHeight), 0.0, 0.0, cv::INTER_LINEAR);
    if (impl_->stopRequested.load()) {
        result.error = "SAM3 preview canceled";
        return result;
    }

    std::vector<float> imageTensor(static_cast<size_t>(impl_->inputWidth) * impl_->inputHeight * 3U);
    const size_t planeSize = static_cast<size_t>(impl_->inputWidth) * impl_->inputHeight;
    for (int y = 0; y < resized.rows; ++y) {
        const cv::Vec3b* row = resized.ptr<cv::Vec3b>(y);
        for (int x = 0; x < resized.cols; ++x) {
            const cv::Vec3b pixel = row[x];
            const size_t index = static_cast<size_t>(y) * resized.cols + x;
            imageTensor[index] = (2.0f * static_cast<float>(pixel[2]) / 255.0f) - 1.0f;
            imageTensor[index + planeSize] = (2.0f * static_cast<float>(pixel[1]) / 255.0f) - 1.0f;
            imageTensor[index + planeSize * 2U] = (2.0f * static_cast<float>(pixel[0]) / 255.0f) - 1.0f;
        }
    }

    std::vector<int64_t> inputIds;
    std::vector<int64_t> attentionMask;
    detail::EncodeSam3Prompt(prompt, impl_->tokenCount, inputIds, attentionMask);

    std::string error;
    const auto copyInput = [&](const char* name, const void* source, size_t bytes) -> bool {
        const auto it = impl_->buffers.find(name);
        if (it == impl_->buffers.end()) {
            error = std::string("Required SAM3 buffer missing: ") + name;
            return false;
        }
        if (bytes != it->second.byteCount) {
            error = std::string("SAM3 buffer size mismatch for tensor: ") + name;
            return false;
        }
        return CheckCuda(cudaMemcpyAsync(it->second.device, source, bytes,
                                         cudaMemcpyHostToDevice, impl_->stream),
                         (std::string("cudaMemcpyAsync ") + name).c_str(), error);
    };

    if (!copyInput(kImageTensorName, imageTensor.data(), imageTensor.size() * sizeof(float))
        || !copyInput(kInputIdsTensorName, inputIds.data(), inputIds.size() * sizeof(int64_t))
        || !copyInput(kAttentionMaskTensorName, attentionMask.data(), attentionMask.size() * sizeof(int64_t))) {
        result.error = error;
        return result;
    }
    if (impl_->stopRequested.load()) {
        result.error = "SAM3 preview canceled";
        return result;
    }

    if (!impl_->context->enqueueV3(impl_->stream)) {
        result.error = "SAM3 preview inference enqueue failed";
        return result;
    }

    while (true) {
        const cudaError_t query = cudaStreamQuery(impl_->stream);
        if (query == cudaSuccess) {
            break;
        }
        if (query != cudaErrorNotReady) {
            result.error = std::string("cudaStreamQuery: ") + cudaGetErrorString(query);
            return result;
        }
        if (impl_->stopRequested.load()) {
            result.error = "SAM3 preview canceled";
            return result;
        }
        Sleep(1);
    }
    if (impl_->stopRequested.load()) {
        result.error = "SAM3 preview canceled";
        return result;
    }

    const auto semanticSeg = ReadFloatTensor(impl_->buffers.at(kSemanticSegTensorName), impl_->stream, error);
    if (!error.empty()) {
        result.error = error;
        return result;
    }

    const auto instanceMasks = ReadFloatTensor(impl_->buffers.at(kInstanceMasksTensorName), impl_->stream, error);
    if (!error.empty()) {
        result.error = error;
        return result;
    }

    (void)semanticSeg;
    result = DecodeBoxesFromInstanceMasks(
        instanceMasks,
        impl_->buffers.at(kInstanceMasksTensorName),
        frame.cols,
        frame.rows,
        request.postprocess);

    return result;
}

Sam3Availability Sam3Labeler::GetAvailabilityForTests() const {
    return GetAvailability();
}

}  // namespace training

#else

#error "training_sam3_labeler_trt.cpp should only be compiled with USE_CUDA defined"

#endif  // USE_CUDA
