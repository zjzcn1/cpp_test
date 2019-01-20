#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <boost/filesystem.hpp>

namespace http_server {
    class Logger {
    private:
        std::shared_ptr<spdlog::logger> logger_;
        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink;
        std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> file_sink;
    public:
        enum Level {
            DEBUG,
            INFO,
            WARN,
            ERROR
        };
        enum Sink {
            CONSOLE,
            FILE,
            ALL
        };
        Logger() {
            console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

            boost::filesystem::path dir("logs");
            if (!boost::filesystem::exists(dir)) {
                create_directory(dir);
            }
            file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/walle.log", 20*1204*1024, 3);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
            logger_ = std::shared_ptr<spdlog::logger>(new spdlog::logger("multi_sink", {console_sink, file_sink}));
            spdlog::register_logger(logger_);
            spdlog::flush_every(std::chrono::seconds(1));
            logger_->flush_on(spdlog::level::warn);
        }

        static Logger *instance() {
            static Logger instance;
            return &instance;
        }

        static void setSink(Sink sink) {
            instance()->logger_->sinks().clear();
            switch (sink) {
                case CONSOLE:
                    instance()->logger_->sinks().push_back(instance()->console_sink);
                    break;
                case FILE:
                    instance()->logger_->sinks().push_back(instance()->file_sink);
                    break;
                case ALL:
                default:
                    instance()->logger_->sinks().push_back(instance()->console_sink);
                    instance()->logger_->sinks().push_back(instance()->file_sink);
            }
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