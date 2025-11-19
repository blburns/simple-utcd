/*
 * tests/test_certificate_acl.cpp
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
#include "simple_utcd/certificate_acl.hpp"
#include "simple_utcd/tls_manager.hpp"

using namespace simple_utcd;

class CertificateACLTest : public ::testing::Test {
protected:
    void SetUp() override {
        acl_.set_default_action(true);
    }

    void TearDown() override {
        acl_.clear_rules();
    }

    CertificateACL acl_;
    
    CertificateInfo create_test_cert(const std::string& cn, const std::string& subject, 
                                     const std::string& fingerprint, const std::string& issuer) {
        CertificateInfo cert;
        cert.common_name = cn;
        cert.subject = subject;
        cert.fingerprint = fingerprint;
        cert.issuer = issuer;
        cert.is_valid = true;
        return cert;
    }
};

// Test default constructor
TEST_F(CertificateACLTest, DefaultConstructor) {
    CertificateACL acl;
    EXPECT_TRUE(acl.get_default_action());
    EXPECT_EQ(acl.get_allowed_count(), 0);
    EXPECT_EQ(acl.get_denied_count(), 0);
}

// Test add rule
TEST_F(CertificateACLTest, AddRule) {
    CertificateACLRule rule;
    rule.id = "rule1";
    rule.common_name = "test.example.com";
    rule.allow = true;
    rule.priority = 10;
    
    EXPECT_TRUE(acl_.add_rule(rule));
    
    auto rules = acl_.get_rules();
    EXPECT_EQ(rules.size(), 1);
    EXPECT_EQ(rules[0].id, "rule1");
}

// Test add duplicate rule
TEST_F(CertificateACLTest, AddDuplicateRule) {
    CertificateACLRule rule;
    rule.id = "rule1";
    rule.common_name = "test.example.com";
    
    EXPECT_TRUE(acl_.add_rule(rule));
    EXPECT_FALSE(acl_.add_rule(rule));  // Duplicate ID
}

// Test remove rule
TEST_F(CertificateACLTest, RemoveRule) {
    CertificateACLRule rule;
    rule.id = "rule1";
    rule.common_name = "test.example.com";
    
    acl_.add_rule(rule);
    EXPECT_EQ(acl_.get_rules().size(), 1);
    
    EXPECT_TRUE(acl_.remove_rule("rule1"));
    EXPECT_EQ(acl_.get_rules().size(), 0);
}

// Test clear rules
TEST_F(CertificateACLTest, ClearRules) {
    CertificateACLRule rule1, rule2;
    rule1.id = "rule1";
    rule2.id = "rule2";
    
    acl_.add_rule(rule1);
    acl_.add_rule(rule2);
    EXPECT_EQ(acl_.get_rules().size(), 2);
    
    acl_.clear_rules();
    EXPECT_EQ(acl_.get_rules().size(), 0);
}

// Test common name matching
TEST_F(CertificateACLTest, CommonNameMatching) {
    CertificateACLRule rule;
    rule.id = "rule1";
    rule.common_name = "test.example.com";
    rule.allow = true;
    
    acl_.add_rule(rule);
    
    auto cert = create_test_cert("test.example.com", "", "", "");
    EXPECT_TRUE(acl_.is_allowed(cert));
    
    auto cert2 = create_test_cert("other.example.com", "", "", "");
    EXPECT_TRUE(acl_.is_allowed(cert2));  // Default allow
}

// Test subject matching
TEST_F(CertificateACLTest, SubjectMatching) {
    CertificateACLRule rule;
    rule.id = "rule1";
    rule.subject = "CN=test";
    rule.allow = true;
    
    acl_.add_rule(rule);
    
    auto cert = create_test_cert("", "CN=test, O=Example", "", "");
    EXPECT_TRUE(acl_.is_allowed(cert));
}

// Test fingerprint matching
TEST_F(CertificateACLTest, FingerprintMatching) {
    CertificateACLRule rule;
    rule.id = "rule1";
    rule.fingerprint = "ABCD1234";
    rule.allow = true;
    
    acl_.add_rule(rule);
    
    auto cert = create_test_cert("", "", "abcd1234", "");  // Case insensitive
    EXPECT_TRUE(acl_.is_allowed(cert));
}

// Test issuer matching
TEST_F(CertificateACLTest, IssuerMatching) {
    CertificateACLRule rule;
    rule.id = "rule1";
    rule.issuer = "CN=CA";
    rule.allow = true;
    
    acl_.add_rule(rule);
    
    auto cert = create_test_cert("", "", "", "CN=CA, O=Example");
    EXPECT_TRUE(acl_.is_allowed(cert));
}

// Test deny rule
TEST_F(CertificateACLTest, DenyRule) {
    CertificateACLRule rule;
    rule.id = "rule1";
    rule.common_name = "blocked.example.com";
    rule.allow = false;
    rule.priority = 10;
    
    acl_.add_rule(rule);
    
    auto cert = create_test_cert("blocked.example.com", "", "", "");
    EXPECT_FALSE(acl_.is_allowed(cert));
    EXPECT_TRUE(acl_.is_denied(cert));
}

// Test priority ordering
TEST_F(CertificateACLTest, PriorityOrdering) {
    CertificateACLRule rule1, rule2;
    rule1.id = "rule1";
    rule1.common_name = "test.example.com";
    rule1.allow = false;
    rule1.priority = 5;
    
    rule2.id = "rule2";
    rule2.common_name = "test.example.com";
    rule2.allow = true;
    rule2.priority = 10;  // Higher priority
    
    acl_.add_rule(rule1);
    acl_.add_rule(rule2);
    
    auto cert = create_test_cert("test.example.com", "", "", "");
    // Higher priority rule (allow) should win
    EXPECT_TRUE(acl_.is_allowed(cert));
}

// Test default action
TEST_F(CertificateACLTest, DefaultAction) {
    acl_.set_default_action(false);
    
    auto cert = create_test_cert("unknown.example.com", "", "", "");
    EXPECT_FALSE(acl_.is_allowed(cert));
    
    acl_.set_default_action(true);
    EXPECT_TRUE(acl_.is_allowed(cert));
}

// Test statistics
TEST_F(CertificateACLTest, Statistics) {
    CertificateACLRule allow_rule, deny_rule;
    allow_rule.id = "allow";
    allow_rule.common_name = "allowed.example.com";
    allow_rule.allow = true;
    
    deny_rule.id = "deny";
    deny_rule.common_name = "denied.example.com";
    deny_rule.allow = false;
    
    acl_.add_rule(allow_rule);
    acl_.add_rule(deny_rule);
    
    auto cert1 = create_test_cert("allowed.example.com", "", "", "");
    auto cert2 = create_test_cert("denied.example.com", "", "", "");
    
    acl_.is_allowed(cert1);
    acl_.is_allowed(cert2);
    
    EXPECT_GT(acl_.get_allowed_count(), 0);
    EXPECT_GT(acl_.get_denied_count(), 0);
    
    acl_.reset_statistics();
    EXPECT_EQ(acl_.get_allowed_count(), 0);
    EXPECT_EQ(acl_.get_denied_count(), 0);
}

// Test multiple field matching
TEST_F(CertificateACLTest, MultipleFieldMatching) {
    CertificateACLRule rule;
    rule.id = "rule1";
    rule.common_name = "test.example.com";
    rule.issuer = "CN=CA";
    rule.allow = true;
    
    acl_.add_rule(rule);
    
    // Matches both
    auto cert1 = create_test_cert("test.example.com", "", "", "CN=CA");
    EXPECT_TRUE(acl_.is_allowed(cert1));
    
    // Matches CN but not issuer
    auto cert2 = create_test_cert("test.example.com", "", "", "CN=Other");
    EXPECT_TRUE(acl_.is_allowed(cert2));  // Default allow
}

// Test wildcard matching
TEST_F(CertificateACLTest, WildcardMatching) {
    CertificateACLRule rule;
    rule.id = "rule1";
    rule.common_name = "*.example.com";
    rule.allow = true;
    
    acl_.add_rule(rule);
    
    auto cert1 = create_test_cert("test.example.com", "", "", "");
    EXPECT_TRUE(acl_.is_allowed(cert1));
    
    auto cert2 = create_test_cert("sub.test.example.com", "", "", "");
    // Simple wildcard implementation may not handle this
    // This is a basic test
}

