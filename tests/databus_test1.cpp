#include <iostream>
#include <chrono>
#include "data_bus/data_bus.h"
#include "util/logger.h"

using namespace data_bus;

struct ChatMsg {
    explicit ChatMsg() {
        time = std::chrono::system_clock::now().time_since_epoch().count();
    }

    long time;

    typedef std::shared_ptr<ChatMsg const> ConstPtr;
};

void monitorDataBus() {
    std::thread([&]() {
        while (true) {
            std::list<QueueStatPtr> list = DataBus::getQueueStats();
            Logger::info("------------------------------------------------------------------------------------");
            for (auto stat : list) {
                Logger::info(
                        "DataBus: topic={}, max_queue_size={}, queue_size={}, subscriber_id={}, filter_size={}, "
                        "success_count={}, incomming_count={}, dropped_count={}.",
                        stat->topic, stat->max_queue_size, stat->queue_size, stat->subscriber_id, stat->filter_size,
                        stat->success_count, stat->incoming_count, stat->dropped_count);
            }
            Logger::info("------------------------------------------------------------------------------------");
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }
    }).detach();
}

class TestFilter : public Filter<ChatMsg> {

    bool doFilter(ChatMsg::ConstPtr data) override {
        Logger::info("filter out: {}", data->time);
        return false;
    }
};

struct Test {
    void onEvent(ChatMsg::ConstPtr e) {
        std::cout << "--------------time: " << e->time << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void onEvent1(ChatMsg::ConstPtr e) {
        std::cout << "time: " << e->time << std::endl;
//        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void start() {
        monitorDataBus();

        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            long id = DataBus::subscribe<ChatMsg>("chat", std::bind(&Test::onEvent, this, std::placeholders::_1));
            DataBus::addFilter(id, FilterPtr<ChatMsg>(new TestFilter));
            Logger::info("topic={}, id={}", "chat", id);
            long id2 = DataBus::subscribe<ChatMsg>("chat1", std::bind(&Test::onEvent1, this, std::placeholders::_1),
                                                   100);
            Logger::info("topic={}, id={}", "chat1", id2);
        }).detach();

        for (int i = 0; i < 1; i++) {
            std::thread([]() {
                while (1) {
                    ChatMsg::ConstPtr chat1(new ChatMsg());
                    DataBus::publish("chat1", chat1);

                    ChatMsg::ConstPtr chat2(new ChatMsg());
                    DataBus::publish<ChatMsg>("chat", chat2);

                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }).join();
        }
    }
};

int main() {
    Logger::setLevel(Logger::Level::DEBUG);
    Logger::setSink(Logger::Sink::CONSOLE_AND_DAILY);
    Test t;
    t.start();
}
