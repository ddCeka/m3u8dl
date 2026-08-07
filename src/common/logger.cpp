#include "common/logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace m3u8dl {

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::init(LogLevel level, const std::string& log_file) {
    std::vector<spdlog::sink_ptr> sinks;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(console_sink);

    if (!log_file.empty()) {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
        sinks.push_back(file_sink);
    }

    logger_ = std::make_shared<spdlog::logger>("m3u8dl", sinks.begin(), sinks.end());
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    set_level(level);
    spdlog::register_logger(logger_);
}

void Logger::debug(const std::string& msg) {
    if (logger_) logger_->debug(msg);
}

void Logger::info(const std::string& msg) {
    if (logger_) logger_->info(msg);
}

void Logger::warn(const std::string& msg) {
    if (logger_) logger_->warn(msg);
}

void Logger::error(const std::string& msg) {
    if (logger_) logger_->error(msg);
}

void Logger::set_level(LogLevel level) {
    if (!logger_) return;

    switch (level) {
        case LogLevel::DEBUG:
            logger_->set_level(spdlog::level::debug);
            break;
        case LogLevel::INFO:
            logger_->set_level(spdlog::level::info);
            break;
        case LogLevel::WARN:
            logger_->set_level(spdlog::level::warn);
            break;
        case LogLevel::ERROR:
            logger_->set_level(spdlog::level::err);
            break;
        case LogLevel::OFF:
            logger_->set_level(spdlog::level::off);
            break;
    }
}

}
