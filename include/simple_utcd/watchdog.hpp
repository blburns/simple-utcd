/*
 * includes/simple_utcd/watchdog.hpp
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

#pragma once

#include <string>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <cstdint>

namespace simple_utcd {

/**
 * @brief Restart policy
 */
enum class RestartPolicy {
    NEVER,           // Never restart
    ON_FAILURE,      // Restart only on failure
    ALWAYS,          // Always restart
    ON_DEMAND        // Restart only when explicitly requested
};

/**
 * @brief Watchdog configuration
 */
struct WatchdogConfig {
    bool enabled;
    uint64_t check_interval_seconds;
    uint64_t max_failures;
    uint64_t restart_delay_seconds;
    RestartPolicy restart_policy;
    bool auto_recovery;
    uint64_t recovery_timeout_seconds;
    
    WatchdogConfig() 
        : enabled(false)
        , check_interval_seconds(30)
        , max_failures(3)
        , restart_delay_seconds(5)
        , restart_policy(RestartPolicy::ON_FAILURE)
        , auto_recovery(true)
        , recovery_timeout_seconds(60)
    {}
};

/**
 * @brief Service status callback
 */
using HealthCheckCallback = std::function<bool()>;
using RestartCallback = std::function<bool()>;
using ShutdownCallback = std::function<void()>;

/**
 * @brief Watchdog process for automatic service recovery
 */
class Watchdog {
public:
    Watchdog();
    ~Watchdog();

    // Configuration
    void set_config(const WatchdogConfig& config);
    WatchdogConfig get_config() const { return config_; }
    
    // Callbacks
    void set_health_check_callback(HealthCheckCallback callback);
    void set_restart_callback(RestartCallback callback);
    void set_shutdown_callback(ShutdownCallback callback);
    
    // Control
    bool start();
    void stop();
    bool is_running() const { return running_; }
    
    // Manual operations
    bool trigger_restart();
    void record_failure();
    void record_success();
    
    // Statistics
    uint64_t get_failure_count() const { return failure_count_; }
    uint64_t get_restart_count() const { return restart_count_; }
    uint64_t get_last_check_time() const;
    bool is_service_healthy() const { return service_healthy_; }

private:
    WatchdogConfig config_;
    std::atomic<bool> running_;
    std::atomic<bool> service_healthy_;
    std::atomic<uint64_t> failure_count_;
    std::atomic<uint64_t> restart_count_;
    std::atomic<uint64_t> consecutive_failures_;
    std::chrono::system_clock::time_point last_check_time_;
    std::chrono::system_clock::time_point last_restart_time_;
    
    std::thread watchdog_thread_;
    mutable std::mutex config_mutex_;
    
    HealthCheckCallback health_check_callback_;
    RestartCallback restart_callback_;
    ShutdownCallback shutdown_callback_;
    
    void watchdog_loop();
    bool perform_health_check();
    bool should_restart() const;
    bool perform_restart();
    void reset_failure_count();
    
    std::chrono::system_clock::time_point now() const;
    uint64_t seconds_since(const std::chrono::system_clock::time_point& time) const;
};

} // namespace simple_utcd

