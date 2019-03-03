#pragma once

#include <list>
#include <map>
#include <string>
#include <thread>
#include <memory>
#include <functional>
#include <atomic>

#include "util/logger.h"
#include "util/time_elapsed.h"

using namespace util;

template<typename T>
using Ptr = std::shared_ptr<T>;

template<typename T>
using ConstPtr = std::shared_ptr<T const>;

