#pragma once

#include <list>
#include <map>
#include <string>
#include <thread>
#include <memory>
#include <functional>
#include <atomic>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include "util/logger.h"
#include "util/time_elapsed.h"

namespace data_bus {

    using namespace util;

    // proto message
    using ProtoMessage = google::protobuf::Message;

    template<typename T>
    using Ptr = std::shared_ptr<T>;

    template<typename T>
    using ConstPtr = std::shared_ptr<T const>;

    // callback
    template<typename T>
    using Callback = std::function<void(ConstPtr<T>)>;

}
