#ifndef COSTMAP_MAP_SERVER_H_
#define COSTMAP_MAP_SERVER_H_

#include <string>
#include <vector>

#include "OccupancyGrid.h"
#include "MapMetaData.h"
#include "yaml-cpp/yaml.h"

namespace walle
{

    class MapLoader
    {
    public:
        explicit MapLoader(std::string map_yaml_file);
        MapLoader() = delete;

        OccupancyGridConstPtr getMap() const;

        // Load the image and generate an OccupancyGrid
        void loadMapFromFile(const std::string & map_name);

    private:
        // Conversion parameters read from the YAML document
        double resolution_;
        std::vector<double> origin_;
        double free_thresh_;
        double occupied_thresh_;
        enum MapMode { TRINARY, SCALE, RAW };
        MapMode mode_;
        int negate_;
        std::string map_filename_;

        OccupancyGridPtr msg_;

        // The frame ID used in the returned OccupancyGrid message
        static const char * frame_id_;
    };

}  // namespace end

#endif