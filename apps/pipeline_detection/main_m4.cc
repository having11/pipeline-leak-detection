#include <cstdio>
#include <string>

#include "libs/base/ipc_m4.h"
#include "libs/base/led.h"
#include "libs/base/tasks.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"

#include "ipc_message.h"
#include "m4_constants.h"

using namespace coralmicro;

static IpcM4* ipc;
static TaskHandle_t inferenceHandle;
static volatile bool shouldStop = false;

[[noreturn]] void inference_task(void* param) {
    bool on = false;

    while (true) {
        if (shouldStop) {
            printf("[M4] Suspending inferencing task\r\n");
            LedSet(Led::kStatus, false);
            on = false;
            shouldStop = false;
            vTaskSuspend(nullptr);
        }

        LedSet(Led::kStatus, on);
        on = !on;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void stopInferenceTask() {
    printf("[M4] Set inferencing task stop flag\r\n");
    
    shouldStop = true;
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
                vTaskResume(inferenceHandle);
                printf("[M4] Task started\r\n");
            }

            else if (contents.shouldStop && !contents.shouldStart) {
                printf("[M4] Stop requested\r\n");
                stopInferenceTask();
            }
            break;
        }

        default: {
            printf("[M4] Unhandled message type received: %d\r\n", (uint8_t)type);
        }
    }

    IpcMessage ackMsg{};
    ackMsg.type = IpcMessageType::kApp;
    auto* app_msg = reinterpret_cast<msg::Message*>(&ackMsg.message.data);
    app_msg->type = msg::MessageType::kAck;
    IpcM4::GetSingleton()->SendMessage(ackMsg);
}

extern "C" [[noreturn]] void app_main(void *param) {
    (void)param;
    printf("[M4] Started\r\n");

    ipc = IpcM4::GetSingleton();
    ipc->RegisterAppMessageHandler(handleM7Message);

    xTaskCreate(&inference_task, "inference_user_task", configMINIMAL_STACK_SIZE * 30,
                    nullptr, kAppTaskPriority, &inferenceHandle);
    vTaskSuspend(inferenceHandle);

    vTaskSuspend(nullptr);
}
