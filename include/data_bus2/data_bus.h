#pragma once

#include "queue_info.h"

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
            if (instance()->queue_info_map_.find(topic) == instance()->queue_info_map_.end()) {
                instance()->queue_info_map_[topic] = std::make_shared<QueueInfo>(topic);
            }

            Ptr<ProtoMessage> message = std::static_pointer_cast<ProtoMessage>(data);
            instance()->queue_info_map_[topic]->putData(message);
        }

        template<typename T>
        static long subscribe(const std::string topic, const std::string callback_name, const Callback<T> &callback,
                              int max_queue_size = DEFAULT_QUEUE_SIZE) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            if (instance()->queue_info_map_.find(topic) == instance()->queue_info_map_.end()) {
                instance()->queue_info_map_[topic] = std::make_shared<QueueInfo>(topic);
            }

            Ptr<QueueInfo> queue_info = instance()->queue_info_map_[topic];
            long callback_id = queue_info->addCallback(callback_name, callback, max_queue_size);
            return callback_id;
        }


        template<typename T>
        static bool unsubscribe(const std::string topic, long callback_id) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            if (instance()->queue_info_map_.find(topic) == instance()->queue_info_map_.end()) {
                return false;
            }

            Ptr<QueueInfo> queue_info = instance()->queue_info_map_[topic];
            return queue_info->removeCallback(callback_id);
        }

        static std::list<Ptr<QueueStat>> getQueueStats() {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            std::list<Ptr<QueueStat>> stats;
            for (auto &pair : instance()->queue_info_map_) {
                stats.push_back(pair.second->getQueueStat());
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
        std::map<std::string, Ptr<QueueInfo>> queue_info_map_;
    };
}
