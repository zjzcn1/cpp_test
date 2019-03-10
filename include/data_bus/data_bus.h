#pragma once

#include "publisher.h"

namespace data_bus {

    class DataBus {
    public:
        static const int DEFAULT_QUEUE_SIZE = 1;

        DataBus() = default;

        DataBus(const DataBus &) = delete;

        DataBus &operator=(const DataBus &) = delete;

        template<typename T>
        static void publish(const std::string &topic, Ptr<T> data) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            if (instance()->publisher_map_.count(topic) == 0) {
                instance()->publisher_map_[topic] = std::make_shared<Publisher>(topic);
            }

            Ptr<ProtoMessage> message = std::static_pointer_cast<ProtoMessage>(data);
            instance()->publisher_map_[topic]->publish(message);
        }

        template<typename T>
        static bool subscribe(const std::string &topic, const std::string &subscriber_name, const Callback<T> &callback,
                              int max_queue_size = DEFAULT_QUEUE_SIZE) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            if (instance()->publisher_map_.count(topic) == 0) {
                instance()->publisher_map_[topic] = std::make_shared<Publisher>(topic);
            }

            Ptr<Publisher> publisher = instance()->publisher_map_[topic];
            bool success = publisher->addSubscriber(subscriber_name, callback, max_queue_size);
            if (success) {
                Logger::info("DataBus", "Subscribe successfully, topic={}, subscriber_name={}.", topic,
                             subscriber_name);
            } else {
                Logger::error("DataBus", "Subscribe failed: repeated subscriber_name, topic={}, subscriber_name={}.",
                              topic, subscriber_name);
            }
            return success;
        }


        static bool unsubscribe(const std::string &topic, const std::string &subscriber_name) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            if (instance()->publisher_map_.count(topic) == 0) {
                Logger::error("DataBus", "Can not find topic, topic={}, subscriber_name={}.", topic,
                              subscriber_name);
                return false;
            }
            bool success = instance()->publisher_map_[topic]->removeSubscriber(subscriber_name);
            if (success) {
                Logger::info("DataBus", "Unsubscribe successfully, topic={}, subscriber_name={}.", topic,
                             subscriber_name);
            } else {
                Logger::error("DataBus",
                              "Can not find subscriber name by subscriber_name, topic={}, subscriber_name={}.", topic,
                              subscriber_name);
            }
            return success;
        }

        static std::list<TopicStat> getTopicStats() {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            std::list<TopicStat> stats;
            for (auto &pair : instance()->publisher_map_) {
                stats.push_back(pair.second->getTopicStat());
            }
            return stats;
        }

    private:
        static DataBus *instance() {
            static DataBus instance;
            return &instance;
        }

    private:
        std::mutex mutex_;
        std::map<std::string, Ptr<Publisher>> publisher_map_;
    };
}
