#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <d3d11.h>
#include <algorithm>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include <thread>

#include "imgui/imgui.h"
#include "sunone_aimbot_2.h"
#include "overlay.h"
#include "overlay/config_dirty.h"
#include "include/other_tools.h"
#include "capture.h"
#include "sunone_aimbot_2/debug/detection_debug_export.h"
#include "sunone_aimbot_2/training/training_sam3_runtime.h"
#include "sunone_aimbot_2/training/training_inference_mode.h"
#include "sunone_aimbot_2/overlay/sam3_preview_render_rules.h"
#include "overlay/ui_sections.h"

#ifdef USE_CUDA
#include "depth/depth_mask.h"
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)       \
    do {                      \
        if ((p) != nullptr) { \
            (p)->Release();   \
            (p) = nullptr;    \
        }                     \
    } while (0)
#endif

int prev_screenshot_delay = config.screenshot_delay;
bool prev_verbose = config.verbose;

static ID3D11Texture2D* g_debugTex = nullptr;
static ID3D11ShaderResourceView* g_debugSRV = nullptr;
static int texW = 0, texH = 0;

static ID3D11Texture2D* g_maskTex = nullptr;
static ID3D11ShaderResourceView* g_maskSRV = nullptr;
static int maskTexW = 0, maskTexH = 0;

static void clearDebugFrameTexture();

static void clearDebugFrameTexture()
{
    SAFE_RELEASE(g_debugTex);
    SAFE_RELEASE(g_debugSRV);
    texW = 0;
    texH = 0;
}

static void clearDebugMaskTexture()
{
    SAFE_RELEASE(g_maskTex);
    SAFE_RELEASE(g_maskSRV);
    maskTexW = 0;
    maskTexH = 0;
}

void CleanupDebugDrawResources()
{
    clearDebugFrameTexture();
    clearDebugMaskTexture();
}

static float debug_scale = 0.5f;
static int g_timedCaptureIntervalMs = 250;
static int g_timedCaptureBurstCount = 5;
static char g_timedCapturePrefix[64] = "";
static std::mutex g_debugExportStateMutex;
static std::string g_lastDebugStatus = "No debug export yet.";
static std::string g_lastDebugPath;
static std::string g_lastDebugBundleId;
static std::string g_lastDebugBackend;
static int g_lastDebugDetectionCount = 0;

static std::string joinShape(const std::vector<int64_t>& shape)
{
    if (shape.empty())
        return "[]";

    std::string text = "[";
    for (size_t i = 0; i < shape.size(); ++i)
    {
        if (i > 0)
            text += ", ";
        text += std::to_string(shape[i]);
    }
    text += "]";
    return text;
}

static std::string formatOptionalInt(const std::optional<int>& value)
{
    return value.has_value() ? std::to_string(*value) : std::string("n/a");
}

static std::string formatOptionalFloat(const std::optional<float>& value)
{
    if (!value.has_value())
        return "n/a";

    char buffer[32] = {};
    snprintf(buffer, sizeof(buffer), "%.4f", *value);
    return buffer;
}

static void runDebugExport(detection_debug::ExportKind kind)
{
    std::thread([kind]() {
        const auto snapshot = detection_debug::CaptureBundleSnapshot("[ManualExport] requested export");
        std::string outPath;
        std::string outStatus;
        const bool success = detection_debug::QueueBundleExport(snapshot, &outPath, &outStatus, kind);

        std::string lastDebugStatus = outStatus.empty()
            ? (success ? "Debug export complete" : "Debug export failed")
            : outStatus;
        {
            std::lock_guard<std::mutex> lock(g_debugExportStateMutex);
            g_lastDebugStatus = lastDebugStatus;
            g_lastDebugPath = outPath;
            g_lastDebugBundleId = snapshot.bundleId;
            g_lastDebugBackend = snapshot.backend;
            g_lastDebugDetectionCount = static_cast<int>(snapshot.boxes.size());
        }

        detection_debug::AppendEvent(
            success
                ? std::string("[ManualExport] ") + lastDebugStatus + " -> " + outPath
                : std::string("[ManualExport] failed: ") + lastDebugStatus);
    }).detach();
}

static int findDebugKeyIndexByName(const std::string& keyName)
{
    for (size_t k = 0; k < key_names.size(); ++k)
    {
        if (key_names[k] == keyName)
            return static_cast<int>(k);
    }
    return 0;
}

static bool drawScreenshotButtonRows()
{
    if (key_names_cstrs.empty())
    {
        ImGui::TextDisabled("No key list available.");
        return false;
    }

    bool changed = false;
    if (config.screenshot_button.empty())
    {
        config.screenshot_button.push_back("None");
        changed = true;
    }

    for (size_t i = 0; i < config.screenshot_button.size();)
    {
        std::string& currentKeyName = config.screenshot_button[i];
        int currentIndex = findDebugKeyIndexByName(currentKeyName);

        ImGui::PushID(static_cast<int>(i));

        const float rowAvail = ImGui::GetContentRegionAvail().x;
        const float actionBtnW = ImGui::GetFrameHeight();
        float comboWidth = rowAvail - (actionBtnW * 2.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f);
        const float comboMin = rowAvail * 0.56f;
        if (comboWidth < comboMin)
            comboWidth = comboMin;
        if (comboWidth < 1.0f)
            comboWidth = 1.0f;
        ImGui::SetNextItemWidth(comboWidth);

        if (ImGui::Combo("##screenshot_binding_combo", &currentIndex, key_names_cstrs.data(), static_cast<int>(key_names_cstrs.size())))
        {
            currentKeyName = key_names[currentIndex];
            changed = true;
        }

        ImGui::SameLine(0.0f, 4.0f);
        if (ImGui::Button("+", ImVec2(actionBtnW, 0.0f)))
        {
            config.screenshot_button.insert(config.screenshot_button.begin() + static_cast<std::vector<std::string>::difference_type>(i + 1), "None");
            changed = true;
        }

        ImGui::SameLine(0.0f, 3.0f);
        bool removedCurrent = false;
        if (ImGui::Button("-", ImVec2(actionBtnW, 0.0f)))
        {
            if (config.screenshot_button.size() <= 1)
            {
                config.screenshot_button[0] = "None";
            }
            else
            {
                config.screenshot_button.erase(config.screenshot_button.begin() + static_cast<std::vector<std::string>::difference_type>(i));
                removedCurrent = true;
            }
            changed = true;
        }

        ImGui::PopID();

        if (removedCurrent)
            continue;

        ++i;
    }

    return changed;
}

static void uploadDebugFrame(const cv::Mat& bgr)
{
    if (bgr.empty()) return;

    if (!g_debugTex || bgr.cols != texW || bgr.rows != texH)
    {
        SAFE_RELEASE(g_debugTex);
        SAFE_RELEASE(g_debugSRV);

        texW = bgr.cols;  texH = bgr.rows;

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = texW;
        td.Height = texH;
        td.MipLevels = td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DYNAMIC;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        g_pd3dDevice->CreateTexture2D(&td, nullptr, &g_debugTex);

        D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MipLevels = 1;
        g_pd3dDevice->CreateShaderResourceView(g_debugTex, &sd, &g_debugSRV);
    }

    static cv::Mat rgba;
    cv::cvtColor(bgr, rgba, cv::COLOR_BGR2RGBA);

    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(g_pd3dDeviceContext->Map(g_debugTex, 0,
        D3D11_MAP_WRITE_DISCARD, 0, &ms)))
    {
        for (int y = 0; y < texH; ++y)
            memcpy((uint8_t*)ms.pData + ms.RowPitch * y,
                rgba.ptr(y), texW * 4);
        g_pd3dDeviceContext->Unmap(g_debugTex, 0);
    }
}

static void uploadMaskFrame(const cv::Mat& rgba)
{
    if (rgba.empty()) return;

    if (!g_maskTex || rgba.cols != maskTexW || rgba.rows != maskTexH)
    {
        SAFE_RELEASE(g_maskTex);
        SAFE_RELEASE(g_maskSRV);

        maskTexW = rgba.cols;
        maskTexH = rgba.rows;

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = maskTexW;
        td.Height = maskTexH;
        td.MipLevels = td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DYNAMIC;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        g_pd3dDevice->CreateTexture2D(&td, nullptr, &g_maskTex);

        D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MipLevels = 1;
        g_pd3dDevice->CreateShaderResourceView(g_maskTex, &sd, &g_maskSRV);
    }

    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(g_pd3dDeviceContext->Map(g_maskTex, 0,
        D3D11_MAP_WRITE_DISCARD, 0, &ms)))
    {
        for (int y = 0; y < maskTexH; ++y)
            memcpy((uint8_t*)ms.pData + ms.RowPitch * y,
                rgba.ptr(y), maskTexW * 4);
        g_pd3dDeviceContext->Unmap(g_maskTex, 0);
    }
}

void draw_debug_frame()
{
    const auto inferenceMode = activeInferenceMode.load();
    const auto sam3Snapshot = training::GetTrainingLatestPreviewOverlaySnapshot();
    const auto sam3Status = training::GetTrainingRuntimeStatus();
    const bool labelModeActive = inferenceMode == training::InferenceMode::Label;
    const bool useSam3LabelPreview =
        labelModeActive &&
        sam3Status.backend.ready &&
        sam3Snapshot.valid;

    cv::Mat frameCopy;
    if (useSam3LabelPreview) {
        frameCopy = sam3Snapshot.frame;
    } else if (!labelModeActive) {
        std::lock_guard<std::mutex> lk(frameMutex);
        if (!latestFrame.empty())
            latestFrame.copyTo(frameCopy);
    }

    if (frameCopy.empty()) {
        clearDebugFrameTexture();
    } else {
        uploadDebugFrame(frameCopy);
    }

    if (!g_debugSRV) return;

    if (labelModeActive && !useSam3LabelPreview) {
        ImGui::TextDisabled("Waiting for SAM3 preview snapshot...");
        return;
    }

    ImGui::SliderFloat("Debug scale", &debug_scale, 0.1f, 2.0f, "%.1fx");

    ImVec2 image_size(texW * debug_scale, texH * debug_scale);
    ImGui::Image(g_debugSRV, image_size);

    ImVec2 image_pos = ImGui::GetItemRectMin();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 clip_min = image_pos;
    ImVec2 clip_max(image_pos.x + image_size.x, image_pos.y + image_size.y);

#ifdef USE_CUDA
    if (!labelModeActive && config.depth_mask_enabled)
    {
        auto& depthMask = depth_anything::GetDepthMaskGenerator();
        cv::Mat mask = depthMask.getMask();
        if (!mask.empty() && mask.size() == frameCopy.size())
        {
            cv::Mat alpha(mask.size(), CV_8U, cv::Scalar(0));
            alpha.setTo(config.depth_mask_alpha, mask);

            std::vector<cv::Mat> channels(4);
            channels[0] = cv::Mat(mask.size(), CV_8U, cv::Scalar(255));
            channels[1] = cv::Mat(mask.size(), CV_8U, cv::Scalar(0));
            channels[2] = cv::Mat(mask.size(), CV_8U, cv::Scalar(0));
            channels[3] = alpha;

            cv::Mat rgba;
            cv::merge(channels, rgba);
            uploadMaskFrame(rgba);

            if (g_maskSRV)
            {
                ImVec2 overlay_max(image_pos.x + image_size.x, image_pos.y + image_size.y);
                draw_list->AddImage(g_maskSRV, image_pos, overlay_max);
            }
        }
    }
#endif

    if (!labelModeActive) {
        std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
        for (size_t i = 0; i < detectionBuffer.boxes.size(); ++i)
        {
            const cv::Rect& box = detectionBuffer.boxes[i];

            ImVec2 p1(image_pos.x + box.x * debug_scale,
                image_pos.y + box.y * debug_scale);
            ImVec2 p2(p1.x + box.width * debug_scale,
                p1.y + box.height * debug_scale);

            ImU32 color = IM_COL32(255, 0, 0, 255);

            draw_list->AddRect(p1, p2, color, 0.0f, 0, 2.0f);

            if (i < detectionBuffer.classes.size())
            {
                std::string label = std::to_string(detectionBuffer.classes[i]);
                draw_list->AddText(ImVec2(p1.x, p1.y - 16), IM_COL32(255, 255, 0, 255), label.c_str());
            }
        }
    }

        if (useSam3LabelPreview) {
            draw_list->PushClipRect(clip_min, clip_max, true);
            const float scale_x = image_size.x / static_cast<float>(std::max(1, sam3Snapshot.frame.cols));
            const float scale_y = image_size.y / static_cast<float>(std::max(1, sam3Snapshot.frame.rows));
            const bool drawSam3Boxes = ShouldDrawSam3PreviewBoxes(config.training_sam3_draw_preview_boxes);
            const bool drawSam3Labels = ShouldDrawSam3PreviewConfidenceLabels(
                config.training_sam3_draw_preview_boxes,
                config.training_sam3_draw_confidence_labels);

            for (const auto& detection : sam3Snapshot.result.boxes) {
                const cv::Rect& box = detection.rect;
                ImVec2 p1(image_pos.x + box.x * scale_x, image_pos.y + box.y * scale_y);
                ImVec2 p2(image_pos.x + (box.x + box.width) * scale_x,
                          image_pos.y + (box.y + box.height) * scale_y);

            if (drawSam3Boxes) {
                draw_list->AddRect(p1, p2, IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);
            }

            if (drawSam3Labels) {
                char label[16] = {};
                snprintf(label, sizeof(label), "%.2f", detection.confidence);
                const ImVec2 text_pos(p1.x, std::max(image_pos.y, p1.y - 16.0f));
                draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), label);
            }
        }
        draw_list->PopClipRect();
    }

    if (!labelModeActive && config.draw_futurePositions && globalMouseThread)
    {
        auto futurePts = globalMouseThread->getFuturePositions();
        if (!futurePts.empty())
        {
            float scale_x = static_cast<float>(texW) / config.detection_resolution;
            float scale_y = static_cast<float>(texH) / config.detection_resolution;

            draw_list->PushClipRect(clip_min, clip_max, true);

            int totalPts = static_cast<int>(futurePts.size());
            for (size_t i = 0; i < futurePts.size(); ++i)
            {
                int px = static_cast<int>(futurePts[i].first * scale_x);
                int py = static_cast<int>(futurePts[i].second * scale_y);
                ImVec2 pt(image_pos.x + px * debug_scale,
                    image_pos.y + py * debug_scale);

                int b = static_cast<int>(255 - (i * 255.0 / totalPts));
                int r = static_cast<int>(i * 255.0 / totalPts);
                int g = 50;

                ImU32 fillColor = IM_COL32(r, g, b, 255);
                ImU32 outlineColor = IM_COL32(255, 255, 255, 255);

                draw_list->AddCircleFilled(pt, 4.0f * debug_scale, fillColor);
                draw_list->AddCircle(pt, 4.0f * debug_scale, outlineColor, 0, 1.0f);
            }

            draw_list->PopClipRect();
        }
    }
}

void draw_capture_preview()
{
    if (OverlayUI::BeginSection("Capture Preview", "capture_section_preview"))
    {
        if (ImGui::Checkbox("Show Preview Window", &config.show_window))
        {
            OverlayConfig_MarkDirty();
        }

        if (config.show_window)
        {
            draw_debug_frame();
        }

        OverlayUI::EndSection();
    }
}

void draw_debug()
{
    if (OverlayUI::BeginSection("Screenshot Buttons", "debug_section_screenshot_buttons"))
    {
        if (drawScreenshotButtonRows())
            OverlayConfig_MarkDirty();

        ImGui::InputInt("Screenshot delay", &config.screenshot_delay, 50, 500);
        ImGui::Checkbox("Verbose console output", &config.verbose);

        if (ImGui::Button("Print OpenCV build information##button_cv2_build_info"))
        {
            std::cout << cv::getBuildInformation() << std::endl;
        }

        OverlayUI::EndSection();
    }

    if (prev_screenshot_delay != config.screenshot_delay ||
        prev_verbose != config.verbose)
    {
        prev_screenshot_delay = config.screenshot_delay;
        prev_verbose = config.verbose;
        OverlayConfig_MarkDirty();
    }

    if (OverlayUI::BeginSection("Detection Diagnostics", "debug_section_detection_diagnostics"))
    {
        int liveFrameW = 0;
        int liveFrameH = 0;
        int liveDetectionCount = 0;
        int minBoxX = std::numeric_limits<int>::max();
        int minBoxY = std::numeric_limits<int>::max();
        int maxBoxX = std::numeric_limits<int>::min();
        int maxBoxY = std::numeric_limits<int>::min();

        {
            std::lock_guard<std::mutex> lk(frameMutex);
            if (!latestFrame.empty())
            {
                liveFrameW = latestFrame.cols;
                liveFrameH = latestFrame.rows;
            }
        }

        {
            std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
            liveDetectionCount = static_cast<int>(detectionBuffer.boxes.size());
            for (const auto& box : detectionBuffer.boxes)
            {
                minBoxX = std::min(minBoxX, box.x);
                minBoxY = std::min(minBoxY, box.y);
                maxBoxX = std::max(maxBoxX, box.x + box.width);
                maxBoxY = std::max(maxBoxY, box.y + box.height);
            }
        }

        const auto sharedSnapshot = detection_debug::GetSharedStateSnapshot();
        const auto timedCaptureState = detection_debug::GetTimedCaptureState();

        if (ImGui::Button("Save Raw Frame"))
            runDebugExport(detection_debug::ExportKind::RawFrame);
        ImGui::SameLine();
        if (ImGui::Button("Save Annotated Frame"))
            runDebugExport(detection_debug::ExportKind::AnnotatedFrame);
        ImGui::SameLine();
        if (ImGui::Button("Save Debug Bundle"))
            runDebugExport(detection_debug::ExportKind::Bundle);
        ImGui::SameLine();
        if (ImGui::Button("Export Debug Log"))
            runDebugExport(detection_debug::ExportKind::LogOnly);

        ImGui::Separator();
        ImGui::Text("Timed Capture");
        ImGui::InputInt("Interval (ms)", &g_timedCaptureIntervalMs, 50, 500);
        ImGui::InputInt("Burst count", &g_timedCaptureBurstCount, 1, 10);
        ImGui::InputText("Prefix", g_timedCapturePrefix, IM_ARRAYSIZE(g_timedCapturePrefix));
        g_timedCaptureIntervalMs = std::max(1, g_timedCaptureIntervalMs);
        g_timedCaptureBurstCount = std::max(1, g_timedCaptureBurstCount);

        if (ImGui::Button("Start Timed Capture"))
        {
            detection_debug::TimedCaptureRequest request;
            request.intervalMs = g_timedCaptureIntervalMs;
            request.remainingShots = g_timedCaptureBurstCount;
            request.prefix = g_timedCapturePrefix;
            detection_debug::StartTimedCapture(request);
            detection_debug::AppendEvent(
                std::string("[TimedCapture] started interval=") + std::to_string(request.intervalMs) +
                "ms burst=" + std::to_string(request.remainingShots));
            std::lock_guard<std::mutex> lock(g_debugExportStateMutex);
            g_lastDebugStatus = "Timed capture started";
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop Timed Capture"))
        {
            detection_debug::StopTimedCapture();
            detection_debug::SetTimedCaptureStatus("Timed capture stopped");
            detection_debug::AppendEvent("[TimedCapture] stopped");
            std::lock_guard<std::mutex> lock(g_debugExportStateMutex);
            g_lastDebugStatus = "Timed capture stopped";
        }

        ImGui::Text(
            "State: %s | Remaining: %d | Completed: %d",
            timedCaptureState.active ? "running" : "idle",
            timedCaptureState.remainingShots,
            timedCaptureState.completedShots);
        ImGui::TextWrapped("Timed status: %s", timedCaptureState.lastStatus.empty() ? "n/a" : timedCaptureState.lastStatus.c_str());
        ImGui::TextWrapped("Timed path: %s", timedCaptureState.lastPath.empty() ? "n/a" : timedCaptureState.lastPath.c_str());

        ImGui::Separator();
        ImGui::Text("Live");
        ImGui::Text("Backend: %s", sharedSnapshot.detector.backend.empty() ? config.backend.c_str() : sharedSnapshot.detector.backend.c_str());
        ImGui::TextWrapped("Model: %s", sharedSnapshot.detector.modelPath.empty() ? config.ai_model.c_str() : sharedSnapshot.detector.modelPath.c_str());
        if (liveFrameW > 0 && liveFrameH > 0)
            ImGui::Text("Frame: %dx%d", liveFrameW, liveFrameH);
        else
            ImGui::TextDisabled("Frame: n/a");
        ImGui::Text("Detection resolution: %d", config.detection_resolution);
        ImGui::Text(
            "Detector input: %s x %s",
            formatOptionalInt(sharedSnapshot.detector.detectorInputWidth).c_str(),
            formatOptionalInt(sharedSnapshot.detector.detectorInputHeight).c_str());
        ImGui::Text(
            "Model size: %s x %s",
            formatOptionalInt(sharedSnapshot.detector.modelWidth).c_str(),
            formatOptionalInt(sharedSnapshot.detector.modelHeight).c_str());
        ImGui::TextWrapped("Output shape: %s", joinShape(sharedSnapshot.detector.outputShape).c_str());
        ImGui::Text(
            "Gains: x=%s y=%s",
            formatOptionalFloat(sharedSnapshot.detector.xGain).c_str(),
            formatOptionalFloat(sharedSnapshot.detector.yGain).c_str());
        ImGui::TextWrapped(
            "Box convention: %s",
            sharedSnapshot.detector.boxConvention.empty() ? "n/a" : sharedSnapshot.detector.boxConvention.c_str());
        ImGui::Text(
            "Detections: live=%d detector=%d",
            liveDetectionCount,
            static_cast<int>(sharedSnapshot.detector.detections.size()));
        if (liveDetectionCount > 0)
            ImGui::Text("Box extents: min(%d,%d) max(%d,%d)", minBoxX, minBoxY, maxBoxX, maxBoxY);
        else
            ImGui::TextDisabled("Box extents: n/a");

        ImGui::Separator();
        ImGui::Text("Last Saved");
        std::string lastDebugStatus;
        std::string lastDebugPath;
        std::string lastDebugBundleId;
        std::string lastDebugBackend;
        int lastDebugDetectionCount = 0;
        {
            std::lock_guard<std::mutex> lock(g_debugExportStateMutex);
            lastDebugStatus = g_lastDebugStatus;
            lastDebugPath = g_lastDebugPath;
            lastDebugBundleId = g_lastDebugBundleId;
            lastDebugBackend = g_lastDebugBackend;
            lastDebugDetectionCount = g_lastDebugDetectionCount;
        }
        ImGui::TextWrapped("Status: %s", lastDebugStatus.c_str());
        ImGui::TextWrapped("Path: %s", lastDebugPath.empty() ? "n/a" : lastDebugPath.c_str());
        ImGui::TextWrapped("Bundle ID: %s", lastDebugBundleId.empty() ? "n/a" : lastDebugBundleId.c_str());
        ImGui::TextWrapped("Backend: %s", lastDebugBackend.empty() ? "n/a" : lastDebugBackend.c_str());
        ImGui::Text("Detection count: %d", lastDebugDetectionCount);

        OverlayUI::EndSection();
    }
}
