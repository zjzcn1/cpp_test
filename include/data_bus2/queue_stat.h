#pragma once

#include "util/common.h"

namespace data_bus {

    struct CallbackStat {
        std::string topic{};
        std::string callback_name{};
        long callback_id{0};
        int queue_size{0};
        int max_queue_size{0};
        std::size_t incoming_count{0};
        std::size_t success_count{0};
        std::size_t dropped_count{0};
        double cost_time_sec{0};
        double total_time_sec{0};

        std::string toString() {
            return "{topic=" + topic +
                   ", callback_name=" + callback_name +
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

    struct QueueStat {
        std::string topic{};
        std::size_t publish_count{0};

        std::list<Ptr<CallbackStat>> callback_stats;
    };

}