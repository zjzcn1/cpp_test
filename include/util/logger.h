#pragma once

#include <string.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <boost/filesystem.hpp>

namespace util {

    static std::string LOG_DIR = "logs";

    class Logger {
    private:
        std::shared_ptr<spdlog::logger> logger_;
        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink;
        std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> file_sink;
        std::shared_ptr<spdlog::sinks::daily_file_sink_mt> daily_sink;

    public:
        enum Level {
            DEBUG,
            INFO,
            WARN,
            ERROR
        };

        Logger() {
            console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

            logger_ = std::shared_ptr<spdlog::logger>(new spdlog::logger("Logger", {console_sink}));
            spdlog::register_logger(logger_);
            spdlog::flush_every(std::chrono::seconds(1));
            logger_->flush_on(spdlog::level::warn);
        }

        static Logger *instance() {
            static Logger instance;
            return &instance;
        }

        static void setRotatingSink(const std::string &file_name) {
            boost::filesystem::path dir(LOG_DIR);
            if (!boost::filesystem::exists(dir)) {
                create_directory(dir);
            }
            instance()->file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(LOG_DIR + "/" + file_name,
                                                                                           20 * 1204 * 1024, 3);
            instance()->file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
            instance()->logger_->sinks().push_back(instance()->file_sink);
        }

        static void setDailySink(const std::string &file_name) {
            boost::filesystem::path dir(LOG_DIR);
            if (!boost::filesystem::exists(dir)) {
                create_directory(dir);
            }
            instance()->daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(LOG_DIR + "/" + file_name, 0, 0);
            instance()->daily_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
            instance()->logger_->sinks().push_back(instance()->daily_sink);
        }

        static void setLevel(Level level) {
            switch (level) {
                case DEBUG:
                    instance()->logger_->set_level(spdlog::level::debug);
                    break;
                case INFO:
                    instance()->logger_->set_level(spdlog::level::info);
                    break;
                case WARN:
                    instance()->logger_->set_level(spdlog::level::warn);
                    break;
                case ERROR:
                    instance()->logger_->set_level(spdlog::level::err);
                    break;
                default:
                    instance()->logger_->set_level(spdlog::level::info);
            }
        }

        template<typename... Args>
        static void debug(std::string tag, const char *fmt, const Args &... args) {
            log(spdlog::level::debug, tag, fmt, args...);
        }

        template<typename... Args>
        static void info(std::string tag, const char *fmt, const Args &... args) {
            log(spdlog::level::info, tag, fmt, args...);
        }

        template<typename... Args>
        static void warn(std::string tag, const char *fmt, const Args &... args) {
            log(spdlog::level::warn, tag, fmt, args...);
        }

        template<typename... Args>
        static void error(std::string tag, const char *fmt, const Args &... args) {
            log(spdlog::level::err, tag, fmt, args...);
        }

    private:
        template<typename... Args>
        static void log(spdlog::level::level_enum lvl, std::string tag, const char *fmt, const Args &... args) {
            std::string msg = "[" + tag + "] " + fmt;
            instance()->logger_->log(lvl, msg.data(), args...);
        }

    };
}
