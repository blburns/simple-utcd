/*
 * src/core/acl.cpp
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

#include "simple_utcd/acl.hpp"
#include <sstream>
#include <algorithm>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace simple_utcd {

ACLManager::ACLManager()
    : default_action_(ACLAction::ALLOW)
{
}

ACLManager::~ACLManager() {
}

void ACLManager::set_default_action(ACLAction action) {
    default_action_ = action;
}

bool ACLManager::add_rule(const ACLRule& rule) {
    if (!is_valid_cidr(rule.network)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    // Remove existing rule for same network if present
    rules_.erase(
        std::remove_if(rules_.begin(), rules_.end(),
            [&rule](const ACLRule& r) { return r.network == rule.network; }),
        rules_.end()
    );
    
    rules_.push_back(rule);
    sort_rules();
    return true;
}

bool ACLManager::add_rule(ACLAction action, const std::string& network, const std::string& description) {
    return add_rule(ACLRule(action, network, description));
}

bool ACLManager::remove_rule(const std::string& network) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    auto it = std::remove_if(rules_.begin(), rules_.end(),
        [&network](const ACLRule& r) { return r.network == network; });
    
    if (it != rules_.end()) {
        rules_.erase(it, rules_.end());
        return true;
    }
    return false;
}

bool ACLManager::has_rule(const std::string& network) const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return std::any_of(rules_.begin(), rules_.end(),
        [&network](const ACLRule& r) { return r.network == network; });
}

void ACLManager::clear_rules() {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_.clear();
}

std::vector<ACLRule> ACLManager::get_rules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return rules_;
}

bool ACLManager::load_rules(const std::vector<std::string>& allowed_networks,
                           const std::vector<std::string>& denied_networks) {
    clear_rules();
    
    for (const auto& network : allowed_networks) {
        add_rule(ACLAction::ALLOW, network);
    }
    
    for (const auto& network : denied_networks) {
        add_rule(ACLAction::DENY, network);
    }
    
    return true;
}

bool ACLManager::load_from_config(const std::vector<std::string>& allowed_clients,
                                 const std::vector<std::string>& denied_clients) {
    return load_rules(allowed_clients, denied_clients);
}

bool ACLManager::is_allowed(const std::string& ip_address) const {
    return !is_denied(ip_address);
}

bool ACLManager::is_denied(const std::string& ip_address) const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    // Check rules in order (most specific first)
    for (const auto& rule : rules_) {
        if (matches_rule(ip_address, rule)) {
            return rule.action == ACLAction::DENY;
        }
    }
    
    // Default action
    return default_action_ == ACLAction::DENY;
}

bool ACLManager::is_valid_cidr(const std::string& cidr) {
    size_t slash_pos = cidr.find('/');
    if (slash_pos == std::string::npos) {
        // Single IP address
        struct sockaddr_in sa;
        return inet_pton(AF_INET, cidr.c_str(), &(sa.sin_addr)) == 1;
    }
    
    std::string ip = cidr.substr(0, slash_pos);
    std::string mask_str = cidr.substr(slash_pos + 1);
    
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 1) {
        return false;
    }
    
    int mask = std::stoi(mask_str);
    return mask >= 0 && mask <= 32;
}

bool ACLManager::is_ip_in_network(const std::string& ip, const std::string& cidr) {
    uint32_t ip_addr, network, mask;
    if (!parse_cidr(cidr, network, mask)) {
        return false;
    }
    
    ip_addr = ip_to_uint32(ip);
    return (ip_addr & mask) == (network & mask);
}

bool ACLManager::parse_cidr(const std::string& cidr, uint32_t& network, uint32_t& mask) {
    size_t slash_pos = cidr.find('/');
    std::string ip_str = cidr;
    int prefix_len = 32;
    
    if (slash_pos != std::string::npos) {
        ip_str = cidr.substr(0, slash_pos);
        prefix_len = std::stoi(cidr.substr(slash_pos + 1));
        if (prefix_len < 0 || prefix_len > 32) {
            return false;
        }
    }
    
    network = ip_to_uint32(ip_str);
    mask = htonl((0xFFFFFFFF << (32 - prefix_len)) & 0xFFFFFFFF);
    
    return true;
}

uint32_t ACLManager::ip_to_uint32(const std::string& ip) {
    struct sockaddr_in sa;
    inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr));
    return ntohl(sa.sin_addr.s_addr);
}

std::string ACLManager::uint32_to_ip(uint32_t ip) {
    struct sockaddr_in sa;
    sa.sin_addr.s_addr = htonl(ip);
    char buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sa.sin_addr), buffer, INET_ADDRSTRLEN);
    return std::string(buffer);
}

bool ACLManager::matches_rule(const std::string& ip, const ACLRule& rule) const {
    return is_ip_in_network(ip, rule.network);
}

int ACLManager::compare_rules(const ACLRule& a, const ACLRule& b) const {
    // More specific networks (smaller prefix) come first
    uint32_t a_network, a_mask, b_network, b_mask;
    parse_cidr(a.network, a_network, a_mask);
    parse_cidr(b.network, b_network, b_mask);
    
    // Count set bits in mask (prefix length)
    int a_prefix = __builtin_popcount(ntohl(a_mask));
    int b_prefix = __builtin_popcount(ntohl(b_mask));
    
    if (a_prefix != b_prefix) {
        return a_prefix - b_prefix;  // Smaller prefix (more specific) first
    }
    
    return 0;
}

void ACLManager::sort_rules() {
    std::sort(rules_.begin(), rules_.end(),
        [this](const ACLRule& a, const ACLRule& b) {
            return compare_rules(a, b) < 0;
        });
}

} // namespace simple_utcd

