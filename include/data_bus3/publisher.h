#pragma once

#include "queue_worker.h"

namespace data_bus {

    class Publisher {
    public:
        explicit Publisher(std::string topic) : topic_(topic) {
        }

        void putData(Ptr<ProtoMessage> data) {
            std::lock_guard<std::mutex> locker(mutex_);
            publish_count_++;
            for (const Ptr<QueueWorker> &worker : subscribe_workers_) {
                worker->putData(data);
            }
        }

        template<typename T>
        long addSubscriber(const std::string subscriber_name, const Callback<T> &callback, int max_queue_size) {
            std::lock_guard<std::mutex> locker(mutex_);
            Ptr<Subscriber> subscriber(new SubscriberT<T>(callback));
            Ptr<QueueWorker> subscribe_worker = std::make_shared<QueueWorker>(topic_, subscriber_name, subscriber,
                                                                              max_queue_size);
            subscribe_workers_.push_back(subscribe_worker);
            return subscribe_worker->getSubscriberId();
        }

        bool removeSubscriber(long subscriber_id) {
            std::lock_guard<std::mutex> locker(mutex_);
            for (std::list<Ptr<QueueWorker>>::iterator it = subscribe_workers_.begin();
                 it != subscribe_workers_.end();) {
                if ((*it)->getSubscriberId() == subscriber_id) {
                    subscribe_workers_.erase(it);
                    return true;
                } else {
                    it++;
                }
            }
            return false;
        }

        Ptr<TopicStat> getTopicStat() {
            std::lock_guard<std::mutex> locker(mutex_);
            Ptr<TopicStat> stat(new TopicStat);
            stat->topic = topic_;
            stat->publish_count = publish_count_;
            for (const Ptr<QueueWorker> &worker : subscribe_workers_) {
                stat->callback_stats.push_back(worker->getQueueStat());
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
