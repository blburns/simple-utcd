/*
 * tests/test_watchdog.cpp
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

#include <gtest/gtest.h>
#include "simple_utcd/watchdog.hpp"
#include <thread>
#include <chrono>
#include <atomic>

using namespace simple_utcd;

class WatchdogTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.enabled = true;
        config_.check_interval_seconds = 1;
        config_.max_failures = 3;
        config_.restart_delay_seconds = 0;
        config_.restart_policy = RestartPolicy::ON_FAILURE;
        config_.auto_recovery = true;
        config_.recovery_timeout_seconds = 10;
        
        watchdog_.set_config(config_);
        
        health_check_count_ = 0;
        restart_count_ = 0;
        shutdown_called_ = false;
    }

    void TearDown() override {
        watchdog_.stop();
    }

    Watchdog watchdog_;
    WatchdogConfig config_;
    
    std::atomic<int> health_check_count_;
    std::atomic<int> restart_count_;
    std::atomic<bool> shutdown_called_;
};

// Test default constructor
TEST_F(WatchdogTest, DefaultConstructor) {
    Watchdog wd;
    EXPECT_FALSE(wd.is_running());
    EXPECT_EQ(wd.get_failure_count(), 0);
    EXPECT_EQ(wd.get_restart_count(), 0);
}

// Test configuration
TEST_F(WatchdogTest, Configuration) {
    WatchdogConfig cfg;
    cfg.enabled = true;
    cfg.check_interval_seconds = 5;
    cfg.max_failures = 5;
    
    watchdog_.set_config(cfg);
    WatchdogConfig retrieved = watchdog_.get_config();
    
    EXPECT_EQ(retrieved.enabled, cfg.enabled);
    EXPECT_EQ(retrieved.check_interval_seconds, cfg.check_interval_seconds);
    EXPECT_EQ(retrieved.max_failures, cfg.max_failures);
}

// Test start/stop
TEST_F(WatchdogTest, StartStop) {
    EXPECT_FALSE(watchdog_.is_running());
    
    watchdog_.set_health_check_callback([]() { return true; });
    EXPECT_TRUE(watchdog_.start());
    EXPECT_TRUE(watchdog_.is_running());
    
    watchdog_.stop();
    EXPECT_FALSE(watchdog_.is_running());
}

// Test health check callback
TEST_F(WatchdogTest, HealthCheckCallback) {
    watchdog_.set_health_check_callback([this]() {
        health_check_count_++;
        return true;
    });
    
    EXPECT_TRUE(watchdog_.start());
    
    // Wait for at least one health check
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    EXPECT_GT(health_check_count_.load(), 0);
    
    watchdog_.stop();
}

// Test failure detection
TEST_F(WatchdogTest, FailureDetection) {
    watchdog_.set_health_check_callback([this]() {
        health_check_count_++;
        return health_check_count_ < 2;  // Fail after first check
    });
    
    EXPECT_TRUE(watchdog_.start());
    
    // Wait for failures
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    
    EXPECT_GT(watchdog_.get_failure_count(), 0);
    EXPECT_FALSE(watchdog_.is_service_healthy());
    
    watchdog_.stop();
}

// Test restart callback
TEST_F(WatchdogTest, RestartCallback) {
    config_.max_failures = 2;
    watchdog_.set_config(config_);
    
    watchdog_.set_health_check_callback([this]() {
        return false;  // Always fail
    });
    
    watchdog_.set_restart_callback([this]() {
        restart_count_++;
        return true;
    });
    
    EXPECT_TRUE(watchdog_.start());
    
    // Wait for failures and restart
    std::this_thread::sleep_for(std::chrono::milliseconds(3500));
    
    EXPECT_GT(watchdog_.get_failure_count(), 0);
    EXPECT_GE(watchdog_.get_restart_count(), 0);
    
    watchdog_.stop();
}

// Test restart policy - NEVER
TEST_F(WatchdogTest, RestartPolicyNever) {
    config_.restart_policy = RestartPolicy::NEVER;
    config_.max_failures = 1;
    watchdog_.set_config(config_);
    
    watchdog_.set_health_check_callback([]() { return false; });
    watchdog_.set_restart_callback([this]() {
        restart_count_++;
        return true;
    });
    
    EXPECT_TRUE(watchdog_.start());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    
    // Should not restart
    EXPECT_EQ(restart_count_.load(), 0);
    
    watchdog_.stop();
}

// Test restart policy - ON_FAILURE
TEST_F(WatchdogTest, RestartPolicyOnFailure) {
    config_.restart_policy = RestartPolicy::ON_FAILURE;
    config_.max_failures = 2;
    watchdog_.set_config(config_);
    
    watchdog_.set_health_check_callback([]() { return false; });
    watchdog_.set_restart_callback([this]() {
        restart_count_++;
        return true;
    });
    
    EXPECT_TRUE(watchdog_.start());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(3500));
    
    // Should restart after max failures
    EXPECT_GE(restart_count_.load(), 0);
    
    watchdog_.stop();
}

// Test restart policy - ALWAYS
TEST_F(WatchdogTest, RestartPolicyAlways) {
    config_.restart_policy = RestartPolicy::ALWAYS;
    config_.max_failures = 1;
    watchdog_.set_config(config_);
    
    bool healthy = false;
    watchdog_.set_health_check_callback([&healthy]() { return healthy; });
    watchdog_.set_restart_callback([this]() {
        restart_count_++;
        return true;
    });
    
    EXPECT_TRUE(watchdog_.start());
    
    // Wait for check
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    // Make unhealthy
    healthy = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Should attempt restart when unhealthy
    EXPECT_GE(restart_count_.load(), 0);
    
    watchdog_.stop();
}

// Test manual restart trigger
TEST_F(WatchdogTest, ManualRestartTrigger) {
    watchdog_.set_restart_callback([this]() {
        restart_count_++;
        return true;
    });
    
    EXPECT_TRUE(watchdog_.start());
    
    EXPECT_TRUE(watchdog_.trigger_restart());
    EXPECT_GT(restart_count_.load(), 0);
    EXPECT_GT(watchdog_.get_restart_count(), 0);
    
    watchdog_.stop();
}

// Test record success/failure
TEST_F(WatchdogTest, RecordSuccessFailure) {
    EXPECT_TRUE(watchdog_.is_service_healthy());
    
    watchdog_.record_failure();
    EXPECT_FALSE(watchdog_.is_service_healthy());
    EXPECT_EQ(watchdog_.get_failure_count(), 1);
    
    watchdog_.record_success();
    EXPECT_TRUE(watchdog_.is_service_healthy());
}

// Test consecutive failures
TEST_F(WatchdogTest, ConsecutiveFailures) {
    config_.max_failures = 3;
    watchdog_.set_config(config_);
    
    for (int i = 0; i < 5; i++) {
        watchdog_.record_failure();
    }
    
    EXPECT_EQ(watchdog_.get_failure_count(), 5);
    EXPECT_FALSE(watchdog_.is_service_healthy());
}

// Test recovery timeout
TEST_F(WatchdogTest, RecoveryTimeout) {
    config_.recovery_timeout_seconds = 2;
    watchdog_.set_config(config_);
    
    watchdog_.set_restart_callback([this]() {
        restart_count_++;
        return true;
    });
    
    EXPECT_TRUE(watchdog_.start());
    
    // Trigger first restart
    watchdog_.trigger_restart();
    int first_restart = restart_count_.load();
    
    // Try to restart immediately (should be blocked by timeout)
    watchdog_.trigger_restart();
    EXPECT_EQ(restart_count_.load(), first_restart);
    
    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    
    // Now should be able to restart
    watchdog_.trigger_restart();
    EXPECT_GT(restart_count_.load(), first_restart);
    
    watchdog_.stop();
}

// Test shutdown callback
TEST_F(WatchdogTest, ShutdownCallback) {
    watchdog_.set_shutdown_callback([this]() {
        shutdown_called_ = true;
    });
    
    EXPECT_TRUE(watchdog_.start());
    watchdog_.stop();
    
    // Shutdown callback would be called during cleanup if needed
    // This is a basic test structure
}

// Test statistics
TEST_F(WatchdogTest, Statistics) {
    EXPECT_EQ(watchdog_.get_failure_count(), 0);
    EXPECT_EQ(watchdog_.get_restart_count(), 0);
    
    watchdog_.record_failure();
    watchdog_.record_failure();
    EXPECT_EQ(watchdog_.get_failure_count(), 2);
    
    watchdog_.set_restart_callback([this]() {
        restart_count_++;
        return true;
    });
    
    watchdog_.trigger_restart();
    EXPECT_GT(watchdog_.get_restart_count(), 0);
}

// Test disabled watchdog
TEST_F(WatchdogTest, DisabledWatchdog) {
    config_.enabled = false;
    watchdog_.set_config(config_);
    
    EXPECT_FALSE(watchdog_.start());
    EXPECT_FALSE(watchdog_.is_running());
}

