#include "map_loader.h"

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include "win_patch/libgen.h"
#else

#include <libgen.h>

#endif

#include "map_loader.h"

#include "SDL/SDL_image.h"
#include "../http/logger.h"

#define MAP_IDX(sx, i, j) ((sx) * (j) + (i))
using namespace http_server;

namespace walle {

    const char *MapLoader::frame_id_ = "map";

    MapLoader::MapLoader(std::string map_yaml_file)
            : origin_(3) {
        YAML::Node doc = YAML::LoadFile(map_yaml_file);
        try {
            resolution_ = doc["resolution"].as<double>();
        } catch (YAML::Exception) {
            throw std::runtime_error("The map does not contain a resolution tag or it is invalid");
        }

        try {
            origin_[0] = doc["origin"][0].as<double>();
            origin_[1] = doc["origin"][1].as<double>();
            origin_[2] = doc["origin"][2].as<double>();
        } catch (YAML::Exception) {
            throw std::runtime_error("The map does not contain an origin tag or it is invalid");
        }

        try {
            free_thresh_ = doc["free_thresh"].as<double>();
        } catch (YAML::Exception) {
            throw std::runtime_error("The map does not contain a free_thresh tag or it is invalid");
        }

        try {
            occupied_thresh_ = doc["occupied_thresh"].as<double>();
        } catch (YAML::Exception) {
            throw std::runtime_error("The map does not contain an occupied_thresh tag or it is invalid");
        }

        std::string mode_str;
        try {
            mode_str = doc["mode"].as<std::string>();

            // Convert the string version of the mode name to one of the enumeration values
            if (mode_str == "trinary") {
                mode_ = TRINARY;
            } else if (mode_str == "scale") {
                mode_ = SCALE;
            } else if (mode_str == "raw") {
                mode_ = RAW;
            } else {
                Logger::warn("Mode parameter not recognized: '{}', using default value (trinary)", mode_str);
                mode_ = TRINARY;
            }
        } catch (YAML::Exception &) {
            Logger::warn("Mode parameter not set, using default value (trinary)");
            mode_ = TRINARY;
        }

        try {
            negate_ = doc["negate"].as<int>();
        } catch (YAML::Exception) {
            throw std::runtime_error("The map does not contain a negate tag or it is invalid");
        }

        try {
            map_filename_ = doc["image"].as<std::string>();
            if (map_filename_.size() == 0) {
                throw std::runtime_error("The image tag cannot be an empty string");
            }
            if (map_filename_[0] != '/') {
                // dirname can modify what you pass it
                char *fname_copy = strdup(map_yaml_file.c_str());
                map_filename_ = std::string(dirname(fname_copy)) + '/' + map_filename_;
                free(fname_copy);
            }
        } catch (YAML::Exception) {
            throw std::runtime_error("The map does not contain a image tag or it is invalid");
        }

        Logger::debug("resolution: {}", resolution_);
        Logger::debug("origin[0]: {}", origin_[0]);
        Logger::debug("origin[1]: {}", origin_[1]);
        Logger::debug("origin[2]: {}", origin_[2]);
        Logger::debug("free_thresh: {}", free_thresh_);
        Logger::debug("occupied_thresh: {}", occupied_thresh_);
        Logger::debug("mode_str: {}", mode_str.c_str());
        Logger::debug("mode: {}", mode_);
        Logger::debug("negate: {}", negate_);
        Logger::debug("image: {}", map_filename_);

        loadMapFromFile(map_filename_);
    }

    OccupancyGridConstPtr MapLoader::getMap() const {
        return msg_;
    }

    void MapLoader::loadMapFromFile(const std::string &map_name) {
        // Load the image using SDL.  If we get NULL back, the image load failed.
        SDL_Surface *img;
        if (!(img = IMG_Load(map_name.c_str()))) {
            std::string errmsg = std::string("failed to open image file \"") +
                                 map_name + std::string("\": ") + IMG_GetError();
            throw std::runtime_error(errmsg);
        }

        // Copy the image data into the map structure
        msg_ = OccupancyGridPtr(new OccupancyGrid());
        msg_->info.width = img->w;
        msg_->info.height = img->h;
        msg_->info.resolution = resolution_;
//        msg_->info.origin.position.x = origin_[0];
//        msg_->info.origin.position.y = origin_[1];
//        msg_->info.origin.position.z = 0.0;

//        Eigen::Vector3d eulerAngle(origin_[2], 0, 0);
//        Eigen::AngleAxisd yawAngle(Eigen::AngleAxisd(eulerAngle(0), Eigen::Vector3d::UnitZ()));
//        Eigen::AngleAxisd pitchAngle(Eigen::AngleAxisd(eulerAngle(1), Eigen::Vector3d::UnitY()));
//        Eigen::AngleAxisd rollAngle(Eigen::AngleAxisd(eulerAngle(2), Eigen::Vector3d::UnitX()));
//
//        Eigen::Quaterniond q = yawAngle * pitchAngle * rollAngle;
//        msg_->info.origin.orientation.x = q.x();
//        msg_->info.origin.orientation.y = q.y();
//        msg_->info.origin.orientation.z = q.z();
//        msg_->info.origin.orientation.w = q.w();

//        btQuaternion q;
//        // setEulerZYX(yaw, pitch, roll)
//        q.setEulerZYX(origin_[2], 0, 0);
//        msg_->info.origin.orientation.x = q.x();
//        msg_->info.origin.orientation.y = q.y();
//        msg_->info.origin.orientation.z = q.z();
//        msg_->info.origin.orientation.w = q.w();

        // Allocate space to hold the data
        msg_->data.resize(msg_->info.width * msg_->info.height);

        // Get values that we'll need to iterate through the pixels
        int rowstride = img->pitch;
        int n_channels = img->format->BytesPerPixel;

        // NOTE: Trinary mode still overrides here to preserve existing behavior.
        // Alpha will be averaged in with color channels when using trinary mode.
        int avg_channels;
        if (mode_ == TRINARY || !img->format->Amask) {
            avg_channels = n_channels;
        } else {
            avg_channels = n_channels - 1;
        }

        // Copy pixel data into the map structure
        unsigned char *pixels = (unsigned char *) (img->pixels);
        int color_sum;
        for (unsigned int j = 0; j < msg_->info.height; j++) {
            for (unsigned int i = 0; i < msg_->info.width; i++) {
                // Compute mean of RGB for this pixel
                unsigned char *p = pixels + j * rowstride + i * n_channels;
                color_sum = 0;
                for (int k = 0; k < avg_channels; k++) {
                    color_sum += *(p + (k));
                }
                double color_avg = color_sum / static_cast<double>(avg_channels);

                int alpha;
                if (n_channels == 1) {
                    alpha = 1;
                } else {
                    alpha = *(p + n_channels - 1);
                }

                if (negate_) {
                    color_avg = 255 - color_avg;
                }

                unsigned char value;
                if (mode_ == RAW) {
                    value = color_avg;
                    msg_->data[MAP_IDX(msg_->info.width, i, msg_->info.height - j - 1)] = value;
                    continue;
                }

                // If negate is true, we consider blacker pixels free, and whiter
                // pixels free.  Otherwise, it's vice versa.
                double occ = (255 - color_avg) / 255.0;

                // Apply thresholds to RGB means to determine occupancy values for
                // map.  Note that we invert the graphics-ordering of the pixels to
                // produce a map with cell (0,0) in the lower-left corner.
                if (occ > occupied_thresh_) {
                    value = +100;
                } else if (occ < free_thresh_) {
                    value = 0;
                } else if (mode_ == TRINARY || alpha < 1.0) {
                    value = -1;
                } else {
                    double ratio = (occ - free_thresh_) / (occupied_thresh_ - free_thresh_);
                    value = 99 * ratio;
                }

                msg_->data[MAP_IDX(msg_->info.width, i, msg_->info.height - j - 1)] = value;
            }
        }

        SDL_FreeSurface(img);

//        msg_->info.map_load_time = Time::now();
//        msg_->header.frame_id = frame_id_;
//        msg_->header.stamp = Time::now();

        Logger::debug("Read map {}: {} X {} map @ {} m/cell, size {}.",
                      map_name,
                      msg_->info.width,
                      msg_->info.height,
                      msg_->info.resolution, msg_->data.size());
    }

}