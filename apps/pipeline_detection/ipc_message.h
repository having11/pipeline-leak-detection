#ifndef IPC_MESSAGE_H
#define IPC_MESSAGE_H

namespace msg {

#include "libs/base/ipc_message_buffer.h"

enum class MessageType : uint8_t {
    kAck,
    kKeywordSpotted,
    kObjectDetection,
    kDetectedObject,
};

struct AudioFound {
    bool found;
};

struct ObjectDetection {
    bool shouldStart;
    bool shouldStop;
};

struct BoundingBox {
    float topLeft[2];
    float bottomRight[2];
    float width;
    float height;
};

struct DetectedObject {
    uint8_t idx;
    uint16_t objectClass;
    float confidence;
    BoundingBox bbox;
};

union MessageData {
    AudioFound audioFound;
    ObjectDetection objectDetection;
    DetectedObject detectedObject;
};

struct Message {
    MessageType type;
    MessageData data;
} __attribute__((packed));

static_assert(sizeof(Message) <=
              coralmicro::kIpcMessageBufferDataSize);

static bool createMessage(Message msg, coralmicro::IpcMessage* ipcMsg) {
    ipcMsg->type = coralmicro::IpcMessageType::kApp;
    auto* appMsg = reinterpret_cast<msg::Message*>(&(ipcMsg->message.data));
    appMsg->type = msg.type;

    // Don't overrun the buffer
    if (sizeof(msg.data) <= sizeof(ipcMsg->message.data)) {
        return false;
    }

    appMsg->data = msg.data;

    return true;
}

constexpr uint8_t kMaxDetectedObjects = 10;

struct DetectedObjects {
    uint8_t count;
    DetectedObject objects[kMaxDetectedObjects];
};

static DetectedObjects* getDetectedObjects() {
    static DetectedObjects detectedObjects;

    return &detectedObjects;
}
}

#endif