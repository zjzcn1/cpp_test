#pragma once

#include <list>
#include <map>
#include <functional>
#include "queue_worker.h"
#include "proto_codec.h"

namespace data_bus {

    class TransportClient {
    public:
        TransportClient() = default;

        ~TransportClient() = default;

        void connect(std::string ip, int port) {

        }

    private:

    };
}
