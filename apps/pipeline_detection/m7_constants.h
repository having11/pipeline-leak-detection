#ifndef M7_CONSTANTS_H
#define M7_CONSTANTS_H

#include <unordered_set>

#include "libs/camera/camera.h"
#include "libs/tensorflow/audio_models.h"

namespace M7Constant {
    using namespace coralmicro;

    constexpr CameraMode kMode = CameraMode::kStreaming;
    constexpr CameraFormat kFormat = CameraFormat::kRgb;
    constexpr CameraFilterMethod kFilter = CameraFilterMethod::kBilinear;
    constexpr CameraRotation kRotation = CameraRotation::k0;
    constexpr size_t kWidth = CameraTask::kWidth;
    constexpr size_t kHeight = CameraTask::kHeight;
    constexpr int kJpegQuality = 75;
    constexpr bool kPreserveRatio = false;
    constexpr bool kWhiteBalance = true;
    constexpr int kMaxCameraWaitMs = 400;

    constexpr int kTensorArenaSize = 1 * 975 * 1024;
    constexpr int kNumDmaBuffers = 2;
    constexpr int kDmaBufferSizeMs = 50;
    constexpr int kDmaBufferSize =
        kNumDmaBuffers * tensorflow::kYamnetSampleRateMs * kDmaBufferSizeMs;
    constexpr int kAudioServicePriority = 4;
    constexpr int kDropFirstSamplesMs = 150;

    constexpr int kAudioBufferSizeMs = tensorflow::kYamnetDurationMs;
    constexpr int kAudioBufferSize =
        kAudioBufferSizeMs * tensorflow::kYamnetSampleRateMs;

    constexpr float kThreshold = 0.3;
    constexpr int kTopK = 5;

    constexpr char kModelName[] = "/yamnet_spectra_in_edgetpu.tflite";
    constexpr bool kUseTpu = true;

    constexpr char kCameraStreamUrlPrefix[] = "/camera_stream";

    const std::unordered_set<uint16_t> kDesiredSoundClasses(
        { 438, 439, 440, 441, 442, 443, 444, 445, 447, 483, }
    );
    constexpr uint32_t kM4InferencingTimeMs = 10000;
}

#endif