/*
 * src/core/utc_config.cpp
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

#include "simple_utcd/utc_config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sys/stat.h>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <string>
// Fallback - use string manipulation for extension detection
#endif
#if defined(ENABLE_JSON) && ENABLE_JSON
#include <json/json.h>
#endif

namespace simple_utcd {

UTCConfig::UTCConfig() {
    file_watching_enabled_ = false;
    last_file_check_ = std::chrono::system_clock::now();
    set_defaults();
}

UTCConfig::UTCConfig(const UTCConfig& other) {
    // Copy all members
    listen_address_ = other.listen_address_;
    listen_port_ = other.listen_port_;
    enable_ipv6_ = other.enable_ipv6_;
    max_connections_ = other.max_connections_;
    stratum_ = other.stratum_;
    reference_id_ = other.reference_id_;
    reference_clock_ = other.reference_clock_;
    upstream_servers_ = other.upstream_servers_;
    sync_interval_ = other.sync_interval_;
    timeout_ = other.timeout_;
    log_file_ = other.log_file_;
    log_level_ = other.log_level_;
    enable_console_logging_ = other.enable_console_logging_;
    enable_syslog_ = other.enable_syslog_;
    enable_authentication_ = other.enable_authentication_;
    authentication_key_ = other.authentication_key_;
    restrict_queries_ = other.restrict_queries_;
    allowed_clients_ = other.allowed_clients_;
    denied_clients_ = other.denied_clients_;
    worker_threads_ = other.worker_threads_;
    max_packet_size_ = other.max_packet_size_;
    enable_statistics_ = other.enable_statistics_;
    stats_interval_ = other.stats_interval_;
}

UTCConfig& UTCConfig::operator=(const UTCConfig& other) {
    if (this != &other) {
        listen_address_ = other.listen_address_;
        listen_port_ = other.listen_port_;
        enable_ipv6_ = other.enable_ipv6_;
        max_connections_ = other.max_connections_;
        stratum_ = other.stratum_;
        reference_id_ = other.reference_id_;
        reference_clock_ = other.reference_clock_;
        upstream_servers_ = other.upstream_servers_;
        sync_interval_ = other.sync_interval_;
        timeout_ = other.timeout_;
        log_file_ = other.log_file_;
        log_level_ = other.log_level_;
        enable_console_logging_ = other.enable_console_logging_;
        enable_syslog_ = other.enable_syslog_;
        enable_authentication_ = other.enable_authentication_;
        authentication_key_ = other.authentication_key_;
        restrict_queries_ = other.restrict_queries_;
        allowed_clients_ = other.allowed_clients_;
        denied_clients_ = other.denied_clients_;
        worker_threads_ = other.worker_threads_;
        max_packet_size_ = other.max_packet_size_;
        enable_statistics_ = other.enable_statistics_;
        stats_interval_ = other.stats_interval_;
    }
    return *this;
}

UTCConfig::~UTCConfig() {
    // Nothing to clean up
}

void UTCConfig::set_defaults() {
    // Network Configuration
    listen_address_ = "0.0.0.0";
    listen_port_ = 37;  // UTC protocol port
    enable_ipv6_ = true;
    max_connections_ = 1000;

    // UTC Server Configuration
    stratum_ = 2;
    reference_id_ = "UTC";
    reference_clock_ = "UTC";
    upstream_servers_ = {"time.nist.gov", "time.google.com", "pool.ntp.org"};
    sync_interval_ = 64;
    timeout_ = 1000;

    // Logging Configuration
    log_file_ = "/var/log/simple-utcd/simple-utcd.log";
    log_level_ = "INFO";
    enable_console_logging_ = true;
    enable_syslog_ = false;

    // Security Configuration
    enable_authentication_ = false;
    authentication_key_ = "";
    restrict_queries_ = false;
    allowed_clients_ = {};
    denied_clients_ = {};

    // Performance Configuration
    worker_threads_ = 4;
    max_packet_size_ = 1024;
    enable_statistics_ = true;
    stats_interval_ = 60;
}

UTCConfig::ConfigFormat UTCConfig::detect_format(const std::string& config_file) {
    std::string ext;
    
#if __has_include(<filesystem>) || __has_include(<experimental/filesystem>)
    fs::path path(config_file);
    ext = path.extension().string();
#else
    // Fallback: extract extension manually
    size_t dot_pos = config_file.find_last_of('.');
    if (dot_pos != std::string::npos) {
        ext = config_file.substr(dot_pos);
    }
#endif
    
    // Convert to lowercase for comparison
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".json") {
        return ConfigFormat::JSON;
    } else if (ext == ".yaml" || ext == ".yml") {
        return ConfigFormat::YAML;
    } else if (ext == ".ini" || ext == ".conf" || ext == ".cfg") {
        return ConfigFormat::INI;
    }
    
    // Default to INI for backward compatibility
    return ConfigFormat::INI;
}

bool UTCConfig::load(const std::string& config_file) {
    config_file_path_ = config_file;
    last_file_check_ = std::chrono::system_clock::now();
    return load(config_file, ConfigFormat::AUTO);
}

bool UTCConfig::load(const std::string& config_file, ConfigFormat format) {
    if (format == ConfigFormat::AUTO) {
        format = detect_format(config_file);
    }
    
    switch (format) {
        case ConfigFormat::INI:
            return load_ini(config_file);
        case ConfigFormat::YAML:
            return load_yaml(config_file);
        case ConfigFormat::JSON:
            return load_json(config_file);
        default:
            return load_ini(config_file); // Fallback to INI
    }
}

bool UTCConfig::load_ini(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    int line_number = 0;

    while (std::getline(file, line)) {
        line_number++;

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse configuration line
        if (!parse_config_line(line)) {
            // Log error but continue parsing
            continue;
        }
    }

    file.close();
    return true;
}

bool UTCConfig::save(const std::string& config_file) {
    std::ofstream file(config_file);
    if (!file.is_open()) {
        return false;
    }

    file << "# Simple UTC Daemon Configuration File\n";
    file << "# Generated automatically\n\n";

    // Network Configuration
    file << "# Network Configuration\n";
    file << "listen_address = " << listen_address_ << "\n";
    file << "listen_port = " << listen_port_ << "\n";
    file << "enable_ipv6 = " << (enable_ipv6_ ? "true" : "false") << "\n";
    file << "max_connections = " << max_connections_ << "\n\n";

    // UTC Server Configuration
    file << "# UTC Server Configuration\n";
    file << "stratum = " << stratum_ << "\n";
    file << "reference_id = " << reference_id_ << "\n";
    file << "reference_clock = " << reference_clock_ << "\n";
    file << "upstream_servers = [";
    for (size_t i = 0; i < upstream_servers_.size(); ++i) {
        if (i > 0) file << ", ";
        file << "\"" << upstream_servers_[i] << "\"";
    }
    file << "]\n";
    file << "sync_interval = " << sync_interval_ << "\n";
    file << "timeout = " << timeout_ << "\n\n";

    // Logging Configuration
    file << "# Logging Configuration\n";
    file << "log_file = " << log_file_ << "\n";
    file << "log_level = " << log_level_ << "\n";
    file << "enable_console_logging = " << (enable_console_logging_ ? "true" : "false") << "\n";
    file << "enable_syslog = " << (enable_syslog_ ? "true" : "false") << "\n\n";

    // Security Configuration
    file << "# Security Configuration\n";
    file << "enable_authentication = " << (enable_authentication_ ? "true" : "false") << "\n";
    file << "authentication_key = " << authentication_key_ << "\n";
    file << "restrict_queries = " << (restrict_queries_ ? "true" : "false") << "\n";
    file << "allowed_clients = [";
    for (size_t i = 0; i < allowed_clients_.size(); ++i) {
        if (i > 0) file << ", ";
        file << "\"" << allowed_clients_[i] << "\"";
    }
    file << "]\n";
    file << "denied_clients = [";
    for (size_t i = 0; i < denied_clients_.size(); ++i) {
        if (i > 0) file << ", ";
        file << "\"" << denied_clients_[i] << "\"";
    }
    file << "]\n\n";

    // Performance Configuration
    file << "# Performance Configuration\n";
    file << "worker_threads = " << worker_threads_ << "\n";
    file << "max_packet_size = " << max_packet_size_ << "\n";
    file << "enable_statistics = " << (enable_statistics_ ? "true" : "false") << "\n";
    file << "stats_interval = " << stats_interval_ << "\n\n";

    file.close();
    return true;
}

bool UTCConfig::parse_config_line(const std::string& line) {
    // Find the equals sign
    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
        return false;
    }

    std::string key = trim(line.substr(0, eq_pos));
    std::string value = trim(line.substr(eq_pos + 1));

    // Convert key to lowercase for case-insensitive matching
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    // Parse different configuration options
    if (key == "listen_address") {
        listen_address_ = value;
    } else if (key == "listen_port") {
        listen_port_ = std::stoi(value);
    } else if (key == "enable_ipv6") {
        enable_ipv6_ = (value == "true" || value == "1" || value == "yes");
    } else if (key == "max_connections") {
        max_connections_ = std::stoi(value);
    } else if (key == "stratum") {
        stratum_ = std::stoi(value);
    } else if (key == "reference_id") {
        reference_id_ = value;
    } else if (key == "reference_clock") {
        reference_clock_ = value;
    } else if (key == "upstream_servers") {
        upstream_servers_ = parse_list(value);
    } else if (key == "sync_interval") {
        sync_interval_ = std::stoi(value);
    } else if (key == "timeout") {
        timeout_ = std::stoi(value);
    } else if (key == "log_file") {
        log_file_ = value;
    } else if (key == "log_level") {
        log_level_ = value;
    } else if (key == "enable_console_logging") {
        enable_console_logging_ = (value == "true" || value == "1" || value == "yes");
    } else if (key == "enable_syslog") {
        enable_syslog_ = (value == "true" || value == "1" || value == "yes");
    } else if (key == "enable_authentication") {
        enable_authentication_ = (value == "true" || value == "1" || value == "yes");
    } else if (key == "authentication_key") {
        authentication_key_ = value;
    } else if (key == "restrict_queries") {
        restrict_queries_ = (value == "true" || value == "1" || value == "yes");
    } else if (key == "allowed_clients") {
        allowed_clients_ = parse_list(value);
    } else if (key == "denied_clients") {
        denied_clients_ = parse_list(value);
    } else if (key == "worker_threads") {
        worker_threads_ = std::stoi(value);
    } else if (key == "max_packet_size") {
        max_packet_size_ = std::stoi(value);
    } else if (key == "enable_statistics") {
        enable_statistics_ = (value == "true" || value == "1" || value == "yes");
    } else if (key == "stats_interval") {
        stats_interval_ = std::stoi(value);
    } else {
        // Unknown configuration option
        return false;
    }

    return true;
}

std::string UTCConfig::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) {
        return "";
    }

    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> UTCConfig::parse_list(const std::string& str) {
    std::vector<std::string> result;

    // Remove brackets if present
    std::string cleaned = str;
    if (cleaned.front() == '[' && cleaned.back() == ']') {
        cleaned = cleaned.substr(1, cleaned.length() - 2);
    }

    // Split by comma
    std::stringstream ss(cleaned);
    std::string item;

    while (std::getline(ss, item, ',')) {
        item = trim(item);

        // Remove quotes if present
        if (item.front() == '"' && item.back() == '"') {
            item = item.substr(1, item.length() - 2);
        }

        if (!item.empty()) {
            result.push_back(item);
        }
    }

    return result;
}

bool UTCConfig::load_yaml(const std::string& config_file) {
    // YAML parsing - basic implementation using INI-style parsing for now
    // Full YAML support would require yaml-cpp library
    // For version 0.1.1, we'll use a simplified approach
    std::ifstream file(config_file);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    std::string current_section;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Check for section header [section]
        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Parse key: value format (YAML style)
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = trim(line.substr(0, colon_pos));
            std::string value = trim(line.substr(colon_pos + 1));
            
            // Remove quotes if present
            if (!value.empty() && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
            // Build full key with section prefix if needed
            std::string full_key = current_section.empty() ? key : current_section + "." + key;
            set_value(full_key, value);
        }
    }
    
    file.close();
    return true;
}

bool UTCConfig::load_json(const std::string& config_file) {
#if defined(ENABLE_JSON) && ENABLE_JSON
    std::ifstream file(config_file);
    if (!file.is_open()) {
        return false;
    }
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(file, root)) {
        file.close();
        return false;
    }
    
    file.close();
    
    // Parse JSON structure
    if (root.isMember("network")) {
        const Json::Value& network = root["network"];
        if (network.isMember("listen_address")) {
            listen_address_ = network["listen_address"].asString();
        }
        if (network.isMember("listen_port")) {
            listen_port_ = network["listen_port"].asInt();
        }
        if (network.isMember("enable_ipv6")) {
            enable_ipv6_ = network["enable_ipv6"].asBool();
        }
        if (network.isMember("max_connections")) {
            max_connections_ = network["max_connections"].asInt();
        }
    }
    
    if (root.isMember("server")) {
        const Json::Value& server = root["server"];
        if (server.isMember("stratum")) {
            stratum_ = server["stratum"].asInt();
        }
        if (server.isMember("reference_id")) {
            reference_id_ = server["reference_id"].asString();
        }
        if (server.isMember("reference_clock")) {
            reference_clock_ = server["reference_clock"].asString();
        }
        if (server.isMember("upstream_servers") && server["upstream_servers"].isArray()) {
            upstream_servers_.clear();
            for (const auto& server_name : server["upstream_servers"]) {
                upstream_servers_.push_back(server_name.asString());
            }
        }
        if (server.isMember("sync_interval")) {
            sync_interval_ = server["sync_interval"].asInt();
        }
        if (server.isMember("timeout")) {
            timeout_ = server["timeout"].asInt();
        }
    }
    
    if (root.isMember("logging")) {
        const Json::Value& logging = root["logging"];
        if (logging.isMember("log_file")) {
            log_file_ = logging["log_file"].asString();
        }
        if (logging.isMember("log_level")) {
            log_level_ = logging["log_level"].asString();
        }
        if (logging.isMember("enable_console_logging")) {
            enable_console_logging_ = logging["enable_console_logging"].asBool();
        }
        if (logging.isMember("enable_syslog")) {
            enable_syslog_ = logging["enable_syslog"].asBool();
        }
    }
    
    if (root.isMember("security")) {
        const Json::Value& security = root["security"];
        if (security.isMember("enable_authentication")) {
            enable_authentication_ = security["enable_authentication"].asBool();
        }
        if (security.isMember("authentication_key")) {
            authentication_key_ = security["authentication_key"].asString();
        }
        if (security.isMember("restrict_queries")) {
            restrict_queries_ = security["restrict_queries"].asBool();
        }
        if (security.isMember("allowed_clients") && security["allowed_clients"].isArray()) {
            allowed_clients_.clear();
            for (const auto& client : security["allowed_clients"]) {
                allowed_clients_.push_back(client.asString());
            }
        }
        if (security.isMember("denied_clients") && security["denied_clients"].isArray()) {
            denied_clients_.clear();
            for (const auto& client : security["denied_clients"]) {
                denied_clients_.push_back(client.asString());
            }
        }
    }
    
    if (root.isMember("performance")) {
        const Json::Value& performance = root["performance"];
        if (performance.isMember("worker_threads")) {
            worker_threads_ = performance["worker_threads"].asInt();
        }
        if (performance.isMember("max_packet_size")) {
            max_packet_size_ = performance["max_packet_size"].asInt();
        }
        if (performance.isMember("enable_statistics")) {
            enable_statistics_ = performance["enable_statistics"].asBool();
        }
        if (performance.isMember("stats_interval")) {
            stats_interval_ = performance["stats_interval"].asInt();
        }
    }
    
    return true;
#else
    // JSON support not enabled, fall back to INI
    return load_ini(config_file);
#endif
}

bool UTCConfig::set_value(const std::string& key, const std::string& value) {
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
    
    // Handle sectioned keys (e.g., "network.listen_address")
    size_t dot_pos = lower_key.find('.');
    if (dot_pos != std::string::npos) {
        std::string section = lower_key.substr(0, dot_pos);
        std::string actual_key = lower_key.substr(dot_pos + 1);
        
        // Map sectioned keys to actual config keys
        if (section == "network") {
            return set_value(actual_key, value);
        } else if (section == "server") {
            // Map server.* keys
            if (actual_key == "upstream_servers") {
                upstream_servers_ = parse_list(value);
                return true;
            }
            return set_value(actual_key, value);
        } else if (section == "logging") {
            return set_value(actual_key, value);
        } else if (section == "security") {
            if (actual_key == "allowed_clients") {
                allowed_clients_ = parse_list(value);
                return true;
            } else if (actual_key == "denied_clients") {
                denied_clients_ = parse_list(value);
                return true;
            }
            return set_value(actual_key, value);
        } else if (section == "performance") {
            return set_value(actual_key, value);
        }
    }
    
    // Use existing parse_config_line logic
    return parse_config_line(key + " = " + value);
}

void UTCConfig::load_from_environment() {
    // Network configuration
    std::string env_value = get_env_var("SIMPLE_UTCD_LISTEN_ADDRESS");
    if (!env_value.empty()) {
        listen_address_ = env_value;
    }
    
    env_value = get_env_var("SIMPLE_UTCD_LISTEN_PORT");
    if (!env_value.empty()) {
        try {
            listen_port_ = std::stoi(env_value);
        } catch (...) {
            // Invalid port, keep default
        }
    }
    
    env_value = get_env_var("SIMPLE_UTCD_ENABLE_IPV6");
    if (!env_value.empty()) {
        enable_ipv6_ = (env_value == "true" || env_value == "1" || env_value == "yes");
    }
    
    env_value = get_env_var("SIMPLE_UTCD_MAX_CONNECTIONS");
    if (!env_value.empty()) {
        try {
            max_connections_ = std::stoi(env_value);
        } catch (...) {
            // Invalid value, keep default
        }
    }
    
    // Server configuration
    env_value = get_env_var("SIMPLE_UTCD_STRATUM");
    if (!env_value.empty()) {
        try {
            stratum_ = std::stoi(env_value);
        } catch (...) {
            // Invalid value, keep default
        }
    }
    
    env_value = get_env_var("SIMPLE_UTCD_LOG_LEVEL");
    if (!env_value.empty()) {
        log_level_ = env_value;
    }
    
    env_value = get_env_var("SIMPLE_UTCD_LOG_FILE");
    if (!env_value.empty()) {
        log_file_ = env_value;
    }
    
    env_value = get_env_var("SIMPLE_UTCD_WORKER_THREADS");
    if (!env_value.empty()) {
        try {
            worker_threads_ = std::stoi(env_value);
        } catch (...) {
            // Invalid value, keep default
        }
    }
    
    // Security configuration (sensitive data)
    env_value = get_env_var("SIMPLE_UTCD_AUTH_KEY");
    if (!env_value.empty()) {
        authentication_key_ = env_value;
        enable_authentication_ = true;
    }
}

std::string UTCConfig::get_env_var(const std::string& name, const std::string& default_value) {
    const char* value = std::getenv(name.c_str());
    if (value != nullptr) {
        return std::string(value);
    }
    return default_value;
}

bool UTCConfig::validate() const {
    validation_errors_.clear();
    
    bool valid = true;
    valid &= validate_network_config();
    valid &= validate_server_config();
    valid &= validate_logging_config();
    valid &= validate_security_config();
    valid &= validate_performance_config();
    
    return valid;
}

std::vector<std::string> UTCConfig::get_validation_errors() const {
    return validation_errors_;
}

bool UTCConfig::validate_network_config() const {
    bool valid = true;
    
    if (listen_port_ < 1 || listen_port_ > 65535) {
        validation_errors_.push_back("Invalid listen_port: must be between 1 and 65535");
        valid = false;
    }
    
    if (max_connections_ < 1 || max_connections_ > 100000) {
        validation_errors_.push_back("Invalid max_connections: must be between 1 and 100000");
        valid = false;
    }
    
    if (listen_address_.empty()) {
        validation_errors_.push_back("listen_address cannot be empty");
        valid = false;
    }
    
    return valid;
}

bool UTCConfig::validate_server_config() const {
    bool valid = true;
    
    if (stratum_ < 1 || stratum_ > 15) {
        validation_errors_.push_back("Invalid stratum: must be between 1 and 15");
        valid = false;
    }
    
    if (sync_interval_ < 1 || sync_interval_ > 65535) {
        validation_errors_.push_back("Invalid sync_interval: must be between 1 and 65535");
        valid = false;
    }
    
    if (timeout_ < 1 || timeout_ > 60000) {
        validation_errors_.push_back("Invalid timeout: must be between 1 and 60000 ms");
        valid = false;
    }
    
    return valid;
}

bool UTCConfig::validate_logging_config() const {
    bool valid = true;
    
    std::vector<std::string> valid_levels = {"DEBUG", "INFO", "WARN", "ERROR"};
    std::string upper_level = log_level_;
    std::transform(upper_level.begin(), upper_level.end(), upper_level.begin(), ::toupper);
    
    if (std::find(valid_levels.begin(), valid_levels.end(), upper_level) == valid_levels.end()) {
        validation_errors_.push_back("Invalid log_level: must be DEBUG, INFO, WARN, or ERROR");
        valid = false;
    }
    
    return valid;
}

bool UTCConfig::validate_security_config() const {
    bool valid = true;
    
    if (enable_authentication_ && authentication_key_.empty()) {
        validation_errors_.push_back("authentication_key is required when authentication is enabled");
        valid = false;
    }
    
    return valid;
}

bool UTCConfig::validate_performance_config() const {
    bool valid = true;
    
    if (worker_threads_ < 1 || worker_threads_ > 128) {
        validation_errors_.push_back("Invalid worker_threads: must be between 1 and 128");
        valid = false;
    }
    
    if (max_packet_size_ < 4 || max_packet_size_ > 65535) {
        validation_errors_.push_back("Invalid max_packet_size: must be between 4 and 65535");
        valid = false;
    }
    
    if (stats_interval_ < 1 || stats_interval_ > 3600) {
        validation_errors_.push_back("Invalid stats_interval: must be between 1 and 3600 seconds");
        valid = false;
    }
    
    return valid;
}

void UTCConfig::enable_file_watching(bool enable) {
    file_watching_enabled_ = enable;
}

bool UTCConfig::check_config_file_changed() const {
    if (!file_watching_enabled_ || config_file_path_.empty()) {
        return false;
    }
    
    // Check file modification time (periodic polling - full implementation would use inotify/kqueue)
    auto now = std::chrono::system_clock::now();
    auto time_since_check = std::chrono::duration_cast<std::chrono::seconds>(now - last_file_check_).count();
    
    // Check every 5 seconds
    if (time_since_check < 5) {
        return false;
    }
    
    // Check file modification time using stat
    struct stat file_stat;
    if (stat(config_file_path_.c_str(), &file_stat) != 0) {
        // File doesn't exist or can't be accessed
        return false;
    }
    
    // Convert file mtime to system_clock time_point
    auto file_mtime = std::chrono::system_clock::from_time_t(file_stat.st_mtime);
    
    // Update last check time
    const_cast<UTCConfig*>(this)->last_file_check_ = now;
    
    // Compare with last known modification time
    // For simplicity, we'll check if mtime is newer than last check minus a small buffer
    // In a full implementation, we'd store the last known mtime
    static std::chrono::system_clock::time_point last_known_mtime;
    static bool first_check = true;
    
    if (first_check) {
        last_known_mtime = file_mtime;
        first_check = false;
        return false;
    }
    
    if (file_mtime > last_known_mtime) {
        last_known_mtime = file_mtime;
        return true;
    }
    
    return false;
}

} // namespace simple_utcd
