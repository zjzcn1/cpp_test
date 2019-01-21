#pragma once

#include <thread>
#include <atomic>
#include <iostream>
#include "ring_queue.h"

namespace databus {

    template<typename T>
    using Callback = std::function<void(std::shared_ptr<T const>)>;

    class QueueWorker {
    public:
        virtual int getQueueSize() {
            return 0;
        }

        virtual int getMaxQueueSize() {
            return 0;
        }

        virtual int getCallbackSize() {
            return 0;
        }

    };

    using QueueWorkerPtr = std::shared_ptr<QueueWorker>;

    template<typename T>
    class QueueWorkerImpl : public QueueWorker {
    public:
        QueueWorkerImpl(std::string topic, int max_queue_size) : topic_(topic), queue_(max_queue_size),
                                                                 is_stop_(false) {
            worker_ = std::make_shared<std::thread>([this] {
                while (!this->is_stop_) {
                    std::shared_ptr<T const> data = queue_.take();

                    std::list<Callback<T>> callbacks;
                    {
                        std::lock_guard<std::mutex> locker(this->mutex_);
                        callbacks = this->callback_list_;
                    }

                    for (Callback<T> callback : callbacks) {
                        try {
                            callback(data);
                        } catch (std::exception &e) {
                            std::cerr << "DataBus callback error, topic=" << this->topic_ << ", " << e.what()
                                      << std::endl;
                        }
                    }
                }
            });
            worker_->detach();
        }

        ~QueueWorkerImpl() {
            is_stop_ = true;
            worker_->join();
        }

        void registerCallback(const Callback<T> &callback) {
            std::lock_guard<std::mutex> locker(mutex_);
            callback_list_.push_back(callback);
        }

        void putData(std::shared_ptr<T const> t) {
            queue_.put(t);
        }

        virtual int getQueueSize() {
            return queue_.size();
        }

        virtual int getMaxQueueSize() {
            return queue_.max_size();
        }

        virtual int getCallbackSize() {
            std::unique_lock<std::mutex> locker(mutex_);
            return callback_list_.size();
        }

    private:
        std::string topic_;
        RingQueue<std::shared_ptr<T const>> queue_;
        std::mutex mutex_;
        std::list<Callback<T>> callback_list_;
        std::atomic_bool is_stop_;
        std::shared_ptr<std::thread> worker_;
    };
}
