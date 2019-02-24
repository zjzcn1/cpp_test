#pragma once

#include "queue_worker.h"

namespace data_bus {

    class QueueInfo {
    public:
        QueueInfo(std::string topic) : topic_(topic) {
        }

        void putData(Ptr<ProtoMessage> data) {
            std::lock_guard<std::mutex> locker(mutex_);
            publish_count_++;
            for (const Ptr<QueueWorker> &worker : subscribe_workers_) {
                worker->putData(data);
            }
        }

        template<typename T>
        long addCallback(const std::string callback_name, const Callback<T> &callback, int max_queue_size) {
            std::lock_guard<std::mutex> locker(mutex_);
            Ptr<CallbackHolder> callback_holder(new CallbackHolderT<T>(callback));
            Ptr<QueueWorker> subscribe_worker = std::make_shared<QueueWorker>(topic_, callback_name, callback_holder,
                                                                              max_queue_size);
            subscribe_workers_.push_back(subscribe_worker);
            return subscribe_worker->getCallbackId();
        }

        bool removeCallback(long callback_id) {
            std::lock_guard<std::mutex> locker(mutex_);
            for (std::list<Ptr<QueueWorker>>::iterator it = subscribe_workers_.begin();
                 it != subscribe_workers_.end();) {
                if ((*it)->getCallbackId() == callback_id) {
                    subscribe_workers_.erase(it);
                    return true;
                } else {
                    it++;
                }
            }
            return false;
        }

        Ptr<QueueStat> getQueueStat() {
            std::lock_guard<std::mutex> locker(mutex_);
            Ptr<QueueStat> stat(new QueueStat);
            for (const Ptr<QueueWorker> &worker : subscribe_workers_) {
                stat->topic = topic_;
                stat->publish_count = publish_count_;
                stat->callback_stats.push_back(worker->getCallbackStat());
            }
            return stat;
        }

    private:
        std::string topic_;
        std::mutex mutex_;
        std::list<Ptr<QueueWorker>> subscribe_workers_;

        std::atomic_long publish_count_{0};
    };

}
