// Generated by gencpp from file nav_msgs/OccupancyGrid.msg
// DO NOT EDIT!


#ifndef NAV_MSGS_MESSAGE_OCCUPANCYGRID_H
#define NAV_MSGS_MESSAGE_OCCUPANCYGRID_H


#include <string>
#include <vector>
#include <map>

#include <MapMetaData.h>

namespace walle {
    struct OccupancyGrid {
        OccupancyGrid()
                : info(), data() {
        }

        MapMetaData info;

        std::vector<int8_t> data;


        typedef std::shared_ptr<OccupancyGrid> Ptr;
        typedef std::shared_ptr<OccupancyGrid const> ConstPtr;

    }; // struct OccupancyGrid_

    typedef std::shared_ptr<OccupancyGrid> OccupancyGridPtr;
    typedef std::shared_ptr<OccupancyGrid const> OccupancyGridConstPtr;

} // namespace ros

#endif // NAV_MSGS_MESSAGE_OCCUPANCYGRID_H
