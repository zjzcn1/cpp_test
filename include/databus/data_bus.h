#pragma once

#include <list>
#include <map>
#include <functional>
#include "queue_worker.h"
#include "queue_stat.h"


namespace databus {
    static const int DEFAULT_QUEUE_SIZE = 100;

    class DataBus {
    public:
        DataBus(const DataBus &) = delete;

        DataBus &operator=(const DataBus &) = delete;

        ~DataBus() {
            queue_worker_map_.clear();
        };

        template<typename T>
        static bool setMaxQueueSize(const std::string topic, const int max_size) {
            const QueueWorkerPtr &queue_worker = instance()->getQueueWorker<T>(topic, max_size);
            return queue_worker->getMaxQueueSize() == max_size;
        }

        template<typename T>
        static void subscribe(const std::string topic, const Callback<T> &callback) {
            const QueueWorkerPtr &queue_worker = instance()->getQueueWorker<T>(topic, DEFAULT_QUEUE_SIZE);
            QueueWorkerImpl<T> *impl = static_cast<QueueWorkerImpl<T> *>(queue_worker.get());
            impl->registerCallback(callback);
        }

        template<typename T>
        static void publish(const std::string topic, std::shared_ptr<T const> data) {
            const QueueWorkerPtr &queue_worker = instance()->getQueueWorker<T>(topic, DEFAULT_QUEUE_SIZE);
            QueueWorkerImpl<T> *impl = static_cast<QueueWorkerImpl<T> *>(queue_worker.get());
            impl->putData(data);
        }

        static std::list<QueueStatPtr> getQueueStats() {
            std::list<QueueStatPtr> list;
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            for (const std::pair<std::string, QueueWorkerPtr> &p : instance()->queue_worker_map_) {
                QueueStatPtr stat(new QueueStat);
                stat->topic = p.first;
                stat->queue_size = p.second->getQueueSize();
                stat->max_queue_size = p.second->getMaxQueueSize();
                stat->callback_size = p.second->getCallbackSize();
                list.push_back(stat);
            }
            return list;
        }

    private:
        DataBus() = default;

        static DataBus *instance() {
            static DataBus instance;
            return &instance;
        }

        template<typename T>
        static QueueWorkerPtr getQueueWorker(std::string topic, int max_queue_size) {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            QueueWorkerPtr &queue_worker = instance()->queue_worker_map_[topic];
            if (queue_worker == nullptr) {
                queue_worker.reset(new QueueWorkerImpl<T>(topic, max_queue_size));
            }
            return queue_worker;
        }

    private:
        std::mutex mutex_;
        std::map<std::string, QueueWorkerPtr> queue_worker_map_;
    };
}
