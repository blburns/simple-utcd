/*
 * src/core/utc_packet.cpp
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

#include "simple_utcd/utc_packet.hpp"
#include "simple_utcd/error_handler.hpp"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace simple_utcd {

UTCPacket::UTCPacket() : timestamp_(0), version_(1), mode_(3) {
    timestamp_ = get_current_utc_timestamp();
}

UTCPacket::UTCPacket(uint32_t timestamp) : timestamp_(timestamp), version_(1), mode_(3) {
}

UTCPacket::~UTCPacket() {
    // Nothing to clean up
}

bool UTCPacket::from_bytes(const std::vector<uint8_t>& data) {
    // Enhanced validation: check packet size first
    if (!validate_packet_size(data.size())) {
        UTC_ERROR("UTCPacket", "Invalid packet size: expected " + std::to_string(get_packet_size()) +
                  " bytes, got " + std::to_string(data.size()));
        return false;
    }

    // For basic UTC protocol (4 bytes), parse timestamp
    // Network byte order (big-endian)
    timestamp_ = (static_cast<uint32_t>(data[0]) << 24) |
                 (static_cast<uint32_t>(data[1]) << 16) |
                 (static_cast<uint32_t>(data[2]) << 8) |
                 static_cast<uint32_t>(data[3]);

    // For extended packets (future), parse version and mode
    if (data.size() >= 6) {
        version_ = data[4];
        mode_ = data[5];
        
        // Validate version
        if (!validate_version(version_)) {
            UTC_ERROR("UTCPacket", "Invalid protocol version: " + std::to_string(version_));
            return false;
        }
        
        // Validate mode
        if (!validate_mode(mode_)) {
            UTC_ERROR("UTCPacket", "Invalid packet mode: " + std::to_string(mode_));
            return false;
        }
    }

    // Validate checksum if present (for extended packets)
    if (data.size() >= 8 && !validate_checksum(data)) {
        UTC_ERROR("UTCPacket", "Checksum validation failed");
        return false;
    }

    // Validate timestamp
    if (!is_valid()) {
        UTC_ERROR("UTCPacket", "Invalid timestamp in packet: " + std::to_string(timestamp_));
        return false;
    }

    return true;
}

std::vector<uint8_t> UTCPacket::to_bytes() const {
    std::vector<uint8_t> data(get_packet_size());

    // Convert timestamp to network byte order (big-endian)
    data[0] = static_cast<uint8_t>((timestamp_ >> 24) & 0xFF);
    data[1] = static_cast<uint8_t>((timestamp_ >> 16) & 0xFF);
    data[2] = static_cast<uint8_t>((timestamp_ >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(timestamp_ & 0xFF);

    return data;
}

uint32_t UTCPacket::get_current_utc_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint32_t>(time_t);
}

std::string UTCPacket::timestamp_to_string(uint32_t timestamp) {
    std::time_t time_t = static_cast<std::time_t>(timestamp);
    std::tm* tm = std::gmtime(&time_t);

    if (!tm) {
        return "Invalid timestamp";
    }

    std::stringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

uint32_t UTCPacket::string_to_timestamp(const std::string& time_str) {
    // Parse format: "YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DD HH:MM:SS UTC"
    std::string cleaned = time_str;

    // Remove "UTC" suffix if present
    if (cleaned.length() > 4 && cleaned.substr(cleaned.length() - 4) == " UTC") {
        cleaned = cleaned.substr(0, cleaned.length() - 4);
    }

    std::tm tm = {};
    std::istringstream ss(cleaned);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        return 0;
    }

    // Convert to UTC timestamp
    std::time_t time_t = std::mktime(&tm);
    if (time_t == -1) {
        return 0;
    }

    return static_cast<uint32_t>(time_t);
}

bool UTCPacket::is_valid() const {
    return validate_timestamp(timestamp_);
}

size_t UTCPacket::get_packet_size() const {
    return 4; // 32-bit timestamp in bytes
}

std::string UTCPacket::to_string() const {
    std::stringstream ss;
    ss << "UTCPacket{timestamp=" << timestamp_
       << ", time=" << timestamp_to_string(timestamp_)
       << ", valid=" << (is_valid() ? "true" : "false") << "}";
    return ss.str();
}

bool UTCPacket::validate_timestamp(uint32_t timestamp) const {
    // Basic validation: timestamp should be reasonable
    // Not before 1970 (Unix epoch) and not too far in the future

    const uint32_t unix_epoch = 0; // January 1, 1970

    // Check if timestamp is within reasonable bounds
    if (timestamp < unix_epoch) {
        return false;
    }

    // Check if timestamp is not too far in the future (e.g., 100 years from now)
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint32_t current_timestamp = static_cast<uint32_t>(now_time_t);

    // Allow some tolerance for clock differences (e.g., 1 hour)
    const uint32_t tolerance = 3600; // 1 hour in seconds

    if (timestamp > (current_timestamp + tolerance)) {
        return false;
    }

    return true;
}

bool UTCPacket::validate_packet_size(size_t size) const {
    // Basic UTC packet is 4 bytes (timestamp only)
    // Extended packets can be 6+ bytes (with version/mode/checksum)
    return size >= get_packet_size() && size <= 48; // Max reasonable packet size
}

bool UTCPacket::validate_checksum(const std::vector<uint8_t>& data) const {
    if (data.size() < 8) {
        return true; // No checksum for basic packets
    }
    
    // Extract stored checksum (last 2 bytes)
    uint16_t stored_checksum = (static_cast<uint16_t>(data[data.size() - 2]) << 8) |
                               static_cast<uint16_t>(data[data.size() - 1]);
    
    // Calculate checksum for data (excluding checksum bytes)
    std::vector<uint8_t> data_for_checksum(data.begin(), data.end() - 2);
    uint16_t calculated_checksum = calculate_checksum(data_for_checksum);
    
    return stored_checksum == calculated_checksum;
}

uint16_t UTCPacket::calculate_checksum(const std::vector<uint8_t>& data) const {
    // Simple checksum: sum of all bytes
    uint32_t sum = 0;
    for (uint8_t byte : data) {
        sum += byte;
    }
    // Return 16-bit checksum
    return static_cast<uint16_t>(sum & 0xFFFF);
}

bool UTCPacket::validate_version(uint8_t version) const {
    // Supported protocol versions: 1-4
    return version >= 1 && version <= 4;
}

bool UTCPacket::validate_mode(uint8_t mode) const {
    // Valid modes: 0-7 (3 bits)
    // Mode 3 = client request, Mode 4 = server response
    return mode <= 7;
}

} // namespace simple_utcd
