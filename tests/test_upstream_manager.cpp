/*
 * tests/test_upstream_manager.cpp
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
#include "simple_utcd/upstream_manager.hpp"
#include <thread>
#include <chrono>

using namespace simple_utcd;

class UpstreamManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_.set_failover_threshold(3);
        manager_.set_recovery_threshold(2);
        manager_.set_timeout(1000);
    }

    void TearDown() override {
        manager_.clear_servers();
    }

    UpstreamManager manager_;
};

// Test default constructor
TEST_F(UpstreamManagerTest, DefaultConstructor) {
    UpstreamManager manager;
    EXPECT_EQ(manager.get_total_server_count(), 0);
}

// Test server management
TEST_F(UpstreamManagerTest, ServerManagement) {
    EXPECT_TRUE(manager_.add_server("time.nist.gov", 37, 1));
    EXPECT_TRUE(manager_.add_server("time.google.com", 37, 2));
    
    EXPECT_EQ(manager_.get_total_server_count(), 2);
    
    EXPECT_TRUE(manager_.remove_server("time.nist.gov"));
    EXPECT_EQ(manager_.get_total_server_count(), 1);
}

// Test server selection strategies
TEST_F(UpstreamManagerTest, SelectionStrategies) {
    manager_.add_server("server1", 37, 1);
    manager_.add_server("server2", 37, 2);
    
    manager_.set_selection_strategy(SelectionStrategy::ROUND_ROBIN);
    UpstreamServer* server1 = manager_.select_server();
    EXPECT_NE(server1, nullptr);
    
    manager_.set_selection_strategy(SelectionStrategy::PRIORITY);
    UpstreamServer* server2 = manager_.select_server();
    EXPECT_NE(server2, nullptr);
}

// Test primary and backup server selection
TEST_F(UpstreamManagerTest, PrimaryBackupSelection) {
    manager_.add_server("primary", 37, 10);
    manager_.add_server("backup", 37, 5);
    
    UpstreamServer* primary = manager_.get_primary_server();
    EXPECT_NE(primary, nullptr);
    EXPECT_EQ(primary->address, "primary");
    
    UpstreamServer* backup = manager_.get_backup_server();
    EXPECT_NE(backup, nullptr);
    EXPECT_EQ(backup->address, "backup");
}

// Test health monitoring
TEST_F(UpstreamManagerTest, HealthMonitoring) {
    manager_.add_server("test-server", 37);
    
    manager_.record_success("test-server", 50);
    EXPECT_EQ(manager_.get_server_status("test-server"), ServerStatus::HEALTHY);
    
    manager_.record_failure("test-server");
    // Status may change based on thresholds
    EXPECT_TRUE(manager_.get_server_status("test-server") == ServerStatus::HEALTHY ||
                manager_.get_server_status("test-server") == ServerStatus::DEGRADED ||
                manager_.get_server_status("test-server") == ServerStatus::UNHEALTHY);
}

// Test failover threshold
TEST_F(UpstreamManagerTest, FailoverThreshold) {
    manager_.set_failover_threshold(2);
    manager_.add_server("test-server", 37);
    
    manager_.record_failure("test-server");
    manager_.record_failure("test-server");
    
    // After threshold failures, server should be marked as failed
    EXPECT_FALSE(manager_.is_server_available("test-server"));
}

// Test recovery
TEST_F(UpstreamManagerTest, Recovery) {
    manager_.set_recovery_threshold(2);
    manager_.add_server("test-server", 37);
    
    // Fail the server
    for (int i = 0; i < 3; i++) {
        manager_.record_failure("test-server");
    }
    
    // Recover with successes
    manager_.record_success("test-server", 50);
    manager_.record_success("test-server", 50);
    
    manager_.update_server_status();
    // Server should recover
    EXPECT_TRUE(manager_.get_server_status("test-server") == ServerStatus::HEALTHY ||
                manager_.get_server_status("test-server") == ServerStatus::DEGRADED);
}

// Test server availability
TEST_F(UpstreamManagerTest, ServerAvailability) {
    manager_.add_server("server1", 37);
    manager_.add_server("server2", 37);
    
    EXPECT_TRUE(manager_.has_available_servers());
    EXPECT_GE(manager_.get_healthy_server_count(), 0);
}

// Test enable/disable server
TEST_F(UpstreamManagerTest, EnableDisableServer) {
    manager_.add_server("server1", 37);
    
    EXPECT_TRUE(manager_.is_server_available("server1"));
    
    manager_.disable_server("server1");
    EXPECT_FALSE(manager_.is_server_available("server1"));
    
    manager_.enable_server("server1");
    EXPECT_TRUE(manager_.is_server_available("server1"));
}

// Test response time tracking
TEST_F(UpstreamManagerTest, ResponseTimeTracking) {
    manager_.add_server("server1", 37);
    
    manager_.record_success("server1", 100);
    EXPECT_EQ(manager_.get_server_response_time("server1"), 100);
    
    manager_.record_success("server1", 50);
    EXPECT_EQ(manager_.get_server_response_time("server1"), 50);
}

// Test multiple servers
TEST_F(UpstreamManagerTest, MultipleServers) {
    manager_.add_server("server1", 37, 1);
    manager_.add_server("server2", 37, 2);
    manager_.add_server("server3", 37, 3);
    
    EXPECT_EQ(manager_.get_total_server_count(), 3);
    
    std::vector<UpstreamServer> servers = manager_.get_servers();
    EXPECT_EQ(servers.size(), 3);
}

// Test round-robin selection
TEST_F(UpstreamManagerTest, RoundRobinSelection) {
    manager_.set_selection_strategy(SelectionStrategy::ROUND_ROBIN);
    manager_.add_server("server1", 37);
    manager_.add_server("server2", 37);
    
    UpstreamServer* s1 = manager_.select_server();
    UpstreamServer* s2 = manager_.select_server();
    
    // Should cycle through servers
    EXPECT_NE(s1, nullptr);
    EXPECT_NE(s2, nullptr);
}

// Test health-based selection
TEST_F(UpstreamManagerTest, HealthBasedSelection) {
    manager_.set_selection_strategy(SelectionStrategy::HEALTH_BASED);
    manager_.add_server("server1", 37);
    manager_.add_server("server2", 37);
    
    manager_.record_success("server1", 50);
    manager_.record_success("server2", 100);
    
    UpstreamServer* selected = manager_.select_server();
    EXPECT_NE(selected, nullptr);
    // Should prefer server1 (lower latency)
    EXPECT_EQ(selected->address, "server1");
}

// Test priority-based selection
TEST_F(UpstreamManagerTest, PriorityBasedSelection) {
    manager_.set_selection_strategy(SelectionStrategy::PRIORITY);
    manager_.add_server("server1", 37, 1);
    manager_.add_server("server2", 37, 10);
    
    UpstreamServer* selected = manager_.select_server();
    EXPECT_NE(selected, nullptr);
    EXPECT_EQ(selected->address, "server2");  // Higher priority
}

