#include <type_traits>
#include <iostream>

#include <string>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <list>
namespace std_msgs {
    struct Header {
        Header() : frame_id() {
        }

        std::string frame_id;
    };

    struct OccupancyGrid {
        OccupancyGrid() : header() {
        }

        Header header;
    };

}

namespace msgs_traits {
    template<typename M, typename = void>
    struct HasHeader : public std::false_type {};

    template <typename M>
    struct HasHeader<M, decltype((void) M::header)> : std::true_type {};

    template<typename M, typename Enable = void>
    struct FrameId
    {
        static std::string* pointer(M& m) { (void)m; return nullptr; }
        static std::string const* pointer(const M& m) { (void)m; return nullptr; }
    };

    template<typename M>
    struct FrameId<M, typename std::enable_if<HasHeader<M>::value>::type >
    {
        static std::string* pointer(M& m) { return &m.header.frame_id; }
        static std::string const* pointer(const M& m) { return &m.header.frame_id; }
        static std::string value(const M& m) { return m.header.frame_id; }
    };

//    template<>
//    struct HasHeader<std_msgs::Header> : std::false_type {
//    };
//
//    template<>
//    struct HasHeader<std_msgs::OccupancyGrid> : std::true_type {
//    };

//    template<typename M, typename Enable = void>
//    struct TimeStamp
//    {
//        static rclcpp::Time value(const M& m) {
//            (void)m;
//            return rclcpp::Time();
//        }
//    };
//
//    template<typename M>
//    struct TimeStamp<M, typename std::enable_if<HasHeader<M>::value>::type >
//    {
//        static rclcpp::Time value(const M& m) {
//            auto stamp = m.header.stamp;
//            return rclcpp::Time(stamp.sec, stamp.nanosec);
//        }
//    };
}


template<typename T>
void test(T &t) {
//    std::cout << msgs_traits::HasHeader<T>::pointer(t)<<std::endl;
    std::cout << msgs_traits::FrameId<T>::value(t)<<std::endl;
}

int main(int argc, char *argv[]) {
    std_msgs::OccupancyGrid grid;
    grid.header.frame_id = "xxx";
    test<std_msgs::OccupancyGrid>(grid);

    std::list<int> messages_{};
    for (auto it = messages_.begin(); it != messages_.end();) {
        if (*it == 1) {
            std::cout << "--"<<std::endl;
            messages_.erase(it++);
        } else {
            std::cout << "++"<<std::endl;
            it++;
        }
    }
//    std_msgs::Header header;
//    header.frame_id = "xxx";
//    test<std_msgs::Header>(header);

    return 0;
}

