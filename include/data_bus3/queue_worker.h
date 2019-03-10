#pragma once

#include "ring_queue.h"
#include "queue_stat.h"
#include "subscriber.h"

namespace data_bus {

    class QueueWorker {
    public:
        QueueWorker(std::string topic, std::string subscriber_name, Ptr<Subscriber> subscriber, int max_queue_size)
                : topic_(topic), subscriber_name_(subscriber_name), subscriber_(std::move(subscriber)),
                  queue_(max_queue_size) {
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
                        Logger::error("QueueWorker",
                                      "Data bus callback error, topic={}, subscriber_name={}, subscriber_id={}, error: ",
                                      this->topic_, this->subscriber_name_, subscriber_->getSubscriberId(),
                                      e.what());
                    }
                }
            });
            worker_thread_->detach();
        }

        void putData(Ptr<ProtoMessage> data) {
            queue_.put(data);
        }

        long getSubscriberId() {
            return subscriber_->getSubscriberId();
        }

        Ptr<QueueStat> getQueueStat() {
            Ptr<QueueStat> stat(new QueueStat);
            stat->topic = topic_;
            stat->subscriber_name = subscriber_name_;
            stat->subscriber_id = getSubscriberId();
            stat->queue_size = queue_.size();
            stat->max_queue_size = queue_.maxSize();
            stat->incoming_count = queue_.incomingCount();
            stat->dropped_count = queue_.droppedCount();
            stat->success_count = static_cast<std::size_t >(success_count_);
            stat->cost_time_sec = cost_time_sec_;
            stat->total_time_sec = total_time_sec_;
            return stat;
        }

    private:
        std::atomic_bool is_stop_{false};
        std::string topic_;
        std::string subscriber_name_;
        RingQueue<Ptr<ProtoMessage>> queue_;
        Ptr<Subscriber> subscriber_;
        std::shared_ptr<std::thread> worker_thread_;

        std::atomic_long success_count_{0};
        double cost_time_sec_{0};
        double total_time_sec_{0};
    };

}
