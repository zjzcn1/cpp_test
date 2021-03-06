#pragma once

#include <list>
#include <map>
#include <functional>
#include "queue_worker.h"
#include "proto_codec.h"

namespace data_bus {
    static const int DEFAULT_QUEUE_SIZE = 10;

    class DataBus {
    public:
        DataBus() = default;
        DataBus(const DataBus &) = delete;
        DataBus &operator=(const DataBus &) = delete;

        template<typename T>
        static long subscribe(const std::string topic, const Callback<T> &callback, int queue_size = DEFAULT_QUEUE_SIZE) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);

            if (instance()->queue_worker_map_.find(topic) == instance()->queue_worker_map_.end()) {
                instance()->queue_worker_map_[topic] = std::list<QueueWorkerPtr>();
            }

            QueueWorkerPtr worker_ptr(new QueueWorkerImpl<T>(topic, callback, queue_size));
            instance()->queue_worker_map_[topic].push_back(worker_ptr);

            return worker_ptr->getSubscriberId();
        }

        template<typename T>
        static void publish(const std::string topic, ConstPtr<T> data) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);

            if (instance()->queue_worker_map_.find(topic) == instance()->queue_worker_map_.end()) {
                instance()->queue_worker_map_[topic] = std::list<QueueWorkerPtr>();
            }

            for (QueueWorkerPtr worker_ptr : instance()->queue_worker_map_[topic]) {
                QueueWorkerImpl<T> *impl = static_cast<QueueWorkerImpl<T> *>(worker_ptr.get());
                impl->putData(data);
            }
        }

        static std::list<QueueStatPtr> getQueueStats() {
            std::lock_guard<std::mutex> locker(instance()->mutex_);

            std::list<QueueStatPtr> stats;
            for (auto &pair : instance()->queue_worker_map_) {
                for (const QueueWorkerPtr &worker_ptr : pair.second) {
                    stats.push_back(worker_ptr->getQueueStat());
                }
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
        std::map<std::string, std::list<QueueWorkerPtr>> queue_worker_map_;
    };
}
