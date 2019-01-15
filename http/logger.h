#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace http_server {
    enum LoggerLevel {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    class Logger {
    private:
        std::shared_ptr <spdlog::logger> logger_;

    public:
        Logger() {
            logger_ = spdlog::stdout_color_mt("walle");
            spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        }

        static Logger *instance() {
            static Logger instance;
            return &instance;
        }

        static void setLevel(LoggerLevel level) {
            switch (level) {
                case DEBUG:
                    spdlog::set_level(spdlog::level::debug);
                    break;
                case INFO:
                    spdlog::set_level(spdlog::level::info);
                    break;
                case WARN:
                    spdlog::set_level(spdlog::level::warn);
                    break;
                case ERROR:
                    spdlog::set_level(spdlog::level::err);
                    break;
                default:
                    spdlog::set_level(spdlog::level::info);
            }

        }

        template<typename... Args>
        static void debug(const char *fmt, const Args &... args) {
            instance()->logger_->debug(fmt, args...);
        }

        template<typename... Args>
        static void info(const char *fmt, const Args &... args) {
            instance()->logger_->info(fmt, args...);
        }

        template<typename... Args>
        static void warn(const char *fmt, const Args &... args) {
            instance()->logger_->warn(fmt, args...);
        }

        template<typename... Args>
        static void error(const char *fmt, const Args &... args) {
            instance()->logger_->error(fmt, args...);
        }

        template<typename... Args>
        static void critical(const char *fmt, const Args &... args) {
            instance()->logger_->critical(fmt, args...);
        }
    };
}