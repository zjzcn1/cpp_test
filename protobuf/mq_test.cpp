#include <iostream>
#include <string>
#include <map>
#include "Message.pb.h"
#include "Pose.pb.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "util/logger.h"

template<typename T>
using Callback = std::function<void(std::shared_ptr<T>)>;

using MessagePtr = std::shared_ptr<google::protobuf::Message>;

class CallbackHolder {
public:
    virtual void call(std::shared_ptr<google::protobuf::Message> &message) = 0;
};

using CallbackHolderPtr = std::shared_ptr<CallbackHolder>;

template<typename T>
class CallbackHolderT : public CallbackHolder {
private:
    Callback<T> callback_;
public:
    explicit CallbackHolderT(const Callback<T> &callback) : callback_(callback) {
    }

    void call(std::shared_ptr<google::protobuf::Message> &message) override {
        callback_(std::static_pointer_cast<T>(message));
    }
};

google::protobuf::Message *createMessage(const std::string type_name) {
    const google::protobuf::Descriptor *descriptor =
            google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type_name);
    if (descriptor) {
        const google::protobuf::Message *prototype =
                google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype) {
            return prototype->New();
        }
    } else {
        std::cerr << "Can't find protobuf descriptor, type_name=" << type_name << std::endl;
    }
    return nullptr;
}

struct Test {

    std::map<std::string, CallbackHolderPtr> map;

    void onEvent(std::shared_ptr<msg::Pose> e) {
        std::cout << "--------------time: " << e->name() << std::endl;
    }

    void start() {
        CallbackHolderPtr ptr(new CallbackHolderT<msg::Pose>(std::bind(&Test::onEvent, this, std::placeholders::_1)));
        map["pose"] = ptr;

        msg::Pose pose;
        pose.set_id(1);
        pose.set_name("hehe");
        std::string data_buf{};
        pose.SerializeToString(&data_buf);
        std::cout << pose.GetTypeName() << std::endl;

        msg::Message message{};
        message.set_topic("pose");
        message.set_data(data_buf);
        std::string msg_buf{};
        message.SerializeToString(&msg_buf);

        // parse
        msg::Message message1;
        message1.ParseFromString(msg_buf);
        std::cout << message1.topic() << std::endl;
        std::cout << message1.data().length() << std::endl;

        MessagePtr msg_ptr(createMessage(pose.GetTypeName()));
        msg_ptr->ParseFromString(message1.data());
        CallbackHolderPtr callback_ptr = map[message1.topic()];
        callback_ptr->call(msg_ptr);
    }
};

int main() {
    Logger::setLevel(Logger::Level::DEBUG);

    Logger::debug("Test", "hehe {}", 1);

    Test t;
    t.start();

    google::protobuf::ShutdownProtobufLibrary();
}

