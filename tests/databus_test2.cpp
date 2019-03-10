#include <iostream>
#include <chrono>
#include "data_bus/data_bus.h"
#include "data_bus/data_bus_client.h"
#include "data_bus/data_bus_proxy.h"

#include "Pose.pb.h"

using namespace data_bus;

struct ChatMsg {
    explicit ChatMsg() {
        time = std::chrono::system_clock::now().time_since_epoch().count();
    }

    long time;

    typedef std::shared_ptr<ChatMsg const> ConstPtr;
};

struct Test {
    void onEvent(ConstPtr<msg::Pose> e) {
        std::cout << "bus pose: " << e->name() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void onEvent1(ConstPtr<msg::Pose> e) {
        std::cout << "client pose: " << e->name() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    bool is_stop = false;

    void start() {
        DataBusProxy::listen(8085);
        DataBusClient::connect("127.0.0.1", 8085);
        std::thread([this]() {
            while (!is_stop) {
                auto t = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
                auto stats = DataBus::getTopicStats();
                for (const auto &s : stats) {
                    Logger::info("QueueStats", "{} {}", s.topic, s.publish_count);
                    for (auto stat : s.queue_stats) {
                        Logger::info("QueueStats", "{}", stat.toString());
                    }
                }

                auto stats1 = DataBusClient::getQueueStats();
                for (auto &stat : stats1) {
                    Logger::info("ClientQueueStats", "{}", stat.toString());
                }

                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        }).detach();

        DataBus::subscribe<msg::Pose>("chat", "main_test", std::bind(&Test::onEvent, this, std::placeholders::_1));

        DataBusClient::subscribe<msg::Pose>("chat", "main_test1",
                                            std::bind(&Test::onEvent1, this, std::placeholders::_1));

        std::thread([]() {
            while (true) {
//                Ptr<msg::Pose> chat(new msg::Pose());
//                chat->set_id(1);
//                chat->set_name("server pose");
//                DataBus::publish<msg::Pose>("chat", chat);

                Ptr<msg::Pose> chat1(new msg::Pose());
                chat1->set_id(1);
                chat1->set_name("client pose");
                DataBusClient::publish<msg::Pose>("chat", chat1);

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }).join();
    }
};

int main() {
    Test t;
    t.start();
}
