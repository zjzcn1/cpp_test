#pragma once

#include <chrono>
#include "./logger.h"

using namespace std::chrono;

class ElapsedTime {
private:
    std::string tag_;
    time_point<system_clock> start_;
    
public:
    explicit ElapsedTime(const char *tag): tag_(tag) {
        start_ = system_clock::now();
    }

    double elapsed() {
        duration<double> elapsed_seconds = system_clock::now() - start_;
        Logger::debug("{}: elapsed {}s.", tag_, elapsed_seconds.count());
        return elapsed_seconds.count();
    }

};
