#pragma once

#include <thread>
#include "ring_queue.h"
#include "queue_stat.h"
#include "subscriber.h"
#include "util/time_elapsed.h"

namespace data_bus {

    using namespace util;

    class SubscriberWorker {
    public:
        SubscriberWorker(const std::string &topic, Ptr<Subscriber> subscriber, int max_queue_size)
                : topic_(topic), subscriber_(std::move(subscriber)), queue_(max_queue_size) {
            worker_thread_ = std::make_shared<std::thread>([this] {
                while (!this->is_stop_) {
                    Ptr<ProtoMessage> data = this->queue_.take();
                    try {
                        TimeElapsed time;
                        this->subscriber_->call(data);
                        cost_time_sec_ = time.elapsed();
                        total_time_sec_ += cost_time_sec_;
                        success_count_++;
                    } catch (std::exception &e) {
                        Logger::error("SubscriberWorker",
                                      "Data bus callback error, topic={}, subscriber_name={}, error: ",
                                      this->topic_, subscriber_->getSubscriberName(), e.what());
                    }
                }
            });
            worker_thread_->detach();
        }

        void putData(Ptr<ProtoMessage> data) {
            queue_.put(data);
        }

        std::string getSubscriberName() {
            return subscriber_->getSubscriberName();
        }

        QueueStat getQueueStat() {
            QueueStat stat;
            stat.topic = topic_;
            stat.subscriber_name = subscriber_->getSubscriberName();
            stat.queue_size = queue_.size();
            stat.max_queue_size = queue_.maxSize();
            stat.incoming_count = queue_.incomingCount();
            stat.dropped_count = queue_.droppedCount();
            stat.success_count = static_cast<std::size_t >(success_count_);
            stat.cost_time_sec = cost_time_sec_;
            stat.total_time_sec = total_time_sec_;
            return stat;
        }

    private:
        std::atomic_bool is_stop_{false};
        std::string topic_;
        RingQueue<Ptr<ProtoMessage>> queue_;
        Ptr<Subscriber> subscriber_;
        Ptr<std::thread> worker_thread_;

        std::atomic_long success_count_{0};
        double cost_time_sec_{0};
        double total_time_sec_{0};
    };

}
