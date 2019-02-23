#pragma once

#include "yaml-cpp/yaml.h"

namespace util {

    class Config {
    private:
        YAML::Node yaml_node_;

    public:
        Config() = default;

        explicit Config(const std::string &filename) {
            yaml_node_ = YAML::LoadFile(filename);
        }

        bool has(const std::string &param_name) const {
            return yaml_node_[param_name];
        }

        template<typename T>
        bool param(const std::string &param_name, T &param_val) const {
            if (has(param_name)) {
                param_val = yaml_node_[param_name].as<T>();
                return true;
            }
            return false;
        }

        template<typename T>
        bool param(const std::string &param_name, T &param_val, const T &default_val) const {
            if (param(param_name, param_val)) {
                return true;
            }
            param_val = default_val;
            return false;
        }


        bool child(const std::string &param_name, Config &config) const {
            if (has(param_name)) {
                config.yaml_node_ = yaml_node_[param_name];
                return true;
            }
            return false;
        }

        bool list(const std::string &param_name, std::list<Config> &list) const {
            if (has(param_name)) {
                for (YAML::Node node : yaml_node_[param_name]) {
                    Config config;
                    config.yaml_node_ = node;
                    list.push_back(config);
                }
                return true;
            }
            return false;
        }
    };
}

