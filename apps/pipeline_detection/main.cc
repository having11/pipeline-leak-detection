#include <cstdio>
#include <vector>

#include "libs/audio/audio_service.h"
#include "libs/base/filesystem.h"
#include "libs/base/http_server.h"
#include "libs/rpc/rpc_http_server.h"
#include "libs/base/ipc_m7.h"
#include "libs/base/led.h"
#include "libs/base/strings.h"
#include "libs/base/timer.h"
#include "libs/base/mutex.h"
#include "libs/base/utils.h"
#include "libs/base/wifi.h"
#include "libs/camera/camera.h"
#include "libs/libjpeg/jpeg.h"
#include "libs/tensorflow/audio_models.h"
#include "libs/tensorflow/utils.h"
#include "libs/tpu/edgetpu_manager.h"
#include "libs/tpu/edgetpu_op.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"
#include "third_party/tflite-micro/tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_interpreter.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "third_party/freertos_kernel/include/semphr.h"
#include "third_party/mjson/src/mjson.h"

#include "ipc_message.h"
#include "m7_constants.h"

namespace coralmicro {
namespace {

static IpcM7 *ipc;
static volatile bool isDetecting = false;
static uint64_t lastMillis;
SemaphoreHandle_t mutex_;

STATIC_TENSOR_ARENA_IN_SDRAM(tensor_arena, M7Constant::kTensorArenaSize);
static AudioDriverBuffers<M7Constant::kNumDmaBuffers, M7Constant::kDmaBufferSize> audio_buffers;
static AudioDriver audio_driver(audio_buffers);
static std::array<int16_t, tensorflow::kYamnetAudioSize> audio_input;

HttpServer::Content uriHandler(const char* uri) {
    if (StrEndsWith(uri, "index.html"))
        return std::string(M7Constant::kIndexFileName);
    else if (StrEndsWith(uri, "is_detecting")) {
        return isDetecting ? std::string("true") : std::string("false");
    } else if (StrEndsWith(uri, M7Constant::kCameraStreamUrlPrefix)) {
        std::vector<uint8_t> buf(M7Constant::kWidth * M7Constant::kHeight *
                                CameraFormatBpp(M7Constant::kFormat));
        auto fmt = CameraFrameFormat{
            M7Constant::kFormat,       M7Constant::kFilter,
            M7Constant::kRotation,     M7Constant::kWidth,
            M7Constant::kHeight,
            M7Constant::kPreserveRatio, buf.data(),
            M7Constant::kWhiteBalance};
        if (!CameraTask::GetSingleton()->GetFrame({fmt})) {
            printf("[M7] Unable to get frame from camera\r\n");
            return {};
        }

        std::vector<uint8_t> jpeg;
        JpegCompressRgb(buf.data(), fmt.width, fmt.height, M7Constant::kJpegQuality, &jpeg);
        return jpeg;
    }
    return {};
}

void getDetections(struct jsonrpc_request* r) {
    printf("[M7] getDetections called\r\n");
    auto* detections = msg::getDetectedObjects();
    std::vector<uint8_t> json;
    json.reserve(2048);
    json.clear();
    StrAppend(&json, "[");

    for (uint8_t i = 0; i < detections->count; i++) {
        auto detection = detections->objects[i];
        StrAppend(&json, "{\n");
        StrAppend(&json, "\"score\": %g,\n", detection.confidence);
        StrAppend(&json, "\"class\": %d,\n", detection.objectClass);
        StrAppend(&json, "\"bounding_box\": {\n");
        StrAppend(&json, "\"top_left\": [%g, %g],\n", detection.bbox.topLeft[0], detection.bbox.topLeft[1]);
        StrAppend(&json, "\"bottom_right\": [%g, %g],\n", detection.bbox.bottomRight[0], detection.bbox.bottomRight[1]);
        StrAppend(&json, "\"width\": %g,\n", detection.bbox.width);
        StrAppend(&json, "\"height\": %g,\n", detection.bbox.height);
        StrAppend(&json, i != detections->count - 1 ? "},\n" : "}\n");
    }

    StrAppend(&json, "]");
    // Add null-terminator just in case
    json.push_back('\0');
    
    jsonrpc_return_success(r, "{%Q: %s}", "detections", json.data());
}

void getIsDetecting(struct jsonrpc_request* r) {
    printf("[M7] getIsDetecting called\r\n");
    jsonrpc_return_success(r, "{%Q: %B}", "is_detecting", isDetecting);
}

void handleM4Message(const uint8_t data[kIpcMessageBufferDataSize]) {
    using namespace msg;

    const auto* msg = reinterpret_cast<const Message*>(data);
    auto type = msg->type;

    printf("[M7] Received message type=%d\r\n", (uint8_t)type);

    switch (type) {
        case MessageType::kAck: {
            printf("[M7] Received ACK from M4\r\n");
            break;
        }

        case MessageType::kDetectedObject: {
            auto obj = msg->data.detectedObject;
            printf("[M7] Detected object received; idx=%d, class=%d\r\n", obj.idx, obj.objectClass);
        }

        default: {
            printf("[M7] Unhandled message type received: %d\r\n", (uint8_t)type);
        }
    }
}

void startM4Detections() {
    if (isDetecting) {
        printf("[M7] Detection already started; reset timer\r\n");
        lastMillis = TimerMillis();
        return;
    }

    printf("[M7] Sound found; starting inferencing on M4\r\n");
    IpcMessage startMsg{};
    startMsg.type = IpcMessageType::kApp;
    auto* app_msg = reinterpret_cast<msg::Message*>(&startMsg.message.data);
    app_msg->type = msg::MessageType::kObjectDetection;
    app_msg->data.objectDetection.shouldStart = 1;
    app_msg->data.objectDetection.shouldStop = 0;
    // msg::Message message;
    // message.type = msg::MessageType::kObjectDetection;
    // message.data.objectDetection.shouldStart = true;
    // bool success = msg::createMessage(message, &startMsg);
    // printf("[M7] Start success=%d\r\n", success);
    bool success = true;
    printf("[M7] message=%d\r\n", startMsg.message.data[0]);
    printf("[M7] alive=%d\r\n", ipc->M4IsAlive(500u));
    if (success) {
        printf("[M7] Sent message\r\n");
        IpcM7::GetSingleton()->SendMessage(startMsg);
    }

    lastMillis = TimerMillis();
    isDetecting = true;
}

void stopM4Detections() {
    if (isDetecting) {
        printf("[M7] Stopping M4 inferencing\r\n");
        IpcMessage stopMsg{};
        stopMsg.type = IpcMessageType::kApp;
        auto* app_msg = reinterpret_cast<msg::Message*>(&stopMsg.message.data);
        app_msg->type = msg::MessageType::kObjectDetection;
        app_msg->data.objectDetection.shouldStart = 0;
        app_msg->data.objectDetection.shouldStop = 1;
        // msg::Message message;
        // message.type = msg::MessageType::kObjectDetection;
        // message.data.objectDetection.shouldStart = true;
        // bool success = msg::createMessage(message, &startMsg);
        // printf("[M7] Start success=%d\r\n", success);
        bool success = true;
        printf("[M7] message=%d\r\n", stopMsg.message.data[0]);
        printf("[M7] alive=%d\r\n", ipc->M4IsAlive(500u));
        if (success) {
            printf("[M7] Sent message\r\n");
            IpcM7::GetSingleton()->SendMessage(stopMsg);
        }

        isDetecting = false;
    }
}

// Run invoke and get the results after the interpreter have already been
// populated with raw audio input.
void runYamnet(tflite::MicroInterpreter* interpreter, FrontendState* frontend_state) {
    auto input_tensor = interpreter->input_tensor(0);
    auto preprocess_start = TimerMillis();
    tensorflow::YamNetPreprocessInput(audio_input.data(), input_tensor,
                                        frontend_state);
    // Reset frontend state.
    FrontendReset(frontend_state);
    auto preprocess_end = TimerMillis();
    if (interpreter->Invoke() != kTfLiteOk) {
        printf("Failed to invoke on test input\r\n");
        vTaskSuspend(nullptr);
    }
    auto current_time = TimerMillis();
    printf(
        "Yamnet preprocess time: %lums, invoke time: %lums, total: "
        "%lums\r\n",
        static_cast<uint32_t>(preprocess_end - preprocess_start),
        static_cast<uint32_t>(current_time - preprocess_end),
        static_cast<uint32_t>(current_time - preprocess_start));
    auto results =
        tensorflow::GetClassificationResults(interpreter, M7Constant::kThreshold, M7Constant::kTopK);
    printf("%s\r\n", tensorflow::FormatClassificationOutput(results).c_str());

    for (auto& c : results) {
        if (M7Constant::kDesiredSoundClasses.find(c.id) != M7Constant::kDesiredSoundClasses.end()) {
            startM4Detections();
            return;
        }
    }
}

void Main() {
    printf("Camera HTTP Example!\r\n");
    // Turn on Status LED to show the board is on.
    LedSet(Led::kStatus, true);

    mutex_ = xSemaphoreCreateMutex();

    CameraTask::GetSingleton()->SetPower(true);
    CameraTask::GetSingleton()->Enable(M7Constant::kMode);

    // Init M4 core
    ipc = IpcM7::GetSingleton();
    ipc->RegisterAppMessageHandler(handleM4Message);
    ipc->StartM4();
    CHECK(ipc->M4IsAlive(500u));

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

    // jsonrpc_init(nullptr, nullptr);
    // jsonrpc_export("detections", getDetections);
    // jsonrpc_export("is_detecting", getIsDetecting);
    // UseHttpServer(new JsonRpcHttpServer);

    std::vector<uint8_t> yamnet_tflite;
    if (!LfsReadFile(M7Constant::kModelName, &yamnet_tflite)) {
        printf("Failed to load model\r\n");
        vTaskSuspend(nullptr);
    }

    const auto* model = tflite::GetModel(yamnet_tflite.data());
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("Model schema version is %lu, supported is %d", model->version(),
            TFLITE_SCHEMA_VERSION);
        vTaskSuspend(nullptr);
    }

    auto edgetpu_context = EdgeTpuManager::GetSingleton()->OpenDevice();
    if (!edgetpu_context) {
        printf("Failed to get TPU context\r\n");
        vTaskSuspend(nullptr);
    }

    tflite::MicroErrorReporter error_reporter;
    auto yamnet_resolver = tensorflow::SetupYamNetResolver<M7Constant::kUseTpu>();

    tflite::MicroInterpreter interpreter{model, yamnet_resolver, tensor_arena,
                                       M7Constant::kTensorArenaSize, &error_reporter};
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        printf("AllocateTensors failed.\r\n");
        vTaskSuspend(nullptr);
    }

    FrontendState frontend_state{};
    if (!coralmicro::tensorflow::PrepareAudioFrontEnd(
            &frontend_state, coralmicro::tensorflow::AudioModel::kYAMNet)) {
        printf("coralmicro::tensorflow::PrepareAudioFrontEnd() failed.\r\n");
        vTaskSuspend(nullptr);
    }

    // Setup audio
    AudioDriverConfig audio_config{AudioSampleRate::k16000_Hz, M7Constant::kNumDmaBuffers,
                                    M7Constant::kDmaBufferSizeMs};
    AudioService audio_service(&audio_driver, audio_config, M7Constant::kAudioServicePriority,
                                M7Constant::kDropFirstSamplesMs);
    LatestSamples audio_latest(
      MsToSamples(AudioSampleRate::k16000_Hz, tensorflow::kYamnetDurationMs));
    audio_service.AddCallback(
        &audio_latest,
        +[](void* ctx, const int32_t* samples, size_t num_samples) {
            static_cast<LatestSamples*>(ctx)->Append(samples, num_samples);
            return true;
        });
    // Delay for the first buffers to fill.
    vTaskDelay(pdMS_TO_TICKS(tensorflow::kYamnetDurationMs));

    LedSet(Led::kTpu, false);

    while (true) {
        audio_latest.AccessLatestSamples(
            [](const std::vector<int32_t>& samples, size_t start_index) {
            size_t i, j = 0;
            // Starting with start_index, grab until the end of the buffer.
            for (i = 0; i < samples.size() - start_index; ++i) {
                audio_input[i] = samples[i + start_index] >> 16;
            }
            // Now fill the rest of the data with the beginning of the
            // buffer.
            for (j = 0; j < samples.size() - i; ++j) {
                audio_input[i + j] = samples[j] >> 16;
            }
            });
        runYamnet(&interpreter, &frontend_state);
        // Delay 500 ms to rate limit the TPU version.
        vTaskDelay(pdMS_TO_TICKS(tensorflow::kYamnetDurationMs));

        if (TimerMillis() - lastMillis >= M7Constant::kM4InferencingTimeMs) {
            stopM4Detections();
        }
    }
}
}  // namespace
}  // namespace coralmicro

extern "C" void app_main(void* param) {
    (void)param;
    coralmicro::Main();
}
