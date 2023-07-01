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

struct DetectedObject {
    uint8_t idx;
    uint16_t objectClass;
    float confidence;
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

struct ClassificationResult {
    char result;
};

static ClassificationResult* getClassificationResult() {
    static ClassificationResult classificationResult;

    return &classificationResult;
}
}

#endif