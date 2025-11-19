/*
 * tests/test_backup_restore.cpp
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
#include "simple_utcd/backup_restore.hpp"
#include <fstream>
#include <cstdio>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

using namespace simple_utcd;

class BackupRestoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_backup_dir_ = "/tmp/test_simple_utcd_backups";
        manager_.set_backup_directory(test_backup_dir_);
        manager_.set_max_backups(5);
        manager_.set_backup_retention_days(7);
    }

    void TearDown() override {
        // Cleanup test directory
#if __has_include(<filesystem>) || __has_include(<experimental/filesystem>)
        if (fs::exists(test_backup_dir_)) {
            fs::remove_all(test_backup_dir_);
        }
#else
        // Fallback cleanup would go here
#endif
    }

    BackupRestoreManager manager_;
    std::string test_backup_dir_;
    std::string test_config_file_ = "/tmp/test_config.conf";
};

// Test default constructor
TEST_F(BackupRestoreTest, DefaultConstructor) {
    BackupRestoreManager manager;
    EXPECT_EQ(manager.get_backup_count(), 0);
}

// Test configuration backup
TEST_F(BackupRestoreTest, ConfigBackup) {
    // Create a test config file
    std::ofstream config_file(test_config_file_);
    config_file << "listen_port = 37\n";
    config_file << "log_level = INFO\n";
    config_file.close();
    
    EXPECT_TRUE(manager_.backup_config(test_config_file_, "Test backup"));
    EXPECT_GT(manager_.get_backup_count(), 0);
    
    // Cleanup
    std::remove(test_config_file_.c_str());
}

// Test state persistence
TEST_F(BackupRestoreTest, StatePersistence) {
    std::map<std::string, std::string> state;
    state["key1"] = "value1";
    state["key2"] = "value2";
    
    EXPECT_TRUE(manager_.save_state(state, "Test state"));
    
    std::map<std::string, std::string> loaded_state;
    EXPECT_TRUE(manager_.load_state(loaded_state));
    
    EXPECT_EQ(loaded_state["key1"], "value1");
    EXPECT_EQ(loaded_state["key2"], "value2");
}

// Test metrics persistence
TEST_F(BackupRestoreTest, MetricsPersistence) {
    std::string metrics = "simple_utcd_requests_total 100\n";
    
    EXPECT_TRUE(manager_.save_metrics(metrics, "Test metrics"));
    
    std::string loaded_metrics;
    EXPECT_TRUE(manager_.load_metrics(loaded_metrics));
    
    EXPECT_EQ(loaded_metrics, metrics);
}

// Test backup listing
TEST_F(BackupRestoreTest, BackupListing) {
    std::map<std::string, std::string> state;
    state["test"] = "data";
    
    manager_.save_state(state);
    
    auto backups = manager_.list_state_backups();
    EXPECT_GT(backups.size(), 0);
    
    auto all_backups = manager_.list_all_backups();
    EXPECT_GT(all_backups.size(), 0);
}

// Test backup deletion
TEST_F(BackupRestoreTest, BackupDeletion) {
    std::map<std::string, std::string> state;
    state["test"] = "data";
    
    manager_.save_state(state);
    uint64_t count_before = manager_.get_backup_count();
    
    auto backups = manager_.list_state_backups();
    if (!backups.empty()) {
        EXPECT_TRUE(manager_.delete_backup(backups[0].id));
        EXPECT_LT(manager_.get_backup_count(), count_before);
    }
}

// Test backup info
TEST_F(BackupRestoreTest, BackupInfo) {
    std::map<std::string, std::string> state;
    state["test"] = "data";
    
    manager_.save_state(state, "Test description");
    
    auto backups = manager_.list_state_backups();
    if (!backups.empty()) {
        BackupEntry info = manager_.get_backup_info(backups[0].id);
        EXPECT_EQ(info.id, backups[0].id);
        EXPECT_EQ(info.type, "state");
        EXPECT_FALSE(info.description.empty());
    }
}

// Test backup count and size
TEST_F(BackupRestoreTest, BackupCountAndSize) {
    std::map<std::string, std::string> state1;
    state1["key1"] = "value1";
    manager_.save_state(state1);
    
    std::map<std::string, std::string> state2;
    state2["key2"] = "value2";
    manager_.save_state(state2);
    
    EXPECT_GE(manager_.get_backup_count(), 2);
    EXPECT_GT(manager_.get_total_backup_size(), 0);
}

// Test max backups limit
TEST_F(BackupRestoreTest, MaxBackupsLimit) {
    manager_.set_max_backups(3);
    
    // Create more backups than limit
    for (int i = 0; i < 5; i++) {
        std::map<std::string, std::string> state;
        state["key"] = "value" + std::to_string(i);
        manager_.save_state(state);
    }
    
    // Should be limited to max_backups
    EXPECT_LE(manager_.get_backup_count(), 3);
}

// Test config restore
TEST_F(BackupRestoreTest, ConfigRestore) {
    // Create and backup config
    std::ofstream config_file(test_config_file_);
    config_file << "listen_port = 37\n";
    config_file.close();
    
    EXPECT_TRUE(manager_.backup_config(test_config_file_, "Test"));
    
    auto backups = manager_.list_config_backups();
    if (!backups.empty()) {
        std::string restore_path = "/tmp/restored_config.conf";
        EXPECT_TRUE(manager_.restore_config(backups[0].id, restore_path));
        
        // Verify restored file exists
#if __has_include(<filesystem>) || __has_include(<experimental/filesystem>)
        EXPECT_TRUE(fs::exists(restore_path));
        fs::remove(restore_path);
#else
        struct stat st;
        EXPECT_EQ(stat(restore_path.c_str(), &st), 0);
        std::remove(restore_path.c_str());
#endif
    }
    
    std::remove(test_config_file_.c_str());
}

