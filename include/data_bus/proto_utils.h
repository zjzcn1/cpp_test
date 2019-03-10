#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "util/logger.h"

namespace data_bus {
    using namespace util;

    class ProtoUtils {
    public:
        static google::protobuf::Message *createMessage(const std::string type_name) {
            const google::protobuf::Descriptor *descriptor =
                    google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type_name);
            if (descriptor) {
                const google::protobuf::Message *prototype =
                        google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
                if (prototype) {
                    return prototype->New();
                }
            } else {
                Logger::error("ProtoUtils", "Can't find protobuf descriptor, type_name={}", type_name);
            }
            return nullptr;
        }
    };
}
