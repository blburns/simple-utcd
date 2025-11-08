/*
 * src/core/logger.cpp
 *
 * Copyright 2024 SimpleDaemons
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "simple_utcd/logger.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <syslog.h>
#include <unistd.h>
#if defined(ENABLE_JSON) && ENABLE_JSON
#include <json/json.h>
#endif

namespace simple_utcd {

Logger::Logger()
    : current_level_(LogLevel::INFO)
    , console_enabled_(true)
    , syslog_enabled_(false)
    , json_format_(false)
{
    // Initialize syslog if enabled
    if (syslog_enabled_) {
        openlog("simple-utcd", LOG_PID | LOG_CONS, LOG_DAEMON);
    }
}

Logger::~Logger() {
    if (syslog_enabled_) {
        closelog();
    }

    if (file_stream_ && file_stream_->is_open()) {
        file_stream_->close();
    }
}

void Logger::set_level(LogLevel level) {
    current_level_ = level;
}

void Logger::set_log_file(const std::string& filename) {
    log_file_ = filename;

    if (file_stream_ && file_stream_->is_open()) {
        file_stream_->close();
    }

    file_stream_ = std::make_unique<std::ofstream>(filename, std::ios::app);
    if (!file_stream_->is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
    }
}

void Logger::enable_console(bool enable) {
    console_enabled_ = enable;
}

void Logger::enable_syslog(bool enable) {
    if (enable && !syslog_enabled_) {
        openlog("simple-utcd", LOG_PID | LOG_CONS, LOG_DAEMON);
        syslog_enabled_ = true;
    } else if (!enable && syslog_enabled_) {
        closelog();
        syslog_enabled_ = false;
    }
}

void Logger::set_json_format(bool enable) {
    json_format_ = enable;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < current_level_) {
        return;
    }

    std::lock_guard<std::mutex> lock(log_mutex_);

    std::string log_message;
    
    if (json_format_) {
        log_message = format_json_log(level, message);
    } else {
        std::string timestamp = get_timestamp();
        std::string level_str = level_to_string(level);
        log_message = "[" + timestamp + "] [" + level_str + "] " + message;
    }

    // Console output
    if (console_enabled_) {
        if (level >= LogLevel::ERROR) {
            std::cerr << log_message << std::endl;
        } else {
            std::cout << log_message << std::endl;
        }
    }

    // File output
    if (file_stream_ && file_stream_->is_open()) {
        *file_stream_ << log_message << std::endl;
        file_stream_->flush();
    }

    // Syslog output (always use plain text for syslog)
    if (syslog_enabled_) {
        int priority;
        switch (level) {
            case LogLevel::DEBUG:
                priority = LOG_DEBUG;
                break;
            case LogLevel::INFO:
                priority = LOG_INFO;
                break;
            case LogLevel::WARN:
                priority = LOG_WARNING;
                break;
            case LogLevel::ERROR:
                priority = LOG_ERR;
                break;
            default:
                priority = LOG_INFO;
                break;
        }
        syslog(priority, "%s", message.c_str());
    }
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARN:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return ss.str();
}

std::string Logger::format_json_log(LogLevel level, const std::string& message) {
#if defined(ENABLE_JSON) && ENABLE_JSON
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    // Get ISO 8601 timestamp
    std::stringstream timestamp_ss;
    timestamp_ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    timestamp_ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    
    // Get Unix timestamp for Prometheus compatibility
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    Json::Value log_entry;
    log_entry["timestamp"] = timestamp_ss.str();
    log_entry["unix_timestamp"] = static_cast<int64_t>(unix_timestamp);
    log_entry["level"] = level_to_string(level);
    log_entry["message"] = message;
    log_entry["service"] = "simple-utcd";
    
    // Add Prometheus-compatible metrics fields
    log_entry["metric_type"] = "log";
    log_entry["severity"] = level_to_string(level);
    
    // Add process info for observability
    log_entry["pid"] = static_cast<int>(getpid());
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";  // Compact JSON
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    
    std::ostringstream json_stream;
    writer->write(log_entry, &json_stream);
    
    return json_stream.str();
#else
    // Fallback to plain text if JSON not available
    std::string timestamp = get_timestamp();
    std::string level_str = level_to_string(level);
    return "[" + timestamp + "] [" + level_str + "] " + message;
#endif
}

} // namespace simple_utcd
