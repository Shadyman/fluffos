/*
 * GraphQL Subscription Manager Implementation
 * 
 * Handles real-time subscriptions via WebSocket integration.
 */

#include "graphql.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <regex>

/*
 * GraphQL Subscription Manager Implementation
 */

GraphQLSubscriptionManager::GraphQLSubscriptionManager() {
    GRAPHQL_DEBUG("Creating GraphQL subscription manager");
}

GraphQLSubscriptionManager::~GraphQLSubscriptionManager() {
    GRAPHQL_DEBUG("Destroying GraphQL subscription manager");
    
    // Clean up all subscriptions
    subscriptions_.clear();
    socket_subscriptions_.clear();
    event_subscriptions_.clear();
}

std::string GraphQLSubscriptionManager::create_subscription(int socket_fd, const std::string& subscription,
                                                          const std::map<std::string, std::string>& variables) {
    GRAPHQL_DEBUG_F("Creating subscription for socket %d", socket_fd);
    
    // Generate unique subscription ID
    std::string subscription_id = generate_subscription_id();
    
    // Extract event type from subscription
    std::string event_type = extract_event_type(subscription);
    
    // Create subscription object
    Subscription sub;
    sub.id = subscription_id;
    sub.socket_fd = socket_fd;
    sub.query = subscription;
    sub.event_type = event_type;
    sub.variables = variables;
    sub.created_at = std::time(nullptr);
    
    // Store subscription
    subscriptions_[subscription_id] = sub;
    
    // Add to socket mapping
    socket_subscriptions_[socket_fd].push_back(subscription_id);
    
    // Add to event type mapping
    if (!event_type.empty()) {
        event_subscriptions_[event_type].push_back(subscription_id);
    }
    
    GRAPHQL_DEBUG_F("Created subscription %s for event type '%s'", 
                   subscription_id.c_str(), event_type.c_str());
    
    return subscription_id;
}

bool GraphQLSubscriptionManager::remove_subscription(const std::string& subscription_id) {
    GRAPHQL_DEBUG_F("Removing subscription %s", subscription_id.c_str());
    
    auto sub_it = subscriptions_.find(subscription_id);
    if (sub_it == subscriptions_.end()) {
        GRAPHQL_DEBUG_F("Subscription %s not found", subscription_id.c_str());
        return false;
    }
    
    const Subscription& sub = sub_it->second;
    
    // Remove from socket mapping
    auto socket_it = socket_subscriptions_.find(sub.socket_fd);
    if (socket_it != socket_subscriptions_.end()) {
        auto& socket_subs = socket_it->second;
        socket_subs.erase(std::remove(socket_subs.begin(), socket_subs.end(), subscription_id), 
                         socket_subs.end());
        
        // Remove socket entry if empty
        if (socket_subs.empty()) {
            socket_subscriptions_.erase(socket_it);
        }
    }
    
    // Remove from event type mapping
    if (!sub.event_type.empty()) {
        auto event_it = event_subscriptions_.find(sub.event_type);
        if (event_it != event_subscriptions_.end()) {
            auto& event_subs = event_it->second;
            event_subs.erase(std::remove(event_subs.begin(), event_subs.end(), subscription_id),
                           event_subs.end());
            
            // Remove event entry if empty
            if (event_subs.empty()) {
                event_subscriptions_.erase(event_it);
            }
        }
    }
    
    // Remove subscription
    subscriptions_.erase(sub_it);
    
    GRAPHQL_DEBUG_F("Removed subscription %s", subscription_id.c_str());
    return true;
}

void GraphQLSubscriptionManager::remove_all_subscriptions(int socket_fd) {
    GRAPHQL_DEBUG_F("Removing all subscriptions for socket %d", socket_fd);
    
    auto socket_it = socket_subscriptions_.find(socket_fd);
    if (socket_it == socket_subscriptions_.end()) {
        GRAPHQL_DEBUG_F("No subscriptions found for socket %d", socket_fd);
        return;
    }
    
    // Get list of subscription IDs for this socket
    std::vector<std::string> subscription_ids = socket_it->second;
    
    // Remove each subscription
    for (const std::string& subscription_id : subscription_ids) {
        remove_subscription(subscription_id);
    }
    
    GRAPHQL_DEBUG_F("Removed %zu subscriptions for socket %d", 
                   subscription_ids.size(), socket_fd);
}

void GraphQLSubscriptionManager::broadcast_to_subscription(const std::string& subscription_id, 
                                                         const std::string& data) {
    GRAPHQL_DEBUG_F("Broadcasting to subscription %s", subscription_id.c_str());
    
    auto sub_it = subscriptions_.find(subscription_id);
    if (sub_it == subscriptions_.end()) {
        GRAPHQL_DEBUG_F("Subscription %s not found for broadcast", subscription_id.c_str());
        return;
    }
    
    const Subscription& sub = sub_it->second;
    
    // Check if connection is still active
    if (!is_connection_active(sub.socket_fd)) {
        GRAPHQL_DEBUG_F("Connection %d is not active, removing subscription %s", 
                       sub.socket_fd, subscription_id.c_str());
        remove_subscription(subscription_id);
        return;
    }
    
    // Format subscription response
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":\"" << subscription_id << "\",";
    oss << "\"type\":\"data\",";
    oss << "\"payload\":" << data;
    oss << "}";
    
    std::string message = oss.str();
    
    // In a real implementation, this would send the message via WebSocket
    // For now, we'll log it
    GRAPHQL_DEBUG_F("Subscription message for fd %d: %s", sub.socket_fd, message.c_str());
}

void GraphQLSubscriptionManager::broadcast_to_type(const std::string& event_type, 
                                                  const std::string& data) {
    GRAPHQL_DEBUG_F("Broadcasting to event type '%s'", event_type.c_str());
    
    auto event_it = event_subscriptions_.find(event_type);
    if (event_it == event_subscriptions_.end()) {
        GRAPHQL_DEBUG_F("No subscriptions found for event type '%s'", event_type.c_str());
        return;
    }
    
    const std::vector<std::string>& subscription_ids = event_it->second;
    
    GRAPHQL_DEBUG_F("Broadcasting to %zu subscriptions for event type '%s'", 
                   subscription_ids.size(), event_type.c_str());
    
    // Broadcast to all subscriptions of this type
    for (const std::string& subscription_id : subscription_ids) {
        broadcast_to_subscription(subscription_id, data);
    }
}

void GraphQLSubscriptionManager::broadcast_global(const std::string& data) {
    GRAPHQL_DEBUG_F("Broadcasting globally to %zu subscriptions", subscriptions_.size());
    
    // Broadcast to all active subscriptions
    for (const auto& sub_pair : subscriptions_) {
        broadcast_to_subscription(sub_pair.first, data);
    }
}

void GraphQLSubscriptionManager::register_connection(int socket_fd) {
    GRAPHQL_DEBUG_F("Registering connection for socket %d", socket_fd);
    
    // Initialize empty subscription list for this socket
    if (socket_subscriptions_.find(socket_fd) == socket_subscriptions_.end()) {
        socket_subscriptions_[socket_fd] = std::vector<std::string>();
    }
}

void GraphQLSubscriptionManager::unregister_connection(int socket_fd) {
    GRAPHQL_DEBUG_F("Unregistering connection for socket %d", socket_fd);
    
    // Remove all subscriptions for this socket
    remove_all_subscriptions(socket_fd);
    
    // Remove socket entry
    socket_subscriptions_.erase(socket_fd);
}

bool GraphQLSubscriptionManager::is_connection_active(int socket_fd) {
    // In a real implementation, this would check if the socket is still open
    // For now, we'll assume all registered connections are active
    return socket_subscriptions_.find(socket_fd) != socket_subscriptions_.end();
}

int GraphQLSubscriptionManager::get_subscription_count() const {
    return static_cast<int>(subscriptions_.size());
}

int GraphQLSubscriptionManager::get_connection_count() const {
    return static_cast<int>(socket_subscriptions_.size());
}

std::map<std::string, int> GraphQLSubscriptionManager::get_subscription_stats() const {
    std::map<std::string, int> stats;
    
    // Count subscriptions by event type
    for (const auto& event_pair : event_subscriptions_) {
        stats[event_pair.first] = static_cast<int>(event_pair.second.size());
    }
    
    // Add total counts
    stats["_total_subscriptions"] = get_subscription_count();
    stats["_total_connections"] = get_connection_count();
    
    return stats;
}

std::string GraphQLSubscriptionManager::generate_subscription_id() {
    // Generate a random subscription ID
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream oss;
    oss << "sub_";
    
    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    oss << std::hex << time_t;
    
    // Add random component
    oss << "_";
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << dis(gen);
    }
    
    return oss.str();
}

std::string GraphQLSubscriptionManager::extract_event_type(const std::string& subscription) {
    // Extract event type from GraphQL subscription
    // This is a simplified implementation
    
    // Look for common patterns in subscription queries
    std::vector<std::string> patterns = {
        "playerUpdated", "playerCreated", "playerDeleted",
        "roomUpdated", "roomCreated", "roomDeleted",
        "chatMessage", "systemMessage",
        "inventoryChanged", "statsChanged",
        "questUpdated", "questCompleted"
    };
    
    for (const std::string& pattern : patterns) {
        if (subscription.find(pattern) != std::string::npos) {
            GRAPHQL_DEBUG_F("Extracted event type '%s' from subscription", pattern.c_str());
            return pattern;
        }
    }
    
    // If no specific pattern found, try to extract from field name
    std::regex field_pattern(R"(subscription\s*\{?\s*(\w+))");
    std::smatch match;
    if (std::regex_search(subscription, match, field_pattern)) {
        std::string field_name = match[1].str();
        GRAPHQL_DEBUG_F("Extracted event type '%s' from field name", field_name.c_str());
        return field_name;
    }
    
    // Default to generic subscription
    GRAPHQL_DEBUG("Using default event type 'subscription'");
    return "subscription";
}

void GraphQLSubscriptionManager::cleanup_expired_subscriptions() {
    GRAPHQL_DEBUG("Cleaning up expired subscriptions");
    
    // Get current time
    time_t now = std::time(nullptr);
    const time_t max_age = 3600; // 1 hour
    
    std::vector<std::string> expired_subscriptions;
    
    // Find expired subscriptions
    for (const auto& sub_pair : subscriptions_) {
        const Subscription& sub = sub_pair.second;
        if (now - sub.created_at > max_age) {
            expired_subscriptions.push_back(sub.id);
        }
    }
    
    // Remove expired subscriptions
    for (const std::string& subscription_id : expired_subscriptions) {
        GRAPHQL_DEBUG_F("Removing expired subscription %s", subscription_id.c_str());
        remove_subscription(subscription_id);
    }
    
    GRAPHQL_DEBUG_F("Cleaned up %zu expired subscriptions", expired_subscriptions.size());
}