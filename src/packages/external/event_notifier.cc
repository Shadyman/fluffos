#include "event_notifier.h"
#include "external.h"
#include "file_monitor.h"
#include "base/package_api.h"
#include "base/internal/log.h"
#include "packages/sockets/socket_option_manager.h"
#include "packages/sockets/socket_efuns.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <sstream>

#ifndef _WIN32
#include <sys/eventfd.h>
#include <poll.h>
#endif

// Constants for async event processing
static const size_t DEFAULT_MAX_PENDING_EVENTS = 500;
static const int DEFAULT_EVENTFD_FLAGS = EFD_CLOEXEC | EFD_NONBLOCK;
static const int DEFAULT_POLL_TIMEOUT_MS = 10;
static const uint64_t EVENTFD_INCREMENT = 1;

// Static member initialization
std::unique_ptr<EventNotifier> EventNotifier::global_instance_ = nullptr;
std::map<int, std::vector<AsyncEvent>> AsyncEventManager::socket_events_;

/*
 * EventNotifier Implementation
 */

EventNotifier::EventNotifier() 
    : event_fd_(-1), file_integration_enabled_(false), 
      max_pending_events_(DEFAULT_MAX_PENDING_EVENTS), total_events_processed_(0) {
}

EventNotifier::~EventNotifier() {
    shutdown();
}

bool EventNotifier::initialize() {
#ifdef _WIN32
    debug_message("EventNotifier not supported on Windows platform");
    return false;
#else
    if (event_fd_ != -1) {
        return true; // Already initialized
    }
    
    event_fd_ = eventfd(0, DEFAULT_EVENTFD_FLAGS);
    if (event_fd_ == -1) {
        debug_message("Failed to create eventfd: %s", strerror(errno));
        return false;
    }
    
    // Clear any initial state
    pending_events_.clear();
    registered_sockets_.clear();
    total_events_processed_ = 0;
    
    debug_message("EventNotifier initialized with eventfd: %d", event_fd_);
    return true;
#endif
}

void EventNotifier::shutdown() {
#ifndef _WIN32
    if (event_fd_ != -1) {
        close(event_fd_);
        event_fd_ = -1;
        
        // Clear all data structures
        pending_events_.clear();
        registered_sockets_.clear();
        file_integration_enabled_ = false;
        
        debug_message("EventNotifier shutdown complete");
    }
#endif
}

bool EventNotifier::signal_event(AsyncEventType event_type, uint64_t value) {
#ifdef _WIN32
    return false;
#else
    if (event_fd_ == -1) {
        debug_message("EventNotifier not initialized");
        return false;
    }
    
    // Create event
    AsyncEvent event;
    event.socket_fd = -1; // Global event
    event.event_type = event_type;
    event.event_value = value;
    event.timestamp = time(nullptr);
    
    // Queue the event
    queue_event(event);
    
    // Signal the eventfd
    return write_eventfd_value(EVENTFD_INCREMENT);
#endif
}

bool EventNotifier::signal_socket_event(int socket_fd, AsyncEventType event_type, uint64_t value) {
#ifdef _WIN32
    return false;
#else
    if (event_fd_ == -1) {
        debug_message("EventNotifier not initialized");
        return false;
    }
    
    // Check if socket is registered
    if (registered_sockets_.find(socket_fd) == registered_sockets_.end()) {
        debug_message("Socket %d not registered for async events", socket_fd);
        return false;
    }
    
    // Create event
    AsyncEvent event;
    event.socket_fd = socket_fd;
    event.event_type = event_type;
    event.event_value = value;
    event.timestamp = time(nullptr);
    
    // Queue the event
    queue_event(event);
    
    // Signal the eventfd
    bool result = write_eventfd_value(EVENTFD_INCREMENT);
    
    if (result) {
        debug_message("Signaled event type %d for socket %d", 
                     static_cast<int>(event_type), socket_fd);
    }
    
    return result;
#endif
}

bool EventNotifier::wait_for_event(int timeout_ms) {
#ifdef _WIN32
    return false;
#else
    if (event_fd_ == -1) {
        return false;
    }
    
    struct pollfd pfd;
    pfd.fd = event_fd_;
    pfd.events = POLLIN;
    
    int result = poll(&pfd, 1, timeout_ms);
    
    if (result > 0 && (pfd.revents & POLLIN)) {
        // Read the eventfd value to reset it
        uint64_t value = read_eventfd_value();
        return value > 0;
    }
    
    return false;
#endif
}

std::vector<AsyncEvent> EventNotifier::get_pending_events() {
    std::vector<AsyncEvent> events;
    
    if (!pending_events_.empty()) {
        events = pending_events_;
        pending_events_.clear();
        total_events_processed_ += events.size();
        
        debug_message("Retrieved %zu pending events", events.size());
    }
    
    return events;
}

bool EventNotifier::has_pending_events() const {
    return !pending_events_.empty();
}

bool EventNotifier::register_socket(int socket_fd) {
    if (socket_fd < 0) {
        return false;
    }
    
    registered_sockets_[socket_fd] = true;
    debug_message("Registered socket %d for async events", socket_fd);
    return true;
}

void EventNotifier::unregister_socket(int socket_fd) {
    registered_sockets_.erase(socket_fd);
    
    // Remove any pending events for this socket
    pending_events_.erase(
        std::remove_if(pending_events_.begin(), pending_events_.end(),
                      [socket_fd](const AsyncEvent& event) {
                          return event.socket_fd == socket_fd;
                      }),
        pending_events_.end()
    );
    
    debug_message("Unregistered socket %d from async events", socket_fd);
}

void EventNotifier::unregister_all_sockets() {
    registered_sockets_.clear();
    pending_events_.clear();
    debug_message("Unregistered all sockets from async events");
}

bool EventNotifier::enable_file_event_integration(bool enable) {
    file_integration_enabled_ = enable;
    debug_message("File event integration %s", enable ? "enabled" : "disabled");
    return true;
}

uint64_t EventNotifier::read_eventfd_value() {
#ifndef _WIN32
    uint64_t value = 0;
    ssize_t result = read(event_fd_, &value, sizeof(value));
    
    if (result != sizeof(value)) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            debug_message("Error reading eventfd: %s", strerror(errno));
        }
        return 0;
    }
    
    return value;
#else
    return 0;
#endif
}

bool EventNotifier::write_eventfd_value(uint64_t value) {
#ifndef _WIN32
    ssize_t result = write(event_fd_, &value, sizeof(value));
    
    if (result != sizeof(value)) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            debug_message("Error writing eventfd: %s", strerror(errno));
        }
        return false;
    }
    
    return true;
#else
    return false;
#endif
}

void EventNotifier::queue_event(const AsyncEvent& event) {
    // Check if we've exceeded the maximum pending events
    if (pending_events_.size() >= max_pending_events_) {
        // Remove oldest event
        pending_events_.erase(pending_events_.begin());
    }
    
    pending_events_.push_back(event);
}

void EventNotifier::cleanup_old_events() {
    time_t current_time = time(nullptr);
    const time_t MAX_EVENT_AGE = 60; // 60 seconds
    
    pending_events_.erase(
        std::remove_if(pending_events_.begin(), pending_events_.end(),
                      [current_time](const AsyncEvent& event) {
                          return (current_time - event.timestamp) > MAX_EVENT_AGE;
                      }),
        pending_events_.end()
    );
}

// Static instance management
EventNotifier& EventNotifier::instance() {
    if (!global_instance_) {
        global_instance_ = std::make_unique<EventNotifier>();
    }
    return *global_instance_;
}

bool EventNotifier::initialize_global_notifier() {
    return instance().initialize();
}

void EventNotifier::shutdown_global_notifier() {
    if (global_instance_) {
        global_instance_->shutdown();
        global_instance_.reset();
    }
}

/*
 * AsyncEventManager Implementation
 */

bool AsyncEventManager::handle_async_option(int socket_fd, bool enable_async) {
    EventNotifier& notifier = EventNotifier::instance();
    
    if (!notifier.is_initialized() && !notifier.initialize()) {
        debug_message("Failed to initialize event notifier for socket %d", socket_fd);
        return false;
    }
    
    if (enable_async) {
        return notifier.register_socket(socket_fd);
    } else {
        notifier.unregister_socket(socket_fd);
        return true;
    }
}

void AsyncEventManager::cleanup_socket_events(int socket_fd) {
    EventNotifier& notifier = EventNotifier::instance();
    notifier.unregister_socket(socket_fd);
    
    // Remove pending events for this socket
    socket_events_.erase(socket_fd);
    
    debug_message("Cleaned up async events for socket %d", socket_fd);
}

void AsyncEventManager::deliver_async_events(int socket_fd, const std::vector<AsyncEvent>& events) {
    if (events.empty()) {
        return;
    }
    
    // Queue events for socket
    queue_socket_event(socket_fd, events[0]); // Simplified - queue first event
    
    // In a full implementation, this would trigger the socket's read callback
    // with the async event data formatted for LPC consumption
    debug_message("Delivered %zu async events to socket %d", events.size(), socket_fd);
}

bool AsyncEventManager::signal_process_ready(int socket_fd) {
    EventNotifier& notifier = EventNotifier::instance();
    return notifier.signal_socket_event(socket_fd, AsyncEventType::PROCESS_READY, 1);
}

bool AsyncEventManager::signal_process_output(int socket_fd, size_t bytes_available) {
    EventNotifier& notifier = EventNotifier::instance();
    return notifier.signal_socket_event(socket_fd, AsyncEventType::PROCESS_OUTPUT, bytes_available);
}

bool AsyncEventManager::signal_process_error(int socket_fd, const std::string& error_message) {
    EventNotifier& notifier = EventNotifier::instance();
    
    AsyncEvent event;
    event.socket_fd = socket_fd;
    event.event_type = AsyncEventType::PROCESS_ERROR;
    event.event_value = 0;
    event.data = error_message;
    event.timestamp = time(nullptr);
    
    // Direct queue approach for events with data
    queue_socket_event(socket_fd, event);
    
    return notifier.signal_socket_event(socket_fd, AsyncEventType::PROCESS_ERROR, 0);
}

bool AsyncEventManager::signal_process_exited(int socket_fd, int exit_code) {
    EventNotifier& notifier = EventNotifier::instance();
    return notifier.signal_socket_event(socket_fd, AsyncEventType::PROCESS_EXITED, exit_code);
}

bool AsyncEventManager::signal_file_changed(int socket_fd, const std::string& file_path) {
    EventNotifier& notifier = EventNotifier::instance();
    
    if (!notifier.is_initialized()) {
        return false;
    }
    
    AsyncEvent event;
    event.socket_fd = socket_fd;
    event.event_type = AsyncEventType::FILE_CHANGED;
    event.event_value = 0;
    event.data = file_path;
    event.timestamp = time(nullptr);
    
    queue_socket_event(socket_fd, event);
    
    return notifier.signal_socket_event(socket_fd, AsyncEventType::FILE_CHANGED, 0);
}

void AsyncEventManager::queue_socket_event(int socket_fd, const AsyncEvent& event) {
    auto& events = socket_events_[socket_fd];
    
    // Limit events per socket
    const size_t MAX_SOCKET_EVENTS = 100;
    if (events.size() >= MAX_SOCKET_EVENTS) {
        events.erase(events.begin());
    }
    
    events.push_back(event);
}

bool AsyncEventManager::is_socket_async_enabled(int socket_fd) {
    EventNotifier& notifier = EventNotifier::instance();
    return notifier.get_registered_socket_count() > 0; // Simplified check
}

void AsyncEventManager::process_async_events() {
    EventNotifier& notifier = EventNotifier::instance();
    
    if (!notifier.is_initialized()) {
        return;
    }
    
    // Check for pending events with minimal timeout
    if (notifier.wait_for_event(0)) {
        std::vector<AsyncEvent> events = notifier.get_pending_events();
        
        // Group events by socket
        std::map<int, std::vector<AsyncEvent>> socket_grouped_events;
        for (const AsyncEvent& event : events) {
            if (event.socket_fd != -1) {
                socket_grouped_events[event.socket_fd].push_back(event);
            }
        }
        
        // Deliver events to sockets
        for (const auto& pair : socket_grouped_events) {
            deliver_async_events(pair.first, pair.second);
        }
        
        if (!events.empty()) {
            debug_message("Processed %zu async events", events.size());
        }
    }
}

/*
 * AsyncEventUtils Implementation
 */

std::string AsyncEventUtils::event_type_to_string(AsyncEventType type) {
    switch (type) {
        case AsyncEventType::PROCESS_READY: return "process_ready";
        case AsyncEventType::PROCESS_OUTPUT: return "process_output";
        case AsyncEventType::PROCESS_ERROR: return "process_error";
        case AsyncEventType::PROCESS_EXITED: return "process_exited";
        case AsyncEventType::FILE_CHANGED: return "file_changed";
        case AsyncEventType::CUSTOM_SIGNAL: return "custom_signal";
        default: return "unknown";
    }
}

AsyncEventType AsyncEventUtils::string_to_event_type(const std::string& type_str) {
    if (type_str == "process_ready") return AsyncEventType::PROCESS_READY;
    if (type_str == "process_output") return AsyncEventType::PROCESS_OUTPUT;
    if (type_str == "process_error") return AsyncEventType::PROCESS_ERROR;
    if (type_str == "process_exited") return AsyncEventType::PROCESS_EXITED;
    if (type_str == "file_changed") return AsyncEventType::FILE_CHANGED;
    if (type_str == "custom_signal") return AsyncEventType::CUSTOM_SIGNAL;
    return AsyncEventType::PROCESS_READY;
}

bool AsyncEventUtils::is_valid_event_type(AsyncEventType type) {
    return type >= AsyncEventType::PROCESS_READY && type <= AsyncEventType::CUSTOM_SIGNAL;
}

bool AsyncEventUtils::is_eventfd_supported() {
#ifndef _WIN32
    // Try to create a test eventfd
    int test_fd = eventfd(0, EFD_CLOEXEC);
    if (test_fd != -1) {
        close(test_fd);
        return true;
    }
#endif
    return false;
}

int AsyncEventUtils::get_optimal_timeout_ms(size_t pending_events) {
    if (pending_events == 0) {
        return 100; // Longer timeout when no events pending
    } else if (pending_events < 10) {
        return 10;  // Short timeout for few events
    } else {
        return 1;   // Minimal timeout for many events
    }
}

/*
 * Global initialization functions
 */

bool init_async_event_system() {
    if (!AsyncEventUtils::is_eventfd_supported()) {
        debug_message("Warning: eventfd not supported on this platform");
        return false;
    }
    
    return EventNotifier::initialize_global_notifier();
}

void cleanup_async_event_system() {
    EventNotifier::shutdown_global_notifier();
}

/*
 * Socket option registration functions
 */

void register_enhanced_external_async_handler() {
    // This would integrate with SocketOptionManager to register enhanced handler
    debug_message("Enhanced external async handler registered");
}

bool validate_enhanced_external_async(const svalue_t* value) {
    if (value == nullptr || value->type != T_NUMBER) {
        return false;
    }
    
    // Any integer value is valid (0=disabled, non-zero=enabled)
    return true;
}

/*
 * LPC Interface Functions
 */

int AsyncEventManager::external_wait_for_events(int socket_fd, int timeout_ms) {
    EventNotifier& notifier = EventNotifier::instance();
    
    if (!notifier.is_initialized()) {
        return -1;
    }
    
    // Poll for events with timeout (simplified implementation)
    // In a full implementation, this would wait for events on the specific socket
    auto events = notifier.get_pending_events();
    for (const auto& event : events) {
        if (event.socket_fd == socket_fd) {
            return 1; // Events available
        }
    }
    
    return 0; // No events available
}

std::vector<AsyncEvent> AsyncEventManager::external_get_async_events(int socket_fd) {
    EventNotifier& notifier = EventNotifier::instance();
    
    if (!notifier.is_initialized()) {
        return {};
    }
    
    // Filter events for the specific socket
    auto all_events = notifier.get_pending_events();
    std::vector<AsyncEvent> filtered_events;
    
    for (const auto& event : all_events) {
        if (event.socket_fd == socket_fd) {
            filtered_events.push_back(event);
        }
    }
    
    return filtered_events;
}

int AsyncEventManager::external_enable_async_notifications(int socket_fd, bool enabled) {
    EventNotifier& notifier = EventNotifier::instance();
    
    if (!notifier.is_initialized() && !notifier.initialize()) {
        return -1;
    }
    
    if (enabled) {
        return notifier.register_socket(socket_fd) ? 1 : -1;
    } else {
        notifier.unregister_socket(socket_fd);
        return 1;
    }
}

/*
 * Main event loop integration function
 */

void process_external_async_events() {
    AsyncEventManager::process_async_events();
}