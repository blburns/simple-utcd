/*
 * src/core/watchdog.cpp
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

#include "simple_utcd/watchdog.hpp"
#include <thread>
#include <chrono>

namespace simple_utcd {

Watchdog::Watchdog()
    : running_(false)
    , service_healthy_(true)
    , failure_count_(0)
    , restart_count_(0)
    , consecutive_failures_(0)
    , last_check_time_(std::chrono::system_clock::now())
    , last_restart_time_(std::chrono::system_clock::now())
{
}

Watchdog::~Watchdog() {
    stop();
}

void Watchdog::set_config(const WatchdogConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
}

void Watchdog::set_health_check_callback(HealthCheckCallback callback) {
    health_check_callback_ = callback;
}

void Watchdog::set_restart_callback(RestartCallback callback) {
    restart_callback_ = callback;
}

void Watchdog::set_shutdown_callback(ShutdownCallback callback) {
    shutdown_callback_ = callback;
}

bool Watchdog::start() {
    if (running_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (!config_.enabled) {
        return false;
    }
    
    running_ = true;
    service_healthy_ = true;
    failure_count_ = 0;
    consecutive_failures_ = 0;
    last_check_time_ = now();
    
    watchdog_thread_ = std::thread(&Watchdog::watchdog_loop, this);
    
    return true;
}

void Watchdog::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }
}

bool Watchdog::trigger_restart() {
    if (!running_) {
        return false;
    }
    
    return perform_restart();
}

void Watchdog::record_failure() {
    failure_count_++;
    consecutive_failures_++;
    service_healthy_ = false;
}

void Watchdog::record_success() {
    service_healthy_ = true;
    reset_failure_count();
}

uint64_t Watchdog::get_last_check_time() const {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now() - last_check_time_).count();
    return static_cast<uint64_t>(std::max(static_cast<decltype(elapsed)>(0), elapsed));
}

void Watchdog::watchdog_loop() {
    while (running_) {
        std::this_thread::sleep_for(
            std::chrono::seconds(config_.check_interval_seconds));
        
        if (!running_) {
            break;
        }
        
        last_check_time_ = now();
        
        // Perform health check
        bool healthy = perform_health_check();
        
        if (!healthy) {
            record_failure();
            
            // Check if we should restart
            if (should_restart()) {
                if (config_.auto_recovery) {
                    perform_restart();
                }
            }
        } else {
            record_success();
        }
    }
}

bool Watchdog::perform_health_check() {
    if (health_check_callback_) {
        return health_check_callback_();
    }
    
    // Default: assume healthy if no callback
    return true;
}

bool Watchdog::should_restart() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    switch (config_.restart_policy) {
        case RestartPolicy::NEVER:
            return false;
        case RestartPolicy::ON_DEMAND:
            return false;  // Only manual restart
        case RestartPolicy::ALWAYS:
            return !service_healthy_;
        case RestartPolicy::ON_FAILURE:
            return consecutive_failures_ >= config_.max_failures;
        default:
            return false;
    }
}

bool Watchdog::perform_restart() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Check if we're within recovery timeout
    if (seconds_since(last_restart_time_) < config_.recovery_timeout_seconds) {
        // Too soon after last restart, wait
        return false;
    }
    
    // Wait for restart delay
    if (config_.restart_delay_seconds > 0) {
        std::this_thread::sleep_for(
            std::chrono::seconds(config_.restart_delay_seconds));
    }
    
    if (!running_) {
        return false;
    }
    
    // Call restart callback if available
    if (restart_callback_) {
        bool success = restart_callback_();
        if (success) {
            restart_count_++;
            last_restart_time_ = now();
            reset_failure_count();
            return true;
        }
    }
    
    return false;
}

void Watchdog::reset_failure_count() {
    consecutive_failures_ = 0;
}

std::chrono::system_clock::time_point Watchdog::now() const {
    return std::chrono::system_clock::now();
}

uint64_t Watchdog::seconds_since(const std::chrono::system_clock::time_point& time) const {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now() - time).count();
    return static_cast<uint64_t>(std::max(static_cast<decltype(elapsed)>(0), elapsed));
}

} // namespace simple_utcd

