#pragma once

#include <thread>
#include <atomic>
#include <iostream>
#include "ring_queue.h"
#include 6
#include "util/logger.h"

namespace data_bus {

    // proto message ptr
    using MessagePtr = std::shared_ptr<google::protobuf::Message>;

    // callback
    template<typename T>
    using Callback = std::function<void(std::shared_ptr<T const>)>;

    // base callback
    class CallbackHolder {
    public:
        CallbackHolder() {
            callback_id_ = generateId();
        }

        virtual void call(MessagePtr &message) = 0;

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
    class LocalCallbackHolder : public CallbackHolder {
    public:
        explicit LocalCallbackHolder(const Callback<T> &callback)
                : callback_(callback) {
        }

        void call(MessagePtr &message) override {
            callback_(std::static_pointer_cast<T>(message));
        }
    private:
        Callback<T> callback_;
    };

    // remote callback
    template<typename T>
    class RemoteCallbackHolder : public CallbackHolder {
    public:
        void call(MessagePtr &message) override {
            Logger::info("RemoteCallbackHolder", "send message");
        }
    };
}
