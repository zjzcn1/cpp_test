#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include "ring_queue.h"
#include "util/common.h"

namespace data_bus {

    // proto message
    using ProtoMessage = google::protobuf::Message;
    // callback
    template<typename T>
    using Callback = std::function<void(ConstPtr<T>)>;

    // base callback
    class CallbackHolder {
    public:
        CallbackHolder() {
            callback_id_ = generateId();
        }

        virtual void call(Ptr<ProtoMessage> &message) = 0;

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

    // local callback
    template<typename T>
    class CallbackHolderT : public CallbackHolder {
    public:
        explicit CallbackHolderT(const Callback<T> &callback)
                : callback_(callback) {
        }

        void call(Ptr<ProtoMessage> &message) override {
            callback_(std::static_pointer_cast<T>(message));
        }

    private:
        Callback<T> callback_;
    };

}
