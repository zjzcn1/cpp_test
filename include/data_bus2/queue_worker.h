#pragma once

#include <thread>
#include <atomic>
#include <iostream>
#include "ring_queue.h"
#include "queue_stat.h"
#include "callback_holder.h"
#include "util/time_elapsed.h"

namespace data_bus {

    class QueueWorker {
    public:
        QueueWorker(std::string topic, std::string subscriber_name, int max_queue_size,
                    CallbackHolderPtr callback_holder)
                : topic_(topic), subscriber_name_(subscriber_name), queue_(max_queue_size),
                  callback_holder_(std::move(callback_holder)), success_count_(0), cost_time_sec_(0),
                  total_time_sec_(0), is_stop_(false) {
            worker_thread_ = std::make_shared<std::thread>([this] {
                while (!this->is_stop_) {
                    ProtoMessagePtr data = this->queue_.take();
                    try {
                        TimeElapsed time;
                        this->callback_holder_->call(data);
                        cost_time_sec_ = time.elapsed();
                        total_time_sec_ += cost_time_sec_;
                        success_count_++;
                    } catch (std::exception &e) {
                        Logger::error("QueueWorker",
                                      "Data bus callback error, topic={}, subscriber_name={}, callback_id={}, error: ",
                                      this->topic_, this->subscriber_name_, callback_holder_->getCallbackId(),
                                      e.what());
                    }
                }
            });
            worker_thread_->detach();
        }

        ~QueueWorker() {
            is_stop_ = true;
            worker_thread_->join();
        }

        void putData(ProtoMessagePtr data) {
            queue_.put(data);
        }

        long getCallbackId() {
            return callback_holder_->getCallbackId();
        }

        QueueStatPtr getQueueStat() {
            QueueStatPtr stat(new QueueStat);
            stat->topic = topic_;
            stat->subscriber_name = subscriber_name_;
            stat->callback_id = getCallbackId();
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
        std::atomic_bool is_stop_;
        std::string topic_;
        std::string subscriber_name_;
        RingQueue<ProtoMessagePtr> queue_;
        CallbackHolderPtr callback_holder_;
        std::atomic_long success_count_;
        std::shared_ptr<std::thread> worker_thread_;

        double cost_time_sec_;
        double total_time_sec_{};
    };

    using QueueWorkerPtr = std::shared_ptr<QueueWorker>;
}
