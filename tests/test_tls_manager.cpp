/*
 * tests/test_tls_manager.cpp
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
#include "simple_utcd/tls_manager.hpp"

using namespace simple_utcd;

class TLSManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.enabled = false;  // Disable by default for tests without certs
    }

    void TearDown() override {
        manager_.destroy_context();
    }

    TLSManager manager_;
    TLSConfig config_;
};

// Test default constructor
TEST_F(TLSManagerTest, DefaultConstructor) {
    TLSManager manager;
    EXPECT_FALSE(manager.is_configured());
    EXPECT_FALSE(manager.is_enabled());
}

// Test configuration
TEST_F(TLSManagerTest, Configuration) {
    config_.enabled = true;
    config_.certificate_path = "/path/to/cert.pem";
    config_.private_key_path = "/path/to/key.pem";
    
    EXPECT_TRUE(manager_.configure(config_));
    EXPECT_TRUE(manager_.is_configured());
    
    TLSConfig retrieved = manager_.get_config();
    EXPECT_EQ(retrieved.enabled, config_.enabled);
    EXPECT_EQ(retrieved.certificate_path, config_.certificate_path);
}

// Test configuration validation
TEST_F(TLSManagerTest, ConfigurationValidation) {
    config_.enabled = true;
    // Missing certificate path
    EXPECT_FALSE(manager_.configure(config_));
    
    config_.certificate_path = "/path/to/cert.pem";
    // Missing private key path
    EXPECT_FALSE(manager_.configure(config_));
    
    config_.private_key_path = "/path/to/key.pem";
    EXPECT_TRUE(manager_.configure(config_));
}

// Test TLS version configuration
TEST_F(TLSManagerTest, TLSVersionConfiguration) {
    config_.enabled = true;
    config_.certificate_path = "/path/to/cert.pem";
    config_.private_key_path = "/path/to/key.pem";
    config_.protocols.clear();
    config_.protocols.push_back(TLSVersion::TLS_1_2);
    
    EXPECT_TRUE(manager_.configure(config_));
    
    TLSConfig retrieved = manager_.get_config();
    EXPECT_EQ(retrieved.protocols.size(), 1);
    EXPECT_EQ(retrieved.protocols[0], TLSVersion::TLS_1_2);
}

// Test cipher suite configuration
TEST_F(TLSManagerTest, CipherSuiteConfiguration) {
    config_.enabled = true;
    config_.certificate_path = "/path/to/cert.pem";
    config_.private_key_path = "/path/to/key.pem";
    config_.cipher_suites.push_back("ECDHE-RSA-AES256-GCM-SHA384");
    config_.cipher_suites.push_back("ECDHE-RSA-AES128-GCM-SHA256");
    
    EXPECT_TRUE(manager_.configure(config_));
    
    TLSConfig retrieved = manager_.get_config();
    EXPECT_EQ(retrieved.cipher_suites.size(), 2);
}

// Test certificate validation settings
TEST_F(TLSManagerTest, CertificateValidationSettings) {
    config_.enabled = true;
    config_.certificate_path = "/path/to/cert.pem";
    config_.private_key_path = "/path/to/key.pem";
    config_.verify_peer = true;
    config_.require_client_certificate = true;
    config_.check_certificate_revocation = true;
    
    EXPECT_TRUE(manager_.configure(config_));
    
    TLSConfig retrieved = manager_.get_config();
    EXPECT_TRUE(retrieved.verify_peer);
    EXPECT_TRUE(retrieved.require_client_certificate);
    EXPECT_TRUE(retrieved.check_certificate_revocation);
}

// Test CA certificate configuration
TEST_F(TLSManagerTest, CACertificateConfiguration) {
    config_.enabled = true;
    config_.certificate_path = "/path/to/cert.pem";
    config_.private_key_path = "/path/to/key.pem";
    config_.ca_certificate_path = "/path/to/ca.pem";
    config_.ca_certificate_directory = "/path/to/ca-certs";
    
    EXPECT_TRUE(manager_.configure(config_));
    
    TLSConfig retrieved = manager_.get_config();
    EXPECT_EQ(retrieved.ca_certificate_path, config_.ca_certificate_path);
    EXPECT_EQ(retrieved.ca_certificate_directory, config_.ca_certificate_directory);
}

// Test session configuration
TEST_F(TLSManagerTest, SessionConfiguration) {
    config_.enabled = true;
    config_.certificate_path = "/path/to/cert.pem";
    config_.private_key_path = "/path/to/key.pem";
    config_.session_cache_size = 5000;
    config_.session_timeout = 1800;
    
    EXPECT_TRUE(manager_.configure(config_));
    
    TLSConfig retrieved = manager_.get_config();
    EXPECT_EQ(retrieved.session_cache_size, 5000);
    EXPECT_EQ(retrieved.session_timeout, 1800);
}

// Test context creation (without actual certificates)
TEST_F(TLSManagerTest, ContextCreationWithoutCerts) {
    config_.enabled = false;
    manager_.configure(config_);
    
    // Should fail without certificates
    EXPECT_FALSE(manager_.create_server_context());
    EXPECT_FALSE(manager_.create_client_context());
}

// Test TLS connection (basic structure)
TEST_F(TLSManagerTest, TLSConnectionBasic) {
    TLSConnection conn;
    EXPECT_FALSE(conn.is_connected());
    EXPECT_EQ(conn.get_socket(), -1);
    
    // Close should be safe even when not connected
    conn.close();
}

// Test certificate info structure
TEST_F(TLSManagerTest, CertificateInfoStructure) {
    CertificateInfo info;
    EXPECT_FALSE(info.is_valid);
    EXPECT_FALSE(info.is_revoked);
    EXPECT_TRUE(info.subject.empty());
    EXPECT_TRUE(info.common_name.empty());
}

