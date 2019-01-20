#pragma once

#include <list>
#include <mutex>
#include <condition_variable>

namespace databus {

    template<typename T>
    class RingQueue {
    public:
        RingQueue() = delete;

        RingQueue(int max_queue_size) : max_queue_size_(max_queue_size), queue_size_(0) {
        }

        void put(const T data) {
            std::lock_guard<std::mutex> locker(mutex_);
            if (isFull()) {
                queue_.pop_front();
                queue_size_--;
            }
            queue_.push_back(data);
            queue_size_++;
            not_empty_.notify_one();
        }

        T take() {
            std::lock_guard<std::mutex> locker(mutex_);
            while (isEmpty()) {
                not_empty_.wait(mutex_);
            }

            T data = queue_.front();
            queue_.pop_front();
            queue_size_--;
            return data;
        }

        int getQueueSize() const {
            return queue_size_;
        }

        int getMaxQueueSize() const {
            return max_queue_size_;
        }

    private:
        bool isFull() const {
            return queue_size_ == max_queue_size_;
        }

        bool isEmpty() const {
            return queue_size_ == 0;
        }

    private:
        int max_queue_size_;
        std::atomic_int queue_size_;
        std::list<T> queue_;
        std::mutex mutex_;
        std::condition_variable_any not_empty_;
    };
}
