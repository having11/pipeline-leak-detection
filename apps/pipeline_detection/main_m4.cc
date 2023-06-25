#include <cstdio>
#include <string>

#include "libs/base/ipc_m4.h"
#include "libs/base/led.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"

#include "ipc_message.h"
#include "m4_constants.h"

using namespace coralmicro;

static IpcM4* ipc;

void handleM7Message(const uint8_t data[kIpcMessageBufferDataSize]) {
    const auto* msg = reinterpret_cast<const msg::Message*>(data);

    if (msg->type == msg::MessageType::kObjectDetectionDone) {
        auto handle = xTaskGetCurrentTaskHandle();
        vTaskResume(handle);
    }

    IpcMessage ackMsg{};
    ackMsg.type = IpcMessageType::kApp;
    auto* appMsg = reinterpret_cast<msg::Message*>(&ackMsg.message.data);
    appMsg->type = msg::MessageType::kAck;
    ipc->SendMessage(ackMsg);
}

[[noreturn]] void Main() {
    vTaskSuspend(nullptr);
}

extern "C" [[noreturn]] void app_main(void *param) {
    (void)param;
    printf("[M4] Started\r\n");

    ipc = IpcM4::GetSingleton();
    ipc->RegisterAppMessageHandler(handleM7Message);

    Main();
}
