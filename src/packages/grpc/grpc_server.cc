/*
 * gRPC Server Implementation
 * 
 * Handles individual gRPC server instances per socket in unified socket architecture.
 */

#include "grpc.h"
#include <chrono>

/*
 * gRPC Server Implementation
 */

GrpcServer::GrpcServer(int socket_fd) 
    : socket_fd_(socket_fd), max_message_size_(4194304), compression_algorithm_(""),
      keepalive_enabled_(false), keepalive_time_ms_(30000), keepalive_timeout_ms_(5000),
      reflection_enabled_(true), health_check_enabled_(true), running_(false), configured_(false) {
    
    GRPC_DEBUG_F("Creating gRPC server for socket %d", socket_fd);
}

GrpcServer::~GrpcServer() {
    GRPC_DEBUG_F("Destroying gRPC server for socket %d", socket_fd_);
    stop_server();
}

bool GrpcServer::configure(std::unique_ptr<SocketOptionManager> option_manager) {
    option_manager_ = std::move(option_manager);
    
    if (!option_manager_) {
        return false;
    }
    
    GRPC_DEBUG_F("Configuring gRPC server for socket %d", socket_fd_);
    
    // Sync configuration from socket options
    std::string service_config;
    if (option_manager_->get_option(GRPC_SERVICE_CONFIG, service_config)) {
        set_service_config(service_config);
    }
    
    int max_size;
    if (option_manager_->get_option(GRPC_MAX_MESSAGE_SIZE, max_size)) {
        set_max_message_size(static_cast<size_t>(max_size));
    }
    
    std::string compression;
    if (option_manager_->get_option(GRPC_COMPRESSION, compression)) {
        set_compression_algorithm(compression);
    }
    
    int keepalive_time;
    if (option_manager_->get_option(GRPC_KEEPALIVE_TIME, keepalive_time)) {
        int keepalive_timeout;
        option_manager_->get_option(GRPC_KEEPALIVE_TIMEOUT, keepalive_timeout);
        enable_keepalive(true, static_cast<uint32_t>(keepalive_time), static_cast<uint32_t>(keepalive_timeout));
    }
    
    int reflection;
    if (option_manager_->get_option(GRPC_REFLECTION_ENABLE, reflection)) {
        reflection_enabled_ = (reflection != 0);
    }
    
    int health_check;
    if (option_manager_->get_option(GRPC_HEALTH_CHECK, health_check)) {
        health_check_enabled_ = (health_check != 0);
    }
    
    configured_ = true;
    GRPC_DEBUG_F("gRPC server configured for socket %d", socket_fd_);
    return true;
}

void GrpcServer::set_service_config(const std::string& service_definition) {
    service_config_ = service_definition;
    GRPC_DEBUG_F("Service config set for socket %d", socket_fd_);
}

void GrpcServer::set_max_message_size(size_t max_size) {
    max_message_size_ = max_size;
    GRPC_DEBUG_F("Max message size set to %zu for socket %d", max_size, socket_fd_);
}

void GrpcServer::set_compression_algorithm(const std::string& algorithm) {
    compression_algorithm_ = algorithm;
    GRPC_DEBUG_F("Compression algorithm set to %s for socket %d", algorithm.c_str(), socket_fd_);
}

void GrpcServer::enable_keepalive(bool enabled, uint32_t time_ms, uint32_t timeout_ms) {
    keepalive_enabled_ = enabled;
    keepalive_time_ms_ = time_ms;
    keepalive_timeout_ms_ = timeout_ms;
    GRPC_DEBUG_F("Keepalive %s for socket %d (time: %u ms, timeout: %u ms)", 
                 enabled ? "enabled" : "disabled", socket_fd_, time_ms, timeout_ms);
}

bool GrpcServer::register_service(const std::string& service_name, const std::string& proto_definition) {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    registered_services_[service_name] = proto_definition;
    GRPC_DEBUG_F("Registered service %s for socket %d", service_name.c_str(), socket_fd_);
    return true;
}

bool GrpcServer::register_method(const std::string& service_name, const std::string& method_name,
                                GrpcServiceHandler handler) {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    std::string key = service_name + "." + method_name;
    method_handlers_[key] = handler;
    
    // Create method info
    GrpcMethodInfo info;
    info.service_name = service_name;
    info.method_name = method_name;
    info.full_method = "/" + service_name + "/" + method_name;
    info.call_type = GRPC_UNARY;
    info.request_type = "Request";
    info.response_type = "Response";
    info.requires_auth = false;
    
    method_info_[key] = info;
    
    GRPC_DEBUG_F("Registered method %s for socket %d", key.c_str(), socket_fd_);
    return true;
}

GrpcResponse GrpcServer::handle_request(const GrpcRequest& request) {
    GRPC_DEBUG_F("Handling request for socket %d: %s.%s", 
                 socket_fd_, request.service_name.c_str(), request.method_name.c_str());
    
    auto start_time = std::chrono::steady_clock::now();
    
    GrpcResponse response;
    response.status = GRPC_OK;
    
    // Validate request
    if (!validate_request(request)) {
        return create_error_response(GRPC_INVALID_ARGUMENT, "Invalid request");
    }
    
    // Check message size
    if (request.request_data.size() > max_message_size_) {
        return create_error_response(GRPC_RESOURCE_EXHAUSTED, "Message too large");
    }
    
    // Handle health check requests
    if (request.service_name == "grpc.health.v1.Health" && health_check_enabled_) {
        std::string health_response;
        if (handle_health_check(request.method_name, health_response)) {
            response.response_data = health_response;
            record_call(request.service_name + "." + request.method_name, 
                       std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - start_time).count());
            return response;
        }
    }
    
    // Handle reflection requests
    if (request.service_name == "grpc.reflection.v1alpha.ServerReflection" && reflection_enabled_) {
        std::string reflection_response;
        if (handle_reflection_request(request.method_name, request.request_data, reflection_response)) {
            response.response_data = reflection_response;
            record_call(request.service_name + "." + request.method_name,
                       std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - start_time).count());
            return response;
        }
    }
    
    // Find method handler
    std::string method_key = request.service_name + "." + request.method_name;
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    auto handler_it = method_handlers_.find(method_key);
    if (handler_it == method_handlers_.end()) {
        return create_error_response(GRPC_UNIMPLEMENTED, "Method not implemented: " + method_key);
    }
    
    // Execute method handler
    try {
        response = handler_it->second(request);
        
        // Apply compression if enabled
        if (!compression_algorithm_.empty() && compression_algorithm_ != "none") {
            apply_compression(response.response_data, compression_algorithm_);
        }
        
        // Record call statistics
        record_call(method_key, std::chrono::duration<double, std::milli>(
                       std::chrono::steady_clock::now() - start_time).count());
        
    } catch (const std::exception& e) {
        return create_error_response(GRPC_INTERNAL, "Handler error: " + std::string(e.what()));
    }
    
    return response;
}

bool GrpcServer::handle_streaming_request(const GrpcRequest& request) {
    GRPC_DEBUG_F("Handling streaming request for socket %d", socket_fd_);
    
    // Streaming implementation would go here
    return true;
}

bool GrpcServer::handle_reflection_request(const std::string& method, const std::string& request_data,
                                          std::string& response_data) {
    GRPC_DEBUG_F("Handling reflection request: %s", method.c_str());
    
    if (method == "ServerReflectionInfo") {
        // Return list of available services
        std::ostringstream oss;
        oss << "{\"valid_hosts\": [\"\"], \"file_descriptor_response\": {\"file_descriptor_proto\": []}}";
        response_data = oss.str();
        return true;
    }
    
    return false;
}

bool GrpcServer::handle_health_check(const std::string& service_name, std::string& response) {
    GRPC_DEBUG_F("Handling health check for service: %s", service_name.c_str());
    
    // Simple health check - always return serving
    response = "{\"status\": \"SERVING\"}";
    return true;
}

bool GrpcServer::start_server() {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    if (running_) {
        return true;
    }
    
    if (!configured_) {
        GRPC_DEBUG_F("Cannot start unconfigured server for socket %d", socket_fd_);
        return false;
    }
    
    GRPC_DEBUG_F("Starting gRPC server for socket %d", socket_fd_);
    running_ = true;
    return true;
}

void GrpcServer::stop_server() {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    if (!running_) {
        return;
    }
    
    GRPC_DEBUG_F("Stopping gRPC server for socket %d", socket_fd_);
    running_ = false;
}

bool GrpcServer::is_running() const {
    std::lock_guard<std::mutex> lock(server_mutex_);
    return running_;
}

std::string GrpcServer::get_server_status() const {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    std::ostringstream oss;
    oss << "gRPC Server Status for socket " << socket_fd_ << ":\n";
    oss << "  Running: " << (running_ ? "Yes" : "No") << "\n";
    oss << "  Configured: " << (configured_ ? "Yes" : "No") << "\n";
    oss << "  Registered services: " << registered_services_.size() << "\n";
    oss << "  Method handlers: " << method_handlers_.size() << "\n";
    oss << "  Max message size: " << max_message_size_ << " bytes\n";
    oss << "  Compression: " << (compression_algorithm_.empty() ? "None" : compression_algorithm_) << "\n";
    oss << "  Keepalive: " << (keepalive_enabled_ ? "Enabled" : "Disabled") << "\n";
    oss << "  Reflection: " << (reflection_enabled_ ? "Enabled" : "Disabled") << "\n";
    oss << "  Health check: " << (health_check_enabled_ ? "Enabled" : "Disabled");
    
    return oss.str();
}

void GrpcServer::record_call(const std::string& method_name, double latency_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    call_counts_[method_name]++;
    total_latencies_[method_name] += latency_ms;
}

std::map<std::string, int> GrpcServer::get_call_counts() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return call_counts_;
}

std::map<std::string, double> GrpcServer::get_average_latencies() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::map<std::string, double> averages;
    for (const auto& pair : total_latencies_) {
        const std::string& method = pair.first;
        double total_latency = pair.second;
        
        auto count_it = call_counts_.find(method);
        if (count_it != call_counts_.end() && count_it->second > 0) {
            averages[method] = total_latency / count_it->second;
        }
    }
    
    return averages;
}

bool GrpcServer::validate_request(const GrpcRequest& request) {
    // Basic validation
    if (request.service_name.empty() || request.method_name.empty()) {
        return false;
    }
    
    // Check deadline
    if (request.deadline_ms > 0) {
        // In a real implementation, this would check against current time
    }
    
    return true;
}

GrpcResponse GrpcServer::create_error_response(GrpcStatus status, const std::string& message) {
    GrpcResponse response;
    response.status = status;
    response.error_message = message;
    response.response_data = "";
    
    GRPC_DEBUG_F("Created error response: %d - %s", status, message.c_str());
    return response;
}

void GrpcServer::apply_compression(std::string& data, const std::string& algorithm) {
    if (algorithm == "gzip") {
        // Placeholder - would implement gzip compression
        GRPC_DEBUG_F("Applying gzip compression to %zu bytes", data.size());
    } else if (algorithm == "deflate") {
        // Placeholder - would implement deflate compression
        GRPC_DEBUG_F("Applying deflate compression to %zu bytes", data.size());
    }
}

bool GrpcServer::decompress_data(std::string& data, const std::string& algorithm) {
    if (algorithm == "gzip") {
        // Placeholder - would implement gzip decompression
        GRPC_DEBUG_F("Decompressing gzip data of %zu bytes", data.size());
        return true;
    } else if (algorithm == "deflate") {
        // Placeholder - would implement deflate decompression
        GRPC_DEBUG_F("Decompressing deflate data of %zu bytes", data.size());
        return true;
    }
    
    return false;
}