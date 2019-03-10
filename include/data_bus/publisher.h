#pragma once

#include "subscriber_worker.h"

namespace data_bus {

    class Publisher {
    public:
        explicit Publisher(const std::string &topic) : topic_(topic) {
        }

        void publish(Ptr<ProtoMessage> data) {
            std::lock_guard<std::mutex> locker(mutex_);
            publish_count_++;
            for (const std::pair<std::string, Ptr<SubscriberWorker>> &pair : workers_) {
                pair.second->putData(data);
            }
        }

        template<typename T>
        bool addSubscriber(const std::string &subscriber_name, const Callback<T> &callback, int max_queue_size) {
            std::lock_guard<std::mutex> locker(mutex_);
            if (workers_.count(subscriber_name) > 0) {
                return false;
            }

            Ptr<Subscriber> subscriber(new SubscriberT<T>(subscriber_name, callback));
            Ptr<SubscriberWorker> worker = std::make_shared<SubscriberWorker>(topic_, subscriber, max_queue_size);
            workers_[subscriber_name] = worker;
            return true;
        }

        bool removeSubscriber(const std::string &subscriber_name) {
            std::lock_guard<std::mutex> locker(mutex_);
            return workers_.erase(subscriber_name) > 0;
        }

        TopicStat getTopicStat() {
            std::lock_guard<std::mutex> locker(mutex_);
            TopicStat stat;
            stat.topic = topic_;
            stat.publish_count = static_cast<size_t>(publish_count_);
            for (const std::pair<std::string, Ptr<SubscriberWorker>> &pair : workers_) {
                stat.queue_stats.push_back(pair.second->getQueueStat());
            }
            return stat;
        }

    private:
        std::string topic_;
        std::mutex mutex_;
        std::map<std::string, Ptr<SubscriberWorker>> workers_;

        std::atomic_long publish_count_{0};
    };

}
