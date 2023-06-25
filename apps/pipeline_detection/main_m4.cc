#include <cstdio>
#include <string>

#include "libs/base/ipc_m4.h"
#include "libs/base/led.h"
#include "libs/base/tasks.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"

#include "ipc_message.h"
#include "m4_constants.h"

// TODO: Add messaging checks for start/stop of classification

// TODO: Add method for getting images (don't forget the mutex!)

using namespace coralmicro;

static IpcM4* ipc;
static TaskHandle_t *inferenceHandle;

[[noreturn]] void inference_task(void* param) {
    // TODO: convert to interpreter pointer for invocation

    while (true) {
        // Get image

        // Invoke

        // Store results in array
        auto* detections = msg::getDetectedObjects();
    }
}

void stopInferenceTask() {
    printf("[M4] Deleting inferencing task\r\n");
    if (inferenceHandle) {
        vTaskDelete(*inferenceHandle);
        inferenceHandle = nullptr;
    }
}

void handleM7Message(const uint8_t data[kIpcMessageBufferDataSize]) {
    const auto* msg = reinterpret_cast<const msg::Message*>(data);
    auto type = msg->type;

    printf("[M4] Received msg type=%d\r\n", (uint8_t)type);

    switch (type) {
        case msg::MessageType::kAck:
            break;

        case msg::MessageType::kObjectDetection: {
            auto contents = msg->data.objectDetection;

            if (contents.shouldStart && !contents.shouldStop) {
                printf("[M4] Start requested\r\n");
                // Create new task and start it
                stopInferenceTask();
                xTaskCreate(&inference_task, "inference_user_task", configMINIMAL_STACK_SIZE * 30,
                    nullptr, kAppTaskPriority, inferenceHandle);
            }

            if (contents.shouldStop && !contents.shouldStart) {
                printf("[M4] Stop requested\r\n");
                // Delete detection task
                stopInferenceTask();
            }
            break;
        }

        default: {
            printf("[M4] Unhandled message type received: %d\r\n", (uint8_t)type);
        }
    }

    IpcMessage ackMsg{};
    bool success = msg::createMessage({ .type = msg::MessageType::kAck }, &ackMsg);
    if (success) {
        ipc->SendMessage(ackMsg);
    }
}

extern "C" [[noreturn]] void app_main(void *param) {
    (void)param;
    printf("[M4] Started\r\n");

    ipc = IpcM4::GetSingleton();
    ipc->RegisterAppMessageHandler(handleM7Message);

    while (true) {
        ;
    }
}
