/*
 * src/core/backup_restore.cpp
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

#include "simple_utcd/backup_restore.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sys/stat.h>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

namespace simple_utcd {

BackupRestoreManager::BackupRestoreManager()
    : backup_directory_("/var/backups/simple-utcd")
    , max_backups_(10)
    , retention_days_(30)
    , auto_backup_enabled_(false)
{
}

BackupRestoreManager::~BackupRestoreManager() {
}

void BackupRestoreManager::set_backup_directory(const std::string& directory) {
    backup_directory_ = directory;
}

void BackupRestoreManager::set_max_backups(uint64_t max_count) {
    max_backups_ = max_count;
}

void BackupRestoreManager::set_backup_retention_days(uint64_t days) {
    retention_days_ = days;
}

void BackupRestoreManager::enable_auto_backup(bool enable) {
    auto_backup_enabled_ = enable;
}

bool BackupRestoreManager::backup_config(const std::string& config_path, const std::string& description) {
    if (!create_backup_directory()) {
        return false;
    }
    
    std::string backup_id = generate_backup_id("config");
    std::string backup_path = get_backup_path(backup_id, "config");
    
    if (!copy_file(config_path, backup_path)) {
        return false;
    }
    
    BackupEntry entry;
    entry.id = backup_id;
    entry.path = backup_path;
    entry.type = "config";
    entry.timestamp = now();
    entry.description = description;
    
    // Get file size
#if __has_include(<filesystem>) || __has_include(<experimental/filesystem>)
    if (fs::exists(backup_path)) {
        entry.size = fs::file_size(backup_path);
    }
#else
    struct stat st;
    if (stat(backup_path.c_str(), &st) == 0) {
        entry.size = st.st_size;
    }
#endif
    
    std::lock_guard<std::mutex> lock(backups_mutex_);
    backups_[backup_id] = entry;
    
    // Cleanup old backups
    cleanup_old_backups();
    
    return true;
}

bool BackupRestoreManager::restore_config(const std::string& backup_id, const std::string& target_path) {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    auto it = backups_.find(backup_id);
    if (it == backups_.end() || it->second.type != "config") {
        return false;
    }
    
    return copy_file(it->second.path, target_path);
}

bool BackupRestoreManager::delete_config_backup(const std::string& backup_id) {
    return delete_backup(backup_id);
}

std::vector<BackupEntry> BackupRestoreManager::list_config_backups() const {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    std::vector<BackupEntry> result;
    
    for (const auto& pair : backups_) {
        if (pair.second.type == "config") {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

bool BackupRestoreManager::save_state(const std::map<std::string, std::string>& state, 
                                     const std::string& description) {
    if (!create_backup_directory()) {
        return false;
    }
    
    std::string backup_id = generate_backup_id("state");
    std::string backup_path = get_backup_path(backup_id, "state");
    
    std::ofstream file(backup_path);
    if (!file.is_open()) {
        return false;
    }
    
    // Simple key-value format
    for (const auto& pair : state) {
        file << pair.first << "=" << pair.second << "\n";
    }
    file.close();
    
    BackupEntry entry;
    entry.id = backup_id;
    entry.path = backup_path;
    entry.type = "state";
    entry.timestamp = now();
    entry.description = description;
    
#if __has_include(<filesystem>) || __has_include(<experimental/filesystem>)
    if (fs::exists(backup_path)) {
        entry.size = fs::file_size(backup_path);
    }
#else
    struct stat st;
    if (stat(backup_path.c_str(), &st) == 0) {
        entry.size = st.st_size;
    }
#endif
    
    std::lock_guard<std::mutex> lock(backups_mutex_);
    backups_[backup_id] = entry;
    
    return true;
}

bool BackupRestoreManager::load_state(std::map<std::string, std::string>& state) {
    // Load most recent state backup
    std::vector<BackupEntry> state_backups = list_state_backups();
    if (state_backups.empty()) {
        return false;
    }
    
    // Sort by timestamp (most recent first)
    std::sort(state_backups.begin(), state_backups.end(),
        [](const BackupEntry& a, const BackupEntry& b) {
            return a.timestamp > b.timestamp;
        });
    
    std::string backup_path = state_backups[0].path;
    std::ifstream file(backup_path);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            state[key] = value;
        }
    }
    
    return true;
}

bool BackupRestoreManager::delete_state_backup(const std::string& backup_id) {
    return delete_backup(backup_id);
}

std::vector<BackupEntry> BackupRestoreManager::list_state_backups() const {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    std::vector<BackupEntry> result;
    
    for (const auto& pair : backups_) {
        if (pair.second.type == "state") {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

bool BackupRestoreManager::save_metrics(const std::string& metrics_data, const std::string& description) {
    if (!create_backup_directory()) {
        return false;
    }
    
    std::string backup_id = generate_backup_id("metrics");
    std::string backup_path = get_backup_path(backup_id, "metrics");
    
    std::ofstream file(backup_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << metrics_data;
    file.close();
    
    BackupEntry entry;
    entry.id = backup_id;
    entry.path = backup_path;
    entry.type = "metrics";
    entry.timestamp = now();
    entry.description = description;
    entry.size = metrics_data.length();
    
    std::lock_guard<std::mutex> lock(backups_mutex_);
    backups_[backup_id] = entry;
    
    return true;
}

bool BackupRestoreManager::load_metrics(std::string& metrics_data, const std::string& backup_id) {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    
    std::string backup_path;
    if (backup_id.empty()) {
        // Load most recent
        std::vector<BackupEntry> metrics_backups = list_metrics_backups();
        if (metrics_backups.empty()) {
            return false;
        }
        std::sort(metrics_backups.begin(), metrics_backups.end(),
            [](const BackupEntry& a, const BackupEntry& b) {
                return a.timestamp > b.timestamp;
            });
        backup_path = metrics_backups[0].path;
    } else {
        auto it = backups_.find(backup_id);
        if (it == backups_.end() || it->second.type != "metrics") {
            return false;
        }
        backup_path = it->second.path;
    }
    
    std::ifstream file(backup_path);
    if (!file.is_open()) {
        return false;
    }
    
    std::ostringstream ss;
    ss << file.rdbuf();
    metrics_data = ss.str();
    
    return true;
}

bool BackupRestoreManager::delete_metrics_backup(const std::string& backup_id) {
    return delete_backup(backup_id);
}

std::vector<BackupEntry> BackupRestoreManager::list_metrics_backups() const {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    std::vector<BackupEntry> result;
    
    for (const auto& pair : backups_) {
        if (pair.second.type == "metrics") {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

void BackupRestoreManager::perform_auto_backup() {
    if (!auto_backup_enabled_) {
        return;
    }
    
    // Auto-backup would be triggered by external scheduler
    // This is a placeholder for the functionality
}

void BackupRestoreManager::cleanup_old_backups() {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    
    // Remove expired backups
    remove_expired_backups();
    
    // Limit total backups
    if (backups_.size() > max_backups_) {
        // Sort by timestamp and remove oldest
        std::vector<std::pair<std::string, BackupEntry>> sorted;
        for (const auto& pair : backups_) {
            sorted.push_back(pair);
        }
        
        std::sort(sorted.begin(), sorted.end(),
            [](const std::pair<std::string, BackupEntry>& a,
               const std::pair<std::string, BackupEntry>& b) {
                return a.second.timestamp < b.second.timestamp;
            });
        
        // Remove oldest until under limit
        while (backups_.size() > max_backups_ && !sorted.empty()) {
            delete_backup(sorted[0].first);
            sorted.erase(sorted.begin());
        }
    }
}

std::vector<BackupEntry> BackupRestoreManager::list_all_backups() const {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    std::vector<BackupEntry> result;
    
    for (const auto& pair : backups_) {
        result.push_back(pair.second);
    }
    
    return result;
}

BackupEntry BackupRestoreManager::get_backup_info(const std::string& backup_id) const {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    auto it = backups_.find(backup_id);
    if (it != backups_.end()) {
        return it->second;
    }
    return BackupEntry();
}

bool BackupRestoreManager::delete_backup(const std::string& backup_id) {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    auto it = backups_.find(backup_id);
    if (it == backups_.end()) {
        return false;
    }
    
    // Delete file
    std::remove(it->second.path.c_str());
    
    // Remove from map
    backups_.erase(it);
    
    return true;
}

uint64_t BackupRestoreManager::get_backup_count() const {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    return backups_.size();
}

uint64_t BackupRestoreManager::get_total_backup_size() const {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    uint64_t total = 0;
    for (const auto& pair : backups_) {
        total += pair.second.size;
    }
    return total;
}

bool BackupRestoreManager::rollback_to_backup(const std::string& backup_id) {
    std::lock_guard<std::mutex> lock(backups_mutex_);
    auto it = backups_.find(backup_id);
    if (it == backups_.end()) {
        return false;
    }
    
    const auto& backup = it->second;
    
    if (backup.type == "config") {
        // Restore config - would need target path
        return true;  // Placeholder
    } else if (backup.type == "state") {
        // Restore state
        return true;  // Placeholder
    }
    
    return false;
}

std::vector<std::string> BackupRestoreManager::get_rollback_history() const {
    // Placeholder for rollback history tracking
    return std::vector<std::string>();
}

std::string BackupRestoreManager::generate_backup_id(const std::string& type) const {
    auto now_time = now();
    auto time_t = std::chrono::system_clock::to_time_t(now_time);
    std::tm* tm = std::gmtime(&time_t);
    
    std::ostringstream ss;
    ss << type << "_";
    ss << std::put_time(tm, "%Y%m%d_%H%M%S");
    ss << "_" << std::chrono::duration_cast<std::chrono::milliseconds>(
        now_time.time_since_epoch()).count() % 1000;
    
    return ss.str();
}

std::string BackupRestoreManager::get_backup_path(const std::string& backup_id, const std::string& type) const {
    return backup_directory_ + "/" + backup_id + "." + type;
}

bool BackupRestoreManager::create_backup_directory() const {
#if __has_include(<filesystem>) || __has_include(<experimental/filesystem>)
    if (!fs::exists(backup_directory_)) {
        return fs::create_directories(backup_directory_);
    }
    return true;
#else
    // Fallback: try to create directory using system calls
    struct stat st;
    if (stat(backup_directory_.c_str(), &st) != 0) {
        // Directory doesn't exist, try to create it
        // Note: This is a simplified version - full implementation would use mkdir
        return false;  // Would need platform-specific implementation
    }
    return true;
#endif
}

bool BackupRestoreManager::copy_file(const std::string& source, const std::string& destination) const {
    std::ifstream src(source, std::ios::binary);
    if (!src.is_open()) {
        return false;
    }
    
    std::ofstream dst(destination, std::ios::binary);
    if (!dst.is_open()) {
        src.close();
        return false;
    }
    
    dst << src.rdbuf();
    
    src.close();
    dst.close();
    
    return true;
}

bool BackupRestoreManager::is_backup_expired(const BackupEntry& backup) const {
    return days_since(backup.timestamp) > retention_days_;
}

void BackupRestoreManager::remove_expired_backups() {
    auto it = backups_.begin();
    while (it != backups_.end()) {
        if (is_backup_expired(it->second)) {
            std::remove(it->second.path.c_str());
            it = backups_.erase(it);
        } else {
            ++it;
        }
    }
}

std::chrono::system_clock::time_point BackupRestoreManager::now() const {
    return std::chrono::system_clock::now();
}

uint64_t BackupRestoreManager::days_since(const std::chrono::system_clock::time_point& time) const {
    auto elapsed = std::chrono::duration_cast<std::chrono::hours>(now() - time).count();
    return static_cast<uint64_t>(elapsed / 24);
}

} // namespace simple_utcd

