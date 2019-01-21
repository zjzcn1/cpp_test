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

        while (queue_.size() == max_size_) {
            not_full_.wait(mutex_);
        }
        queue_.push_back(x);
        not_empty_.notify_one();
    }

    T front() {
        std::lock_guard<std::mutex> locker(mutex_);
        return queue_.front();
    }

    T take() {
        std::lock_guard<std::mutex> locker(mutex_);

        while (queue_.empty()) {
            not_empty_.wait(mutex_);
        }

        T data = queue_.front();
        queue_.pop_front();
        not_full_.notify_one();
        return data;
    }

    int size() {
        std::lock_guard<std::mutex> locker(mutex_);
        return queue_.size();
    }

    int maxSize() const {
        return max_size_;
    }

    bool isFull() {
        std::lock_guard<std::mutex> locker(mutex_);
        return queue_.size() == max_size_;
    }

    bool isEmpty() {
        std::lock_guard<std::mutex> locker(mutex_);
        return queue_.empty();
    }

private:
    std::list<T> queue_;
    std::mutex mutex_;
    std::condition_variable_any not_empty_;
    std::condition_variable_any not_full_;
    int max_size_;
};