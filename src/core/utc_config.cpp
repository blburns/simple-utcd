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
#include <filesystem>
#if defined(ENABLE_JSON) && ENABLE_JSON
#include <json/json.h>
#endif

namespace simple_utcd {

UTCConfig::UTCConfig() {
    set_defaults();
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
    std::filesystem::path path(config_file);
    std::string ext = path.extension().string();
    
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

} // namespace simple_utcd
