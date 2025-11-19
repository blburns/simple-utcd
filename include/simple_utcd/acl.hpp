/*
 * includes/simple_utcd/acl.hpp
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
#include <set>
#include <memory>
#include <mutex>
#include <cstdint>

namespace simple_utcd {

/**
 * @brief ACL rule action
 */
enum class ACLAction {
    ALLOW,
    DENY
};

/**
 * @brief ACL rule
 */
struct ACLRule {
    ACLAction action;
    std::string network;  // CIDR notation (e.g., "192.168.1.0/24")
    std::string description;
    
    ACLRule() : action(ACLAction::DENY) {}
    ACLRule(ACLAction act, const std::string& net, const std::string& desc = "")
        : action(act), network(net), description(desc) {}
};

/**
 * @brief Access Control List manager
 */
class ACLManager {
public:
    ACLManager();
    ~ACLManager();

    // Configuration
    void set_default_action(ACLAction action);
    ACLAction get_default_action() const { return default_action_; }
    
    // Rule management
    bool add_rule(const ACLRule& rule);
    bool add_rule(ACLAction action, const std::string& network, const std::string& description = "");
    bool remove_rule(const std::string& network);
    bool has_rule(const std::string& network) const;
    void clear_rules();
    std::vector<ACLRule> get_rules() const;
    
    // Bulk operations
    bool load_rules(const std::vector<std::string>& allowed_networks,
                   const std::vector<std::string>& denied_networks);
    bool load_from_config(const std::vector<std::string>& allowed_clients,
                         const std::vector<std::string>& denied_clients);
    
    // Access checking
    bool is_allowed(const std::string& ip_address) const;
    bool is_denied(const std::string& ip_address) const;
    
    // Network utilities
    static bool is_valid_cidr(const std::string& cidr);
    static bool is_ip_in_network(const std::string& ip, const std::string& cidr);
    static bool parse_cidr(const std::string& cidr, uint32_t& network, uint32_t& mask);
    static uint32_t ip_to_uint32(const std::string& ip);
    static std::string uint32_to_ip(uint32_t ip);

private:
    ACLAction default_action_;
    std::vector<ACLRule> rules_;
    mutable std::mutex rules_mutex_;
    
    // Network matching
    bool matches_rule(const std::string& ip, const ACLRule& rule) const;
    int compare_rules(const ACLRule& a, const ACLRule& b) const;
    
    // Rule ordering (more specific rules first)
    void sort_rules();
};

} // namespace simple_utcd

