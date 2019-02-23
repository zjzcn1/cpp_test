#pragma once

#include<functional>
#include<chrono>
#include<thread>
#include<atomic>
#include<memory>
#include<mutex>
#include<condition_variable>
#include <iostream>

namespace util {

    class Timer {
    public:
        Timer() : stopped_(true) {
        }

        ~Timer() {
            stop();
        }

        void start(int interval_ms, const std::function<void()> &task) {
            if (stopped_ == false) {
                return;
            }
            stopped_ = false;
            std::thread([this, interval_ms, task]() {
                while (!stopped_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                    if (stopped_) {
                        break;
                    }
                    task();
                }
            }).detach();
        }

        void stop() {
            if (stopped_) {
                return;
            }

            stopped_ = true;
        }

    private:
        std::atomic<bool> stopped_;
    };
}
