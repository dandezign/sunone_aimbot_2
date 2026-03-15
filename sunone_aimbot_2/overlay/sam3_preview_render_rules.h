#pragma once

inline bool ShouldDrawSam3PreviewBoxes(bool drawPreviewBoxes) {
    return drawPreviewBoxes;
}

inline bool ShouldDrawSam3PreviewConfidenceLabels(bool drawPreviewBoxes, bool drawConfidenceLabels) {
    return drawPreviewBoxes && drawConfidenceLabels;
}
