#include <cstdio>
#include <vector>

#include "libs/base/http_server.h"
#include "libs/base/led.h"
#include "libs/base/strings.h"
#include "libs/base/utils.h"
#include "libs/camera/camera.h"
#include "libs/libjpeg/jpeg.h"
#include "libs/base/ipc_m7.h"
#include "libs/tensorflow/audio_models.h"
#include "libs/tensorflow/utils.h"
#include "libs/tpu/edgetpu_manager.h"
#include "libs/tpu/edgetpu_op.h"
#include "libs/audio/audio_service.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"
#include "third_party/tflite-micro/tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_interpreter.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "libs/base/wifi.h"

#include "ipc_message.h"
#include "m7_constants.h"

namespace coralmicro {
namespace {

static IpcM7 *ipc;
SemaphoreHandle_t xCameraSema;
StaticSemaphore_t xCameraSemaBuffer;

STATIC_TENSOR_ARENA_IN_SDRAM(tensor_arena, M7Constant::kTensorArenaSize);
static AudioDriverBuffers<M7Constant::kNumDmaBuffers, M7Constant::kDmaBufferSize> audio_buffers;
static AudioDriver audio_driver(audio_buffers);
static std::array<int16_t, tensorflow::kYamnetAudioSize> audio_input;
static std::unique_ptr<tflite::MicroInterpreter> interpreter;
static std::unique_ptr<FrontendState> frontend_state;
static std::unique_ptr<LatestSamples> audio_latest;

constexpr char kIndexFileName[] = "/coral_micro_camera.html";
constexpr char kCameraStreamUrlPrefix[] = "/camera_stream";

HttpServer::Content uriHandler(const char* uri) {
    if (StrEndsWith(uri, "index.html") ||
        StrEndsWith(uri, "coral_micro_camera.html")) {
        return std::string(kIndexFileName);
    } else if (StrEndsWith(uri, kCameraStreamUrlPrefix)) {
        // [start-snippet:jpeg]
        std::vector<uint8_t> buf(CameraTask::kWidth * CameraTask::kHeight *
                                CameraFormatBpp(CameraFormat::kRgb));
        auto fmt = CameraFrameFormat{
            CameraFormat::kRgb,       CameraFilterMethod::kBilinear,
            CameraRotation::k0,       CameraTask::kWidth,
            CameraTask::kHeight,
            /*preserve_ratio=*/false, buf.data(),
            /*while_balance=*/true};
        if (!CameraTask::GetSingleton()->GetFrame({fmt})) {
        printf("Unable to get frame from camera\r\n");
        return {};
        }

        std::vector<uint8_t> jpeg;
        JpegCompressRgb(buf.data(), fmt.width, fmt.height, /*quality=*/75, &jpeg);
        // [end-snippet:jpeg]
        return jpeg;
    }
    return {};
}

void handleM4Message(const uint8_t data[kIpcMessageBufferDataSize]) {
    const auto* msg = reinterpret_cast<const msg::Message*>(data);

    printf("[M7] Received message type=%d\r\n", (uint8_t)msg->type);

    if (msg->type == msg::MessageType::kKeywordSpotted) {
        printf("[M7] KWS value=%d\r\n", msg->data.audioFound.found);

        // Check if sound has been detected by M4

        // Start inferencing on images
    }

    else if (msg->type == msg::MessageType::kAck) {
        printf("[M7] M4 ACK message\r\n");
    }
}

void Main() {
    printf("Camera HTTP Example!\r\n");
    // Turn on Status LED to show the board is on.
    LedSet(Led::kStatus, true);

    CameraTask::GetSingleton()->SetPower(true);
    CameraTask::GetSingleton()->Enable(CameraMode::kStreaming);

    // // Init M4 core
    // ipc = IpcM7::GetSingleton();
    // ipc->RegisterAppMessageHandler(handleM4Message);
    // ipc->StartM4();
    // CHECK(ipc->M4IsAlive(500u));

    if (!WiFiTurnOn(/*default_iface=*/false)) {
        printf("Unable to bring up WiFi...\r\n");
        vTaskSuspend(nullptr);
    }
    if (!WiFiConnect(10)) {
        printf("Unable to connect to WiFi...\r\n");
        vTaskSuspend(nullptr);
    }
    if (auto wifi_ip = WiFiGetIp()) {
        printf("Serving on: %s\r\n", wifi_ip->c_str());
    } else {
        printf("Failed to get Wifi Ip\r\n");
        vTaskSuspend(nullptr);
    }

    HttpServer http_server;
    http_server.AddUriHandler(uriHandler);
    UseHttpServer(&http_server);

    vTaskSuspend(nullptr);
}
}  // namespace
}  // namespace coralmicro

extern "C" void app_main(void* param) {
    (void)param;
    coralmicro::Main();
}
