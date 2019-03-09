#pragma once

#include <string>
#include <memory>

namespace data_bus {

    struct QueueStat {
        QueueStat() : topic(""), subscriber_id(0), queue_size(0), max_queue_size(0), filter_size(0), incoming_count(0),
                      dropped_count(0), success_count(0) {
        }

        std::string topic;
        long subscriber_id;
        int queue_size;
        int max_queue_size;
        int filter_size;
        uint64_t incoming_count;
        uint64_t success_count;
        uint64_t dropped_count;
    };

    typedef std::shared_ptr<QueueStat> QueueStatPtr;
}