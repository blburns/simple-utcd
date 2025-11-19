/*
 * includes/simple_utcd/backup_restore.hpp
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
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <cstdint>

namespace simple_utcd {

/**
 * @brief Backup entry
 */
struct BackupEntry {
    std::string id;
    std::string path;
    std::string type;  // "config", "state", "metrics"
    std::chrono::system_clock::time_point timestamp;
    uint64_t size;
    std::string description;
    
    BackupEntry() : size(0) {}
};

/**
 * @brief Backup and restore manager
 */
class BackupRestoreManager {
public:
    BackupRestoreManager();
    ~BackupRestoreManager();

    // Configuration
    void set_backup_directory(const std::string& directory);
    void set_max_backups(uint64_t max_count);
    void set_backup_retention_days(uint64_t days);
    void enable_auto_backup(bool enable);
    
    // Configuration backup
    bool backup_config(const std::string& config_path, const std::string& description = "");
    bool restore_config(const std::string& backup_id, const std::string& target_path);
    bool delete_config_backup(const std::string& backup_id);
    std::vector<BackupEntry> list_config_backups() const;
    
    // State persistence
    bool save_state(const std::map<std::string, std::string>& state, const std::string& description = "");
    bool load_state(std::map<std::string, std::string>& state);
    bool delete_state_backup(const std::string& backup_id);
    std::vector<BackupEntry> list_state_backups() const;
    
    // Metrics persistence
    bool save_metrics(const std::string& metrics_data, const std::string& description = "");
    bool load_metrics(std::string& metrics_data, const std::string& backup_id = "");
    bool delete_metrics_backup(const std::string& backup_id);
    std::vector<BackupEntry> list_metrics_backups() const;
    
    // Automatic backup
    void perform_auto_backup();
    void cleanup_old_backups();
    
    // Backup management
    std::vector<BackupEntry> list_all_backups() const;
    BackupEntry get_backup_info(const std::string& backup_id) const;
    bool delete_backup(const std::string& backup_id);
    uint64_t get_backup_count() const;
    uint64_t get_total_backup_size() const;
    
    // Rollback
    bool rollback_to_backup(const std::string& backup_id);
    std::vector<std::string> get_rollback_history() const;

private:
    std::string backup_directory_;
    uint64_t max_backups_;
    uint64_t retention_days_;
    bool auto_backup_enabled_;
    
    std::map<std::string, BackupEntry> backups_;
    mutable std::mutex backups_mutex_;
    
    // Backup operations
    std::string generate_backup_id(const std::string& type) const;
    std::string get_backup_path(const std::string& backup_id, const std::string& type) const;
    bool create_backup_directory() const;
    bool copy_file(const std::string& source, const std::string& destination) const;
    
    // Cleanup
    bool is_backup_expired(const BackupEntry& backup) const;
    void remove_expired_backups();
    
    // Time utilities
    std::chrono::system_clock::time_point now() const;
    uint64_t days_since(const std::chrono::system_clock::time_point& time) const;
};

} // namespace simple_utcd

