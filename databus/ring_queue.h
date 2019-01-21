#pragma once

#include <list>
#include <mutex>
#include <condition_variable>

namespace databus {

    template<typename T>
    class RingQueue {
    public:
        RingQueue() = delete;

        RingQueue(int max_size) : max_size_(max_size) {
        }

        void put(const T data) {
            std::lock_guard<std::mutex> locker(mutex_);
            if (isFull()) {
                queue_.pop_front();
            }
            queue_.push_back(data);
            not_empty_.notify_one();
        }

        T take() {
            std::lock_guard<std::mutex> locker(mutex_);
            while (isEmpty()) {
                not_empty_.wait(mutex_);
            }

            T data = queue_.front();
            queue_.pop_front();
            return data;
        }

        int size() {
            std::lock_guard<std::mutex> locker(mutex_);
            return queue_.size();
        }

        int max_size() const {
            return max_size_;
        }

    private:
        bool isFull() const {
            return queue_.size() == max_size_;
        }

        bool isEmpty() const {
            return queue_.size() == 0;
        }

    private:
        int max_size_;
        std::list<T> queue_;
        std::mutex mutex_;
        std::condition_variable_any not_empty_;
    };
}
