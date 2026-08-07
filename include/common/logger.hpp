#pragma once

#include <spdlog/spdlog.h>
#include <string>
#include <memory>

namespace m3u8dl {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    OFF
};

class Logger {
public:
    static void init(LogLevel level, const std::string& log_file = "");

    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);

    static void set_level(LogLevel level);

private:
    static std::shared_ptr<spdlog::logger> logger_;
};

}
