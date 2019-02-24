#pragma once

#include <string>
#include <memory>

namespace data_bus {

    struct QueueStat {
        QueueStat() : topic(""), subscriber_name(""), callback_id(0), queue_size(0), max_queue_size(0),
                      incoming_count(0),
                      dropped_count(0), success_count(0) {
        }

        std::string topic;
        std::string subscriber_name;
        long callback_id;
        int queue_size;
        int max_queue_size;
        std::size_t incoming_count;
        std::size_t success_count;
        std::size_t dropped_count;
        double cost_time_sec;
        double total_time_sec;

        std::string toString() {
            return "{topic=" + topic +
                   ", subscriber_name=" + subscriber_name +
                   ", callback_id=" + std::to_string(callback_id) +
                   ", queue_size=" + std::to_string(queue_size) +
                   ", max_queue_size=" + std::to_string(max_queue_size) +
                   ", incoming_count=" + std::to_string(incoming_count) +
                   ", success_count=" + std::to_string(success_count) +
                   ", dropped_count=" + std::to_string(dropped_count) +
                   ", cost_time_sec=" + std::to_string(cost_time_sec) +
                   ", total_time_sec=" + std::to_string(total_time_sec) + "}";
        }
    };

    typedef std::shared_ptr<QueueStat> QueueStatPtr;
}