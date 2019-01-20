#pragma once


#include <string>
#include <vector>
#include <map>

#include <std_msgs/Header.h>
#include <geometry_msgs/PoseStamped.h>

namespace walle {
    template<typename T>
    struct Message {
        Message() : op(), id() {
        }

        std::string op;
        std::string id;
        T data;
    };

}

