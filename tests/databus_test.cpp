#include <iostream>
#include <chrono>
#include "../databus/data_bus.h"

using namespace databus;

struct ChatMsg {
    explicit ChatMsg() {
        time = std::chrono::system_clock::now().time_since_epoch().count();
    }

    long time;

    typedef std::shared_ptr<ChatMsg const> ConstPtr;
};

struct Test {
    void onEvent(ChatMsg::ConstPtr e) {
        std::cout << "--------------time: " << e->time << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void onEvent1(ChatMsg::ConstPtr e) {
        std::cout << "time: " << e->time << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void start() {
        std::thread([]() {
            while (true) {
                auto t = std::chrono::system_clock::now().time_since_epoch().count()/1000000;
                std::list<QueueStatPtr> list = DataBus::getQueueStats();
                for (auto stat : list) {
                    std::cout << stat->topic << "  " << stat->max_queue_size << "  " << stat->queue_size << "  "
                              << stat->callback_size << std::endl;
                }
                std::cout << "time: " <<(std::chrono::system_clock::now().time_since_epoch().count()/1000000 - t)<< std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        }).detach();

        DataBus::setMaxQueueSize<ChatMsg>("chat", 2);
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            DataBus::subscribe<ChatMsg>("chat", std::bind(&Test::onEvent, this, std::placeholders::_1));
            DataBus::subscribe<ChatMsg>("chat1", std::bind(&Test::onEvent1, this, std::placeholders::_1));
        }).detach();

        for (int i=0; i<10; i++) {
            std::thread([]() {
                while (1) {
                    ChatMsg::ConstPtr chat1(new ChatMsg());
                    DataBus::publish<ChatMsg>("chat1", chat1);

                    ChatMsg::ConstPtr chat2(new ChatMsg());
                    DataBus::publish<ChatMsg>("chat", chat2);

//                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }).join();
        }
    }
};

int main() {
    Test t;
    t.start();
}
