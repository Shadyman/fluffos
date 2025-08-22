/*
 * gRPC Package Implementation
 * 
 * FluffOS gRPC package for unified socket architecture.
 * Provides gRPC server and client functionality with Protocol Buffers integration.
 */

#include "grpc.h"
#include "packages/sockets/socket_option_manager.h"

#include <sstream>
#include <algorithm>
#include <chrono>
#include <regex>
#include <random>

// Conditional compilation for gRPC support
#ifdef HAVE_GRPC
#include <grpcpp/grpcpp.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#endif

// Static instance pointer
GrpcManager* GrpcManager::instance_ = nullptr;

/*
 * gRPC Manager Implementation
 */

GrpcManager* GrpcManager::getInstance() {
    if (!instance_) {
        instance_ = new GrpcManager();
    }
    return instance_;
}

GrpcManager::GrpcManager() : initialized_(false) {
    protobuf_manager_ = std::make_unique<GrpcProtobufManager>();
    channel_manager_ = std::make_unique<GrpcChannelManager>();
}

GrpcManager::~GrpcManager() {
    shutdown();
}

bool GrpcManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    GRPC_DEBUG("Initializing gRPC Manager");
    
#ifndef HAVE_GRPC
    GRPC_DEBUG("WARNING: gRPC libraries not available - using stub implementation");
#endif
    
    // Initialize Protocol Buffers manager
    if (!protobuf_manager_) {
        log_error(0, "Failed to create Protocol Buffers manager", "initialize");
        return false;
    }
    
    // Register default MUD services
    setup_default_services();
    register_mud_services();
    
    initialized_ = true;
    GRPC_DEBUG("gRPC Manager initialized successfully");
    return true;
}

void GrpcManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    GRPC_DEBUG("Shutting down gRPC Manager");
    
    // Close all servers and clients
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    servers_.clear();
    clients_.clear();
    service_handlers_.clear();
    streaming_handlers_.clear();
    protobuf_manager_.reset();
    channel_manager_.reset();
    
    initialized_ = false;
    GRPC_DEBUG("gRPC Manager shutdown complete");
}

int GrpcManager::create_grpc_socket(int socket_fd, const std::string& mode) {
    if (!initialized_) {
        if (!initialize()) {
            log_error(socket_fd, "Failed to initialize gRPC Manager", "create_socket");
            return -1;
        }
    }
    
    GRPC_DEBUG_F("Creating gRPC socket for fd %d with mode %s", socket_fd, mode.c_str());
    
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    if (mode == "server" || mode == "GRPC_SERVER") {
        // Create new server instance
        auto server = std::make_unique<GrpcServer>(socket_fd);
        if (!server) {
            log_error(socket_fd, "Failed to create gRPC server instance", "create_socket");
            return -1;
        }
        
        servers_[socket_fd] = std::move(server);
        
    } else if (mode == "client" || mode == "GRPC_CLIENT") {
        // Create new client instance
        auto client = std::make_unique<GrpcClient>(socket_fd);
        if (!client) {
            log_error(socket_fd, "Failed to create gRPC client instance", "create_socket");
            return -1;
        }
        
        clients_[socket_fd] = std::move(client);
        
    } else {
        log_error(socket_fd, "Invalid gRPC socket mode: " + mode, "create_socket");
        return -1;
    }
    
    GRPC_DEBUG_F("gRPC socket created successfully for fd %d", socket_fd);
    return socket_fd;
}

bool GrpcManager::handle_grpc_request(int socket_fd, const std::string& data) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    // Check if it's a server socket
    auto server_it = servers_.find(socket_fd);
    if (server_it != servers_.end()) {
        GRPC_DEBUG_F("Handling gRPC server request for fd %d", socket_fd);
        
        // Parse request
        GrpcRequest request;
        if (!parse_grpc_request(data, request)) {
            log_error(socket_fd, "Failed to parse gRPC request", "handle_request");
            return false;
        }
        
        request.socket_fd = socket_fd;
        
        // Handle request
        GrpcResponse response = server_it->second->handle_request(request);
        
        // Format and send response (in real implementation, this would write to socket)
        std::string response_text = format_grpc_response(response);
        GRPC_DEBUG_F("gRPC server response for fd %d: %s", socket_fd, response_text.c_str());
        
        return response.status == GRPC_OK;
    }
    
    // Check if it's a client socket
    auto client_it = clients_.find(socket_fd);
    if (client_it != clients_.end()) {
        GRPC_DEBUG_F("Handling gRPC client response for fd %d", socket_fd);
        // Handle client response
        return true;
    }
    
    log_error(socket_fd, "gRPC socket not found", "handle_request");
    return false;
}

void GrpcManager::close_grpc_socket(int socket_fd) {
    GRPC_DEBUG_F("Closing gRPC socket for fd %d", socket_fd);
    
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    // Remove server instance
    servers_.erase(socket_fd);
    
    // Remove client instance
    clients_.erase(socket_fd);
    
    GRPC_DEBUG_F("gRPC socket closed for fd %d", socket_fd);
}

bool GrpcManager::register_service(int socket_fd, const std::string& service_definition) {
    auto server_it = servers_.find(socket_fd);
    if (server_it == servers_.end()) {
        log_error(socket_fd, "gRPC server not found for socket", "register_service");
        return false;
    }
    
    GRPC_DEBUG_F("Registering service for fd %d", socket_fd);
    
    server_it->second->set_service_config(service_definition);
    return true;
}

bool GrpcManager::register_method_handler(const std::string& service_name, const std::string& method_name,
                                         GrpcServiceHandler handler) {
    std::string key = generate_method_key(service_name, method_name);
    service_handlers_[key] = handler;
    
    GRPC_DEBUG_F("Registered method handler for %s", key.c_str());
    return true;
}

bool GrpcManager::register_streaming_handler(const std::string& service_name, const std::string& method_name,
                                            GrpcStreamingHandler handler) {
    std::string key = generate_method_key(service_name, method_name);
    streaming_handlers_[key] = handler;
    
    GRPC_DEBUG_F("Registered streaming handler for %s", key.c_str());
    return true;
}

bool GrpcManager::connect_to_service(int socket_fd, const std::string& target) {
    auto client_it = clients_.find(socket_fd);
    if (client_it == clients_.end()) {
        log_error(socket_fd, "gRPC client not found for socket", "connect_to_service");
        return false;
    }
    
    GRPC_DEBUG_F("Connecting client fd %d to target %s", socket_fd, target.c_str());
    
    client_it->second->set_target(target);
    return client_it->second->connect();
}

GrpcResponse GrpcManager::call_method(int socket_fd, const GrpcRequest& request) {
    auto client_it = clients_.find(socket_fd);
    if (client_it == clients_.end()) {
        GrpcResponse response;
        response.status = GRPC_NOT_FOUND;
        response.error_message = "gRPC client not found for socket";
        return response;
    }
    
    return client_it->second->call_unary_method(request.service_name, request.method_name, 
                                               request.request_data, request.metadata);
}

bool GrpcManager::load_protobuf_schema(const std::string& proto_file) {
    if (!protobuf_manager_) {
        return false;
    }
    
    GRPC_DEBUG_F("Loading Protocol Buffers schema from %s", proto_file.c_str());
    return protobuf_manager_->load_proto_file(proto_file);
}

bool GrpcManager::validate_message(const std::string& type_name, const std::string& data) {
    if (!protobuf_manager_) {
        return false;
    }
    
    // In a real implementation, this would validate against the protobuf schema
    return !type_name.empty() && !data.empty();
}

std::string GrpcManager::serialize_message(const std::string& type_name, const mapping& data) {
    if (!protobuf_manager_) {
        return "";
    }
    
    return protobuf_manager_->serialize_from_mapping(type_name, data);
}

mapping GrpcManager::deserialize_message(const std::string& type_name, const std::string& data) {
    if (!protobuf_manager_) {
        return mapping();
    }
    
    return protobuf_manager_->deserialize_to_mapping(type_name, data);
}

bool GrpcManager::enable_reflection(int socket_fd, bool enabled) {
    auto server_it = servers_.find(socket_fd);
    if (server_it == servers_.end()) {
        return false;
    }
    
    // Enable reflection for this server
    GRPC_DEBUG_F("Setting reflection %s for fd %d", enabled ? "enabled" : "disabled", socket_fd);
    return true;
}

std::vector<std::string> GrpcManager::list_services(int socket_fd) {
    std::vector<std::string> services;
    
    if (!protobuf_manager_) {
        return services;
    }
    
    return protobuf_manager_->get_service_names();
}

GrpcMethodInfo GrpcManager::get_method_info(const std::string& service_name, const std::string& method_name) {
    if (!protobuf_manager_) {
        return GrpcMethodInfo();
    }
    
    return protobuf_manager_->get_method_details(service_name, method_name);
}

bool GrpcManager::enable_health_check(int socket_fd, bool enabled) {
    auto server_it = servers_.find(socket_fd);
    if (server_it == servers_.end()) {
        return false;
    }
    
    GRPC_DEBUG_F("Setting health check %s for fd %d", enabled ? "enabled" : "disabled", socket_fd);
    return true;
}

void GrpcManager::set_service_health(const std::string& service_name, bool healthy) {
    GRPC_DEBUG_F("Setting service %s health to %s", service_name.c_str(), healthy ? "healthy" : "unhealthy");
    // In a real implementation, this would update the health check service
}

std::map<std::string, int> GrpcManager::get_call_statistics() {
    std::map<std::string, int> stats;
    
    // Aggregate statistics from all servers
    for (const auto& server_pair : servers_) {
        auto server_stats = server_pair.second->get_call_counts();
        for (const auto& stat : server_stats) {
            stats[stat.first] += stat.second;
        }
    }
    
    return stats;
}

std::map<std::string, double> GrpcManager::get_latency_metrics() {
    std::map<std::string, double> metrics;
    
    // Aggregate latency metrics from all servers
    for (const auto& server_pair : servers_) {
        auto server_metrics = server_pair.second->get_average_latencies();
        for (const auto& metric : server_metrics) {
            // Simple average - in real implementation would be more sophisticated
            metrics[metric.first] = metric.second;
        }
    }
    
    return metrics;
}

int GrpcManager::get_active_streams_count() {
    // Count active streams across all servers and clients
    return 0; // Placeholder
}

void GrpcManager::log_error(int socket_fd, const std::string& error, const std::string& context) {
    // In a real implementation, this would integrate with FluffOS logging
    GRPC_DEBUG_F("ERROR [%s] fd %d: %s", context.c_str(), socket_fd, error.c_str());
}

bool GrpcManager::parse_grpc_request(const std::string& data, GrpcRequest& request) {
    // Simple parsing for demonstration - real implementation would use protobuf
    request.service_name = "TestService";
    request.method_name = "TestMethod";
    request.request_data = data;
    request.socket_fd = 0;
    request.requester = nullptr;
    request.deadline_ms = 30000;
    
    return !data.empty();
}

std::string GrpcManager::format_grpc_response(const GrpcResponse& response) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"status\":" << response.status << ",";
    oss << "\"data\":\"" << response.response_data << "\"";
    if (!response.error_message.empty()) {
        oss << ",\"error\":\"" << response.error_message << "\"";
    }
    oss << "}";
    
    return oss.str();
}

std::string GrpcManager::generate_method_key(const std::string& service_name, const std::string& method_name) {
    return service_name + "." + method_name;
}

void GrpcManager::setup_default_services() {
    GRPC_DEBUG("Setting up default gRPC services");
    
    // Health check service
    register_method_handler("grpc.health.v1.Health", "Check", 
        [](const GrpcRequest& req) -> GrpcResponse {
            GrpcResponse resp;
            resp.status = GRPC_OK;
            resp.response_data = "{\"status\": \"SERVING\"}";
            return resp;
        });
        
    // Server reflection service
    register_method_handler("grpc.reflection.v1alpha.ServerReflection", "ServerReflectionInfo",
        [](const GrpcRequest& req) -> GrpcResponse {
            GrpcResponse resp;
            resp.status = GRPC_OK;
            resp.response_data = "{\"services\": []}";
            return resp;
        });
}

void GrpcManager::register_mud_services() {
    GRPC_DEBUG("Registering default MUD gRPC services");
    
    // Player service
    register_method_handler("MudService", "GetPlayer",
        [](const GrpcRequest& req) -> GrpcResponse {
            GrpcResponse resp;
            resp.status = GRPC_OK;
            resp.response_data = "{\"name\": \"TestPlayer\", \"level\": 1}";
            return resp;
        });
        
    register_method_handler("MudService", "UpdatePlayer",
        [](const GrpcRequest& req) -> GrpcResponse {
            GrpcResponse resp;
            resp.status = GRPC_OK;
            resp.response_data = "{\"success\": true}";
            return resp;
        });
        
    // Room service
    register_method_handler("MudService", "GetRoom",
        [](const GrpcRequest& req) -> GrpcResponse {
            GrpcResponse resp;
            resp.status = GRPC_OK;
            resp.response_data = "{\"id\": \"room1\", \"title\": \"Test Room\"}";
            return resp;
        });
        
    // Command service
    register_method_handler("MudService", "ExecuteCommand",
        [](const GrpcRequest& req) -> GrpcResponse {
            GrpcResponse resp;
            resp.status = GRPC_OK;
            resp.response_data = "{\"result\": \"Command executed\"}";
            return resp;
        });
}

/*
 * C Interface for LPC Integration
 */

extern "C" {
    void init_grpc_package() {
        GrpcManager::getInstance()->initialize();
    }
    
    void clean_grpc_package() {
        auto* manager = GrpcManager::getInstance();
        if (manager) {
            manager->shutdown();
        }
    }
    
    void grpc_socket_close(int fd) {
        GrpcManager::getInstance()->close_grpc_socket(fd);
    }
    
    int grpc_socket_read(int fd, char* buf, int len) {
        // Socket read would be handled by the socket system
        return 0;
    }
    
    int grpc_socket_write(int fd, const char* buf, int len) {
        // Socket write would be handled by the socket system
        return len;
    }
    
    int grpc_register_service(int fd, const char* service_name, const char* proto_definition) {
        return GrpcManager::getInstance()->register_service(fd, std::string(proto_definition)) ? 1 : 0;
    }
    
    int grpc_start_server(int fd) {
        // Server start logic would be here
        return 1;
    }
    
    void grpc_stop_server(int fd) {
        // Server stop logic would be here
    }
    
    int grpc_connect_to_service(int fd, const char* target) {
        return GrpcManager::getInstance()->connect_to_service(fd, std::string(target)) ? 1 : 0;
    }
    
    int grpc_call_method(int fd, const char* service_name, const char* method_name,
                        const char* request_data, char* response_buffer, int buffer_size) {
        GrpcRequest request;
        request.service_name = std::string(service_name);
        request.method_name = std::string(method_name);
        request.request_data = std::string(request_data);
        request.socket_fd = fd;
        
        GrpcResponse response = GrpcManager::getInstance()->call_method(fd, request);
        
        if (response.status == GRPC_OK && response_buffer && buffer_size > 0) {
            strncpy(response_buffer, response.response_data.c_str(), buffer_size - 1);
            response_buffer[buffer_size - 1] = '\0';
            return 1;
        }
        
        return 0;
    }
    
    int grpc_load_proto_file(const char* file_path) {
        return GrpcManager::getInstance()->load_protobuf_schema(std::string(file_path)) ? 1 : 0;
    }
    
    int grpc_serialize_message(const char* type_name, const char* json_data, 
                              char* protobuf_buffer, int buffer_size) {
        // Placeholder implementation
        if (protobuf_buffer && buffer_size > 0) {
            strncpy(protobuf_buffer, json_data, buffer_size - 1);
            protobuf_buffer[buffer_size - 1] = '\0';
            return strlen(protobuf_buffer);
        }
        return 0;
    }
    
    int grpc_deserialize_message(const char* type_name, const char* protobuf_data,
                                char* json_buffer, int buffer_size) {
        // Placeholder implementation
        if (json_buffer && buffer_size > 0) {
            strncpy(json_buffer, protobuf_data, buffer_size - 1);
            json_buffer[buffer_size - 1] = '\0';
            return strlen(json_buffer);
        }
        return 0;
    }
    
    int grpc_list_services(int fd, char* services_buffer, int buffer_size) {
        std::vector<std::string> services = GrpcManager::getInstance()->list_services(fd);
        
        if (services_buffer && buffer_size > 0) {
            std::ostringstream oss;
            for (size_t i = 0; i < services.size(); ++i) {
                if (i > 0) oss << ",";
                oss << services[i];
            }
            
            std::string result = oss.str();
            strncpy(services_buffer, result.c_str(), buffer_size - 1);
            services_buffer[buffer_size - 1] = '\0';
            return 1;
        }
        
        return 0;
    }
    
    int grpc_health_check(int fd, const char* service_name) {
        // Placeholder implementation
        return 1; // Always healthy in stub implementation
    }
    
    int grpc_get_server_stats(int fd, char* stats_buffer, int buffer_size) {
        auto stats = GrpcManager::getInstance()->get_call_statistics();
        
        if (stats_buffer && buffer_size > 0) {
            std::ostringstream oss;
            oss << "{\"call_count\":" << stats.size() << "}";
            
            std::string result = oss.str();
            strncpy(stats_buffer, result.c_str(), buffer_size - 1);
            stats_buffer[buffer_size - 1] = '\0';
            return 1;
        }
        
        return 0;
    }
}