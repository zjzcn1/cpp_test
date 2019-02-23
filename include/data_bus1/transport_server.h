#pragma once

#include <list>
#include <map>
#include <functional>
#include "queue_worker.h"
#include "proto_codec.h"

namespace data_bus {

    class TransportServer {
    public:
        TransportServer() = default;

        ~TransportServer() = default;

        void listen(int port) {

        }

    private:

    };
}
