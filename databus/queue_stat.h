#pragma once

namespace databus {

    struct QueueStat {
        std::string topic;
        int queue_size;
        int max_queue_size;
        int callback_size;
    };

    typedef std::shared_ptr<QueueStat> QueueStatPtr;
}