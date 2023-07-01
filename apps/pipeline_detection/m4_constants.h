#ifndef M4_CONSTANTS_H
#define M4_CONSTANTS_H

#include "libs/camera/camera.h"

namespace M4Constant {
    using namespace coralmicro;

    constexpr CameraMode kMode = CameraMode::kStreaming;
    constexpr CameraFormat kFormat = CameraFormat::kRgb;
    constexpr CameraFilterMethod kFilter = CameraFilterMethod::kBilinear;
    constexpr CameraRotation kRotation = CameraRotation::k0;
    constexpr size_t kWidth = CameraTask::kWidth;
    constexpr size_t kHeight = CameraTask::kHeight;
    constexpr int kJpegQuality = 75;
    constexpr int kMaxCameraWaitMs = 400;
}

#endif