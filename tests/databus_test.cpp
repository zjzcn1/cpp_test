#include <iostream>
#include <chrono>
#include "data_bus3/data_bus.h"
#include "Pose.pb.h"
#include "json.hpp"

using namespace data_bus;
using json = nlohmann::json;

struct ChatMsg {
    explicit ChatMsg() {
        time = std::chrono::system_clock::now().time_since_epoch().count();
    }

    long time;

    typedef std::shared_ptr<ChatMsg const> ConstPtr;
};

struct Test {
    void onEvent(ConstPtr<ProtoMessage> e) {
        std::cout << "--------------time: " << e->ByteSize() << std::endl;

        int size = e->ByteSize();
        Ptr<std::vector<uint8_t>> buf(new std::vector<uint8_t>(size));
        e->SerializeToArray(buf->data(), size);
        json p={{"data", *buf}};

        std::cout << "--------------data: " << p.dump() << std::endl;
    }

    void onEvent1(ConstPtr<msg::Pose> e) {
        std::cout << "--------------time: " << e->name() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    bool is_stop = false;
    void start() {
        std::thread([this]() {
            while (!is_stop) {
                auto t = std::chrono::system_clock::now().time_since_epoch().count()/1000000;
                auto stats = DataBus::getTopicStats();
                for (const auto &s : stats) {
                    Logger::info("QueueStats", "{} {}", s->topic, s->publish_count);
                    for (auto stat : s->callback_stats) {
                        Logger::info("QueueStats", "{}", stat->toString());
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        }).detach();

//        std::thread([this]() {
//            std::this_thread::sleep_for(std::chrono::seconds(1));
            DataBus::subscribe<ProtoMessage>("chat", "main_test", std::bind(&Test::onEvent, this, std::placeholders::_1));
            DataBus::subscribe<msg::Pose>("chat1", "main_test", std::bind(&Test::onEvent1, this, std::placeholders::_1));
//        }).detach();

        for (int i=0; i<10; i++) {
            std::thread([]() {
                while (1) {
                    Ptr<msg::Pose> chat(new msg::Pose());
                    chat->set_id(1);
                    chat->set_name("hehe pose");
                    DataBus::publish<msg::Pose>("chat", chat);

                    Ptr<msg::Pose> chat1(new msg::Pose());
                    chat1->set_id(1);
                    chat1->set_name("hehe pose1");
                    DataBus::publish<msg::Pose>("chat1", chat1);

                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }).join();
        }
    }
};

int main() {
    Test t;
    t.start();
}
