#pragma once

#include <list>
#include <mutex>
#include <condition_variable>

namespace walle {

    template<typename T>
    using ConstPtr = std::shared_ptr<T const>;

    template<typename T>
    class RingQueue {
    public:
        RingQueue() = delete;

        RingQueue(int max_size) : max_size_(max_size), incoming_count_(0), dropped_count_(0) {
        }

        void put(const ConstPtr<T> data) {
            std::lock_guard<std::mutex> locker(mutex_);
            if (queue_.size() >= max_size_) {
                queue_.pop_front();
                dropped_count_++;
            }
            incoming_count_++;
            queue_.push_back(data);
            not_empty_.notify_one();
        }

        ConstPtr<T> take() {
            std::lock_guard<std::mutex> locker(mutex_);
            while (queue_.empty()) {
                not_empty_.wait(mutex_);
            }

            ConstPtr<T> data = queue_.front();
            queue_.pop_front();
            return data;
        }

        ConstPtr<T> front() {
            std::lock_guard<std::mutex> locker(mutex_);
            if (queue_.empty()) {
                return nullptr;
            }

            ConstPtr<T> data = queue_.front();
            return data;
        }

        void clear() {
            std::lock_guard<std::mutex> locker(mutex_);
            queue_.clear();
        }

        int size() {
            std::lock_guard<std::mutex> locker(mutex_);
            return queue_.size();
        }

        uint64_t incomingCount() {
            std::lock_guard<std::mutex> locker(mutex_);
            return incoming_count_;
        }

        uint64_t droppedCount() {
            std::lock_guard<std::mutex> locker(mutex_);
            return dropped_count_;
        }

        int maxSize() const {
            return max_size_;
        }

        bool isFull() const {
            std::lock_guard<std::mutex> locker(mutex_);
            return queue_.size() >= max_size_;
        }

        bool isEmpty() const {
            std::lock_guard<std::mutex> locker(mutex_);
            return queue_.empty();
        }

    private:
        uint64_t incoming_count_;
        uint64_t dropped_count_;
        int max_size_;
        std::list<ConstPtr<T>> queue_;
        std::mutex mutex_;
        std::condition_variable_any not_empty_;
    };
}
