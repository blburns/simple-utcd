/*
 * tests/test_utc_packet.cpp
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
#include "simple_utcd/utc_packet.hpp"
#include <vector>
#include <cstdint>
#include <thread>
#include <chrono>

using namespace simple_utcd;

class UTCPacketTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test fixtures if needed
    }

    void TearDown() override {
        // Clean up test fixtures if needed
    }
};

// Test default constructor
TEST_F(UTCPacketTest, DefaultConstructor) {
    UTCPacket packet;
    EXPECT_GT(packet.get_timestamp(), 0);
    EXPECT_TRUE(packet.is_valid());
}

// Test constructor with timestamp
TEST_F(UTCPacketTest, ConstructorWithTimestamp) {
    uint32_t timestamp = 1609459200; // 2021-01-01 00:00:00 UTC
    UTCPacket packet(timestamp);
    EXPECT_EQ(packet.get_timestamp(), timestamp);
    EXPECT_TRUE(packet.is_valid());
}

// Test packet size
TEST_F(UTCPacketTest, PacketSize) {
    UTCPacket packet;
    EXPECT_EQ(packet.get_packet_size(), 4); // 32-bit timestamp = 4 bytes
}

// Test to_bytes and from_bytes roundtrip
TEST_F(UTCPacketTest, BytesRoundtrip) {
    uint32_t original_timestamp = 1609459200;
    UTCPacket original_packet(original_timestamp);
    
    // Convert to bytes
    std::vector<uint8_t> bytes = original_packet.to_bytes();
    EXPECT_EQ(bytes.size(), 4);
    
    // Convert back from bytes
    UTCPacket restored_packet;
    EXPECT_TRUE(restored_packet.from_bytes(bytes));
    EXPECT_EQ(restored_packet.get_timestamp(), original_timestamp);
}

// Test from_bytes with invalid size
TEST_F(UTCPacketTest, FromBytesInvalidSize) {
    UTCPacket packet;
    std::vector<uint8_t> invalid_data = {0x01, 0x02, 0x03}; // Only 3 bytes
    EXPECT_FALSE(packet.from_bytes(invalid_data));
}

// Test from_bytes with valid data
TEST_F(UTCPacketTest, FromBytesValid) {
    uint32_t timestamp = 1609459200;
    std::vector<uint8_t> data = {
        static_cast<uint8_t>((timestamp >> 24) & 0xFF),
        static_cast<uint8_t>((timestamp >> 16) & 0xFF),
        static_cast<uint8_t>((timestamp >> 8) & 0xFF),
        static_cast<uint8_t>(timestamp & 0xFF)
    };
    
    UTCPacket packet;
    EXPECT_TRUE(packet.from_bytes(data));
    EXPECT_EQ(packet.get_timestamp(), timestamp);
}

// Test timestamp_to_string
TEST_F(UTCPacketTest, TimestampToString) {
    uint32_t timestamp = 1609459200; // 2021-01-01 00:00:00 UTC
    std::string time_str = UTCPacket::timestamp_to_string(timestamp);
    EXPECT_FALSE(time_str.empty());
    EXPECT_NE(time_str, "Invalid timestamp");
}

// Test get_current_utc_timestamp
TEST_F(UTCPacketTest, CurrentUTCTimestamp) {
    uint32_t timestamp1 = UTCPacket::get_current_utc_timestamp();
    EXPECT_GT(timestamp1, 0);
    
    // Wait a moment
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    uint32_t timestamp2 = UTCPacket::get_current_utc_timestamp();
    EXPECT_GE(timestamp2, timestamp1);
}

// Test set_timestamp
TEST_F(UTCPacketTest, SetTimestamp) {
    UTCPacket packet;
    uint32_t new_timestamp = 1609459200;
    packet.set_timestamp(new_timestamp);
    EXPECT_EQ(packet.get_timestamp(), new_timestamp);
}

// Test to_string
TEST_F(UTCPacketTest, ToString) {
    UTCPacket packet(1609459200);
    std::string str = packet.to_string();
    EXPECT_FALSE(str.empty());
}

