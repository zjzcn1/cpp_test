#pragma once

#include <list>
#include <mutex>
#include <condition_variable>

template<typename T>
class ConcurrentQueue {
public:
    ConcurrentQueue(int max_size) : max_size_(max_size) {
    }

    void put(const T x) {
        std::lock_guard<std::mutex> locker(mutex_);

        while (isFull()) {
            not_full_.wait(mutex_);
        }
        queue_.push_back(x);
        not_empty_.notify_one();
    }

    void take(T &x) {
        std::lock_guard<std::mutex> locker(mutex_);

        while (isEmpty()) {
            not_empty_.wait(mutex_);
        }

        x = queue_.front();
        queue_.pop_front();
        not_full_.notify_one();
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
        return queue_.empty();
    }

private:
    std::list<T> queue_;
    std::mutex mutex_;
    std::atomic_int queue_size_;
    std::condition_variable_any not_empty_;
    std::condition_variable_any not_full_;
    int max_size_;
};