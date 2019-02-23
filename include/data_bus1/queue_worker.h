#pragma once

#include <thread>
#include <atomic>
#include <iostream>
#include "ring_queue.h"
#include "queue_stat.h"
#include "proto_codec.h"
#include "util/logger.h"

namespace data_bus {

    class QueueWorker {
    public:
        QueueWorker(std::string topic, int max_queue_size, const Callback<T> &callback)
                : topic_(topic), callback_(callback), queue_(max_queue_size), success_count_(0), is_stop_(false),
                  subscriber_id_(0) {
            worker_thread_ = std::make_shared<std::thread>([this] {
                while (!this->is_stop_) {
                    ConstPtr<T> data = this->queue_.take();

                    try {
                        this->callback_(data);
                        success_count_++;
                    } catch (std::exception &e) {
                        Logger::error("Data bus callback error, topic={}, subscriber_id={}, error: ", this->topic_,
                                      subscriber_id_, e.what());
                    }
                }
            });
            worker_->detach();

            subscriber_id_ = QueueWorker::generateId();
        }

        ~QueueWorkerImpl() {
            is_stop_ = true;
            worker_thread_->join();
        }

        void addFilter(FilterPtr<T> filter) {
            std::lock_guard<std::mutex> locker(filter_mutex_);
            filters_.push_back(filter);
        }

        void putData(std::shared_ptr<T const> t) {
            queue_.put(t);
        }

        long getSubscriberId() override {
            return subscriber_id_;
        }

        QueueStatPtr getQueueStat() override {
            QueueStatPtr stat(new QueueStat);
            stat->topic = topic_;
            stat->subscriber_id = subscriber_id_;
            stat->queue_size = queue_.size();
            stat->max_queue_size = queue_.maxSize();
            stat->incoming_count = queue_.incomingCount();
            stat->dropped_count = queue_.droppedCount();
            std::lock_guard<std::mutex> locker(filter_mutex_);
            stat->filter_size = static_cast<int>(filters_.size());
            stat->success_count = static_cast<uint64_t>(success_count_);
            return stat;
        }

    private:
        std::atomic_bool is_stop_;
        long subscriber_id_;
        std::string topic_;
        std::mutex filter_mutex_;
        RingQueue<T> queue_;
        Callback<T> callback_;
        std::atomic_long success_count_;
        std::shared_ptr<std::thread> worker_thread_;
    };

    using QueueWorkerPtr = std::shared_ptr<QueueWorker>;
}
