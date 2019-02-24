#pragma once

#include <chrono>
#include "./logger.h"

using namespace std::chrono;

namespace util {
    class TimeElapsed {
    private:
        std::string tag_;
        time_point<system_clock> start_;

    public:
        TimeElapsed() : tag_("") {
            start_ = system_clock::now();
        }

        explicit TimeElapsed(const char *tag) : tag_(tag) {
            start_ = system_clock::now();
        }

        double elapsed() {
            duration<double> elapsed_seconds = system_clock::now() - start_;
            if (tag_ != "") {
                Logger::debug(tag_, "elapsed time {}s.", elapsed_seconds.count());
            }
            return elapsed_seconds.count();
        }

    };
}
