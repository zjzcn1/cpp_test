#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include "ring_queue.h"
#include "util/logger.h"

namespace data_bus {

    using namespace util;

    // proto message
    using ProtoMessage = google::protobuf::Message;
    using ProtoMessagePtr = std::shared_ptr<google::protobuf::Message>;

    template<typename T>
    using Ptr = std::shared_ptr<T>;

    template<typename T>
    using ConstPtr = std::shared_ptr<T const>;

    // callback
    template<typename T>
    using Callback = std::function<void(ConstPtr<T>)>;

    // base callback
    class CallbackHolder {
    public:
        CallbackHolder() {
            callback_id_ = generateId();
        }

        virtual void call(ProtoMessagePtr &message) = 0;

        long getCallbackId() {
            return callback_id_;
        }

        static long generateId() {
            static std::atomic_long id(1);
            return id++;
        }

    private:
        long callback_id_;
    };

    using CallbackHolderPtr = std::shared_ptr<CallbackHolder>;

    // local callback
    template<typename T>
    class CallbackHolderT : public CallbackHolder {
    public:
        explicit CallbackHolderT(const Callback<T> &callback)
                : callback_(callback) {
        }

        void call(ProtoMessagePtr &message) override {
            callback_(std::static_pointer_cast<T>(message));
        }

    private:
        Callback<T> callback_;
    };

}
