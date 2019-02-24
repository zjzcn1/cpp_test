#pragma once

#include <list>
#include <map>
#include <functional>
#include "queue_worker.h"

namespace data_bus {

    template<typename T>
    class DataBusClient {
    public:
        virtual bool doFilter(ConstPtr<T> data);
    };

    template<typename T>
    using FilterPtr = std::shared_ptr<Filter<T>>;
}
