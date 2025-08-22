/*
 * gRPC Client Implementation
 * 
 * Handles gRPC client connections and method calls in unified socket architecture.
 */

#include "grpc.h"

/*
 * gRPC Client Implementation
 */

GrpcClient::GrpcClient(int socket_fd) 
    : socket_fd_(socket_fd), target_(""), deadline_ms_(30000), retry_policy_(""),
      compression_algorithm_(""), connected_(false), configured_(false),
      streaming_active_(false), active_stream_method_(""), active_stream_type_(GRPC_UNARY) {
    
    GRPC_DEBUG_F("Creating gRPC client for socket %d", socket_fd);
}

GrpcClient::~GrpcClient() {
    GRPC_DEBUG_F("Destroying gRPC client for socket %d", socket_fd_);
    disconnect();
}

bool GrpcClient::configure(std::unique_ptr<SocketOptionManager> option_manager) {
    option_manager_ = std::move(option_manager);
    
    if (!option_manager_) {
        return false;
    }
    
    GRPC_DEBUG_F("Configuring gRPC client for socket %d", socket_fd_);
    
    // Sync configuration from socket options
    std::string target;
    if (option_manager_->get_option(GRPC_TARGET_ADDRESS, target)) {
        set_target(target);
    }
    
    int deadline;
    if (option_manager_->get_option(GRPC_DEADLINE, deadline)) {
        set_deadline(static_cast<uint32_t>(deadline));
    }
    
    std::string retry_policy;
    if (option_manager_->get_option(GRPC_RETRY_POLICY, retry_policy)) {
        set_retry_policy(retry_policy);
    }
    
    std::string compression;
    if (option_manager_->get_option(GRPC_COMPRESSION, compression)) {
        set_compression(compression);
    }
    
    configured_ = true;
    GRPC_DEBUG_F("gRPC client configured for socket %d", socket_fd_);
    return true;
}

void GrpcClient::set_target(const std::string& target) {
    target_ = target;
    GRPC_DEBUG_F("Target set to %s for socket %d", target.c_str(), socket_fd_);
}

void GrpcClient::set_deadline(uint32_t deadline_ms) {
    deadline_ms_ = deadline_ms;
    GRPC_DEBUG_F("Deadline set to %u ms for socket %d", deadline_ms, socket_fd_);
}

void GrpcClient::set_retry_policy(const std::string& policy) {
    retry_policy_ = policy;
    GRPC_DEBUG_F("Retry policy set for socket %d: %s", socket_fd_, policy.c_str());
}

void GrpcClient::set_compression(const std::string& algorithm) {
    compression_algorithm_ = algorithm;
    GRPC_DEBUG_F("Compression algorithm set to %s for socket %d", algorithm.c_str(), socket_fd_);
}

bool GrpcClient::connect() {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (connected_) {
        return true;
    }
    
    if (target_.empty()) {
        handle_connection_error("Target address not set");
        return false;
    }
    
    GRPC_DEBUG_F("Connecting gRPC client socket %d to %s", socket_fd_, target_.c_str());
    
    // In a real implementation, this would establish the gRPC channel
#ifdef HAVE_GRPC
    // Would create grpc::Channel here
#endif
    
    connected_ = true;
    GRPC_DEBUG_F("gRPC client connected successfully for socket %d", socket_fd_);
    return true;
}

void GrpcClient::disconnect() {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (!connected_) {
        return;
    }
    
    GRPC_DEBUG_F("Disconnecting gRPC client for socket %d", socket_fd_);
    
    // Close any active streams
    if (streaming_active_) {
        streaming_active_ = false;
        active_stream_method_.clear();
        active_stream_type_ = GRPC_UNARY;
    }
    
    connected_ = false;
    GRPC_DEBUG_F("gRPC client disconnected for socket %d", socket_fd_);
}

bool GrpcClient::is_connected() const {
    std::lock_guard<std::mutex> lock(client_mutex_);
    return connected_;
}

GrpcResponse GrpcClient::call_unary_method(const std::string& service_name, const std::string& method_name,
                                          const std::string& request_data,
                                          const std::map<std::string, std::string>& metadata) {
    GRPC_DEBUG_F("Calling unary method %s.%s for socket %d", 
                 service_name.c_str(), method_name.c_str(), socket_fd_);
    
    GrpcResponse response;
    
    if (!is_connected()) {
        response.status = GRPC_UNAVAILABLE;
        response.error_message = "Client not connected";
        return response;
    }
    
    // Create method path
    std::string method_path = format_method_path(service_name, method_name);
    
    // Send request
    if (!send_request(method_path, request_data, metadata)) {
        response.status = GRPC_INTERNAL;
        response.error_message = "Failed to send request";
        return response;
    }
    
    // Receive response
    std::string response_data;
    std::map<std::string, std::string> response_metadata;
    if (!receive_response(response_data, response_metadata)) {
        response.status = GRPC_DEADLINE_EXCEEDED;
        response.error_message = "Failed to receive response or deadline exceeded";
        return response;
    }
    
    response.status = GRPC_OK;
    response.response_data = response_data;
    response.metadata = response_metadata;
    
    GRPC_DEBUG_F("Unary method call completed for socket %d", socket_fd_);
    return response;
}

bool GrpcClient::start_client_stream(const std::string& service_name, const std::string& method_name) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (streaming_active_) {
        GRPC_DEBUG_F("Stream already active for socket %d", socket_fd_);
        return false;
    }
    
    GRPC_DEBUG_F("Starting client stream %s.%s for socket %d", 
                 service_name.c_str(), method_name.c_str(), socket_fd_);
    
    streaming_active_ = true;
    active_stream_method_ = service_name + "." + method_name;
    active_stream_type_ = GRPC_CLIENT_STREAMING;
    
    return true;
}

bool GrpcClient::send_stream_message(const std::string& message_data) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (!streaming_active_) {
        GRPC_DEBUG_F("No active stream for socket %d", socket_fd_);
        return false;
    }
    
    GRPC_DEBUG_F("Sending stream message for socket %d (%zu bytes)", 
                 socket_fd_, message_data.size());
    
    // In a real implementation, this would send via the gRPC stream
    return true;
}

GrpcResponse GrpcClient::finish_client_stream() {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    GrpcResponse response;
    
    if (!streaming_active_ || active_stream_type_ != GRPC_CLIENT_STREAMING) {
        response.status = GRPC_FAILED_PRECONDITION;
        response.error_message = "No active client stream";
        return response;
    }
    
    GRPC_DEBUG_F("Finishing client stream for socket %d", socket_fd_);
    
    // In a real implementation, this would finish the stream and get the response
    streaming_active_ = false;
    active_stream_method_.clear();
    active_stream_type_ = GRPC_UNARY;
    
    response.status = GRPC_OK;
    response.response_data = "{\"stream_finished\": true}";
    
    return response;
}

bool GrpcClient::start_server_stream(const std::string& service_name, const std::string& method_name,
                                    const std::string& request_data) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (streaming_active_) {
        GRPC_DEBUG_F("Stream already active for socket %d", socket_fd_);
        return false;
    }
    
    GRPC_DEBUG_F("Starting server stream %s.%s for socket %d", 
                 service_name.c_str(), method_name.c_str(), socket_fd_);
    
    streaming_active_ = true;
    active_stream_method_ = service_name + "." + method_name;
    active_stream_type_ = GRPC_SERVER_STREAMING;
    
    // In a real implementation, this would send the initial request
    return true;
}

bool GrpcClient::read_stream_message(std::string& response_data) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (!streaming_active_ || active_stream_type_ != GRPC_SERVER_STREAMING) {
        GRPC_DEBUG_F("No active server stream for socket %d", socket_fd_);
        return false;
    }
    
    GRPC_DEBUG_F("Reading stream message for socket %d", socket_fd_);
    
    // In a real implementation, this would read from the gRPC stream
    response_data = "{\"stream_message\": \"data\"}";
    return true;
}

bool GrpcClient::start_bidirectional_stream(const std::string& service_name, const std::string& method_name) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (streaming_active_) {
        GRPC_DEBUG_F("Stream already active for socket %d", socket_fd_);
        return false;
    }
    
    GRPC_DEBUG_F("Starting bidirectional stream %s.%s for socket %d", 
                 service_name.c_str(), method_name.c_str(), socket_fd_);
    
    streaming_active_ = true;
    active_stream_method_ = service_name + "." + method_name;
    active_stream_type_ = GRPC_BIDIRECTIONAL_STREAMING;
    
    return true;
}

std::string GrpcClient::get_connection_status() const {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    std::ostringstream oss;
    oss << "gRPC Client Status for socket " << socket_fd_ << ":\n";
    oss << "  Connected: " << (connected_ ? "Yes" : "No") << "\n";
    oss << "  Configured: " << (configured_ ? "Yes" : "No") << "\n";
    oss << "  Target: " << target_ << "\n";
    oss << "  Deadline: " << deadline_ms_ << " ms\n";
    oss << "  Compression: " << (compression_algorithm_.empty() ? "None" : compression_algorithm_) << "\n";
    oss << "  Streaming active: " << (streaming_active_ ? "Yes" : "No") << "\n";
    if (streaming_active_) {
        oss << "  Active stream: " << active_stream_method_ << "\n";
        oss << "  Stream type: " << active_stream_type_;
    }
    
    return oss.str();
}

std::map<std::string, std::string> GrpcClient::get_channel_info() const {
    std::map<std::string, std::string> info;
    
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    info["target"] = target_;
    info["connected"] = connected_ ? "true" : "false";
    info["deadline_ms"] = std::to_string(deadline_ms_);
    info["compression"] = compression_algorithm_;
    info["retry_policy"] = retry_policy_;
    
    return info;
}

std::string GrpcClient::format_method_path(const std::string& service_name, const std::string& method_name) {
    return "/" + service_name + "/" + method_name;
}

bool GrpcClient::send_request(const std::string& method_path, const std::string& request_data,
                             const std::map<std::string, std::string>& metadata) {
    GRPC_DEBUG_F("Sending request to %s (%zu bytes) for socket %d", 
                 method_path.c_str(), request_data.size(), socket_fd_);
    
    // In a real implementation, this would send via gRPC channel
    // For now, just validate the data
    if (method_path.empty() || request_data.empty()) {
        return false;
    }
    
    // Check message size
    if (request_data.size() > 67108864) { // 64MB max
        handle_connection_error("Message too large");
        return false;
    }
    
    return true;
}

bool GrpcClient::receive_response(std::string& response_data, std::map<std::string, std::string>& metadata) {
    GRPC_DEBUG_F("Receiving response for socket %d", socket_fd_);
    
    // In a real implementation, this would receive via gRPC channel
    // For now, return a placeholder response
    response_data = "{\"result\": \"success\", \"timestamp\": \"" + 
                   std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count()) + "\"}";
    
    metadata["content-type"] = "application/grpc+proto";
    metadata["grpc-status"] = "0";
    
    return true;
}

void GrpcClient::handle_connection_error(const std::string& error) {
    GRPC_DEBUG_F("Connection error for socket %d: %s", socket_fd_, error.c_str());
    
    // In a real implementation, this would handle reconnection logic
    std::lock_guard<std::mutex> lock(client_mutex_);
    connected_ = false;
}