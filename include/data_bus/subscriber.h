#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include "ring_queue.h"

namespace data_bus {

    template<typename T>
    using Ptr = std::shared_ptr<T>;
    template<typename T>
    using ConstPtr = std::shared_ptr<T const>;
    // proto message
    using ProtoMessage = google::protobuf::Message;
    // callback
    template<typename T>
    using Callback = std::function<void(ConstPtr<T>)>;

    class Subscriber {
    public:
        explicit Subscriber() {
        }

        virtual void call(Ptr<ProtoMessage> &message) = 0;

        virtual std::string getSubscriberName() = 0;
    };

    template<typename T>
    class SubscriberT : public Subscriber {
    public:
        explicit SubscriberT(const std::string &subscriber_name, const Callback<T> &callback)
                : subscriber_name_(subscriber_name), callback_(callback) {
        }

        std::string getSubscriberName() override {
            return subscriber_name_;
        }

        void call(Ptr<ProtoMessage> &message) override {
            callback_(std::static_pointer_cast<T>(message));
        }

    private:
        std::string subscriber_name_;
        Callback<T> callback_;
    };

}
