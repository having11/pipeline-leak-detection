#include <cstdio>
#include <string>
#include <vector>

#include "libs/base/ipc_m4.h"
#include "libs/base/led.h"
#include "libs/base/tasks.h"
#include "libs/base/filesystem.h"
#include "libs/tensorflow/classification.h"
#include "libs/base/main_freertos_m4.h"
#include "libs/tensorflow/utils.h"
#include "libs/camera/camera.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_error_reporter.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_interpreter.h"

#include "ipc_message.h"
#include "m4_constants.h"

namespace coralmicro {
namespace {
const std::string kModelPath = "/model";
constexpr int kTensorArenaSize = 84 * 1024;
STATIC_TENSOR_ARENA_IN_OCRAM(tensor_arena, kTensorArenaSize);

volatile bool shouldStop = false;

void stopInferenceTask() {
    shouldStop = true;
}

void handleM7Message(const uint8_t data[kIpcMessageBufferDataSize]) {
    const auto* msg = reinterpret_cast<const msg::Message*>(data);
    auto type = msg->type;

    switch (type) {
        case msg::MessageType::kObjectDetection: {
            auto contents = msg->data.objectDetection;

            if (contents.shouldStart && !contents.shouldStop) {
                vTaskResume(xTaskGetCurrentTaskHandle());
            }

            else if (contents.shouldStop && !contents.shouldStart) {
                stopInferenceTask();
            }
            break;
        }

        default: {
        }
    }
}

extern "C" [[noreturn]] void app_main(void *param) {
    IpcM4::GetSingleton()->RegisterAppMessageHandler(handleM7Message);

    std::vector<uint8_t> model;
    if (!LfsReadFile(kModelPath.c_str(), &model)) {
        vTaskSuspend(nullptr);
    }

    tflite::MicroErrorReporter error_reporter;
    tflite::MicroMutableOpResolver<7> resolver;
    resolver.AddConv2D();
    resolver.AddFullyConnected();
    resolver.AddAdd();
    resolver.AddReshape();
    resolver.AddPad();
    resolver.AddDepthwiseConv2D();
    resolver.AddSoftmax();

    tflite::MicroInterpreter interpreter(tflite::GetModel(model.data()), resolver,
                                        tensor_arena, kTensorArenaSize,
                                        &error_reporter);
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        vTaskSuspend(nullptr);
    }

    if (interpreter.inputs().size() != 1) {
        vTaskSuspend(nullptr);
    }

    vTaskSuspend(nullptr);

    while (true) {
        if (shouldStop) {
            shouldStop = false;
            vTaskSuspend(nullptr);
        }

        std::vector<uint8_t> image(
            96 * 96 * CameraFormatBpp(CameraFormat::kRgb));
        auto* input_tensor = interpreter.input_tensor(0);

        // Note if the model is bayered, the raw data will not be rotated.
        auto format = CameraFormat::kRgb;
        CameraFrameFormat fmt{format,
                                CameraFilterMethod::kBilinear,
                                CameraRotation::k270,
                                96,
                                96,
                                false,
                                image.data()};

        if (!CameraTask::GetSingleton()->GetFrame({fmt})) vTaskSuspend(nullptr);

        std::memcpy(tflite::GetTensorData<uint8_t>(input_tensor), image.data(),
                    image.size());
        if (interpreter.Invoke() != kTfLiteOk) vTaskSuspend(nullptr);

        auto* output_tensor = interpreter.output(0);
        // Process the inference results.
        auto defectScore =
            static_cast<int8_t>(output_tensor->data.uint8[0]);
        auto goodScore =
            static_cast<int8_t>(output_tensor->data.uint8[1]);

        auto* result = msg::getClassificationResult();
        // Typically, don't do this. We can here since there are only 2 classes
        result->result = defectScore > goodScore ? 'd' : 'g';
    }
}

}
}