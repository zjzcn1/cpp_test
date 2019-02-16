#pragma once

#include <thread>
#include <atomic>
#include <iostream>
#include "ring_queue.h"
#include "queue_stat.h"
#include "filter.h"

namespace walle {

    template<typename T>
    using Callback = std::function<void(std::shared_ptr<T const>)>;

    class QueueWorker {
    public:
        static long generateId() {
            static std::atomic_long id(1);
            return id++;
        }

        virtual long getCallbackId() {
            return 0;
        }

        virtual QueueStatPtr getQueueStat() {
            QueueStatPtr stat(new QueueStat);
            return stat;
        }
    };

    using QueueWorkerPtr = std::shared_ptr<QueueWorker>;

    template<typename T>
    class QueueWorkerImpl : public QueueWorker {
    public:
        QueueWorkerImpl(std::string topic, const Callback<T> &callback, int max_queue_size)
                : topic_(topic), callback_(callback), queue_(max_queue_size), success_count_(0), is_stop_(false),
                  callback_id_(0) {
            worker_ = std::make_shared<std::thread>([this] {
                while (!this->is_stop_) {
                    ConstPtr<T> data = this->queue_.take();

                    std::list<FilterPtr<T>> filters;
                    {
                        std::lock_guard<std::mutex> locker(this->filter_mutex_);
                        filters = this->filters_;
                    }

                    for (const FilterPtr<T> &filter : filters) {
                        try {
                            filter->doFilter(data);
                        } catch (std::exception &e) {
                            std::cerr << "Data bus filter error, topic=" << this->topic_ << ", " << e.what()
                                      << std::endl;
                        }
                    }

                    try {
                        this->callback_(data);
                        success_count_++;
                    } catch (std::exception &e) {
                        std::cerr << "Data bus callback[topic=" << this->topic_ << ", callback_id=" << callback_id_
                                  << "] error: " << e.what() << std::endl;
                    }
                }
            });
            worker_->detach();

            callback_id_ = QueueWorker::generateId();
        }

        ~QueueWorkerImpl() {
            is_stop_ = true;
            worker_->join();
        }

        void addFilter(FilterPtr<T> filter) {
            std::lock_guard<std::mutex> locker(filter_mutex_);
            filters_.push_back(filter);
        }

        void putData(std::shared_ptr<T const> t) {
            queue_.put(t);
        }

        long getCallbackId() override {
            return callback_id_;
        }

        QueueStatPtr getQueueStat() override {
            QueueStatPtr stat(new QueueStat);
            stat->topic = topic_;
            stat->callback_id = callback_id_;
            stat->queue_size = queue_.size();
            stat->max_queue_size = queue_.maxSize();
            stat->incoming_count = queue_.incomingCount();
            stat->dropped_count = queue_.droppedCount();
            std::lock_guard<std::mutex> locker(filter_mutex_);
            stat->filter_size = filters_.size();
            stat->success_count = success_count_;
            return stat;
        }

    private:
        std::atomic_bool is_stop_;
        long callback_id_;
        std::string topic_;
        std::mutex filter_mutex_;
        std::list<FilterPtr<T>> filters_;
        RingQueue<T> queue_;
        Callback<T> callback_;
        std::atomic_long success_count_;
        std::shared_ptr<std::thread> worker_;
    };
}
