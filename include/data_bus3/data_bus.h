#pragma once

#include "publisher.h"

namespace data_bus {

    static const int DEFAULT_QUEUE_SIZE = 10;

    class DataBus {
    public:
        DataBus() = default;

        DataBus(const DataBus &) = delete;

        DataBus &operator=(const DataBus &) = delete;

        template<typename T>
        static void publish(const std::string topic, Ptr<T> data) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            if (instance()->publiser_map_.find(topic) == instance()->publiser_map_.end()) {
                instance()->publiser_map_[topic] = std::make_shared<Publisher>(topic);
            }

            Ptr<ProtoMessage> message = std::static_pointer_cast<ProtoMessage>(data);
            instance()->publiser_map_[topic]->putData(message);
        }

        template<typename T>
        static long subscribe(const std::string topic, const std::string subscriber_name, const Callback<T> &callback,
                              int max_queue_size = DEFAULT_QUEUE_SIZE) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            if (instance()->publiser_map_.find(topic) == instance()->publiser_map_.end()) {
                instance()->publiser_map_[topic] = std::make_shared<Publisher>(topic);
            }

            Ptr<Publisher> queue_info = instance()->publiser_map_[topic];
            long subscriber_id = queue_info->addSubscriber(subscriber_name, callback, max_queue_size);
            return subscriber_id;
        }


        static bool unsubscribe(long subscriber_id) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            for (std::pair<std::string, Ptr<Publisher>> pair : instance()->publiser_map_) {
                bool is_delete = pair.second->removeSubscriber(subscriber_id);
                if (is_delete) {
                    return true;
                }
            }
            return false;
        }

        static std::list<Ptr<TopicStat>> getTopicStats() {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            std::list<Ptr<TopicStat>> stats;
            for (auto &pair : instance()->publiser_map_) {
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
        std::map<std::string, Ptr<Publisher>> publiser_map_;
    };
}
