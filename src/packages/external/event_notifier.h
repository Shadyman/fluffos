#ifndef PACKAGES_EXTERNAL_EVENT_NOTIFIER_H_
#define PACKAGES_EXTERNAL_EVENT_NOTIFIER_H_

#include <memory>
#include <map>
#include <unordered_map>
#include <functional>
#include <string>

// Forward declarations for FluffOS types
struct svalue_t;

#ifndef _WIN32
#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>
#endif

/*
 * Event Notifier for External Process Package - Phase 2
 * 
 * This class provides high-performance event notification using Linux eventfd(2)
 * to enhance the EXTERNAL_ASYNC socket option. It replaces polling-based async
 * operations with event-driven notifications for better scalability and reduced
 * CPU usage.
 * 
 * Features:
 * - eventfd integration for async process communication
 * - Signal-based event delivery to socket callbacks  
 * - Integration with existing FileMonitor system
 * - Thread-safe event signaling and waiting
 * - Automatic cleanup on socket closure
 * - Fallback to polling on non-Linux platforms
 */

// Event types for async notifications
enum class AsyncEventType {
    PROCESS_READY = 1,      // Process spawned and ready
    PROCESS_OUTPUT = 2,     // Process has output available
    PROCESS_ERROR = 4,      // Process error occurred
    PROCESS_EXITED = 8,     // Process has exited
    FILE_CHANGED = 16,      // File monitoring event (integration with FileMonitor)
    CUSTOM_SIGNAL = 32      // Custom application signal
};

// Event data structure for async notifications
struct AsyncEvent {
    int socket_fd;              // Socket that triggered the event
    AsyncEventType event_type;  // Type of event that occurred
    uint64_t event_value;       // Event-specific value (e.g., exit code, bytes available)
    std::string data;           // Optional event data (e.g., error message)
    time_t timestamp;           // When the event occurred
    
    AsyncEvent() : socket_fd(-1), event_type(AsyncEventType::PROCESS_READY), 
                   event_value(0), timestamp(0) {}
};

/*
 * EventNotifier Class - eventfd integration for high-performance async notifications
 * 
 * Provides eventfd-based event notification system to enhance EXTERNAL_ASYNC
 * with better performance characteristics than polling-based approaches.
 */
class EventNotifier {
public:
    EventNotifier();
    ~EventNotifier();
    
    // Initialization and cleanup
    bool initialize();
    void shutdown();
    bool is_initialized() const { return event_fd_ != -1; }
    
    // Event signaling
    bool signal_event(AsyncEventType event_type, uint64_t value = 1);
    bool signal_socket_event(int socket_fd, AsyncEventType event_type, uint64_t value = 1);
    
    // Event waiting and processing
    bool wait_for_event(int timeout_ms = -1);
    std::vector<AsyncEvent> get_pending_events();
    bool has_pending_events() const;
    
    // Socket registration for async notifications
    bool register_socket(int socket_fd);
    void unregister_socket(int socket_fd);
    void unregister_all_sockets();
    
    // Integration with existing file monitoring
    bool enable_file_event_integration(bool enable = true);
    
    // Configuration
    void set_max_pending_events(size_t max_events) { max_pending_events_ = max_events; }
    
    // File descriptor for integration with main event loop
    int get_event_fd() const { return event_fd_; }
    
    // Statistics
    size_t get_registered_socket_count() const { return registered_sockets_.size(); }
    size_t get_pending_event_count() const { return pending_events_.size(); }
    uint64_t get_total_events_processed() const { return total_events_processed_; }
    
    // Static instance management
    static EventNotifier& instance();
    static bool initialize_global_notifier();
    static void shutdown_global_notifier();

private:
    int event_fd_;                                      // eventfd file descriptor
    std::map<int, bool> registered_sockets_;           // socket_fd -> registered flag
    std::vector<AsyncEvent> pending_events_;           // Queue of pending events
    bool file_integration_enabled_;                     // File monitor integration flag
    size_t max_pending_events_;                         // Maximum queued events
    uint64_t total_events_processed_;                   // Statistics counter
    
    static std::unique_ptr<EventNotifier> global_instance_;
    
    // Helper methods
    bool create_eventfd();
    void cleanup_eventfd();
    uint64_t read_eventfd_value();
    bool write_eventfd_value(uint64_t value);
    void queue_event(const AsyncEvent& event);
    void cleanup_old_events();
};

/*
 * AsyncEventManager - Integration with External Package
 * 
 * Manages async event notifications for external process sockets and handles
 * the enhanced EXTERNAL_ASYNC socket option with eventfd integration.
 */
class AsyncEventManager {
public:
    // Socket option handling
    static bool handle_async_option(int socket_fd, bool enable_async);
    static void cleanup_socket_events(int socket_fd);
    
    // Event delivery to socket callbacks
    static void deliver_async_events(int socket_fd, const std::vector<AsyncEvent>& events);
    
    // Process event signaling
    static bool signal_process_ready(int socket_fd);
    static bool signal_process_output(int socket_fd, size_t bytes_available);
    static bool signal_process_error(int socket_fd, const std::string& error_message);
    static bool signal_process_exited(int socket_fd, int exit_code);
    
    // File monitoring integration
    static bool signal_file_changed(int socket_fd, const std::string& file_path);
    
    // LPC interface functions
    static int external_wait_for_events(int socket_fd, int timeout_ms);
    static std::vector<AsyncEvent> external_get_async_events(int socket_fd);
    static int external_enable_async_notifications(int socket_fd, bool enable);
    
    // Event processing integration
    static void process_async_events();
    
private:
    static std::map<int, std::vector<AsyncEvent>> socket_events_;
    static void queue_socket_event(int socket_fd, const AsyncEvent& event);
    static bool is_socket_async_enabled(int socket_fd);
};

/*
 * Enhanced process information with eventfd integration
 */
struct AsyncProcessInfo {
    int socket_fd;
    int event_fd;                   // Per-process eventfd (optional)
    bool async_enabled;             // Enhanced async mode with eventfd
    bool file_monitoring_enabled;   // Integrated file monitoring
    
    // Event statistics
    uint64_t events_signaled;
    uint64_t events_delivered;
    time_t last_event_time;
    
    AsyncProcessInfo() : socket_fd(-1), event_fd(-1), async_enabled(false),
                        file_monitoring_enabled(false), events_signaled(0),
                        events_delivered(0), last_event_time(0) {}
};

/*
 * Utility functions for async event integration
 */
namespace AsyncEventUtils {
    // Event type conversion
    std::string event_type_to_string(AsyncEventType type);
    AsyncEventType string_to_event_type(const std::string& type_str);
    
    // Event validation and filtering
    bool is_valid_event_type(AsyncEventType type);
    bool should_deliver_event(int socket_fd, const AsyncEvent& event);
    
    // Performance utilities
    bool is_eventfd_supported();
    int get_optimal_timeout_ms(size_t pending_events);
    
    // Integration helpers
    bool integrate_with_file_monitor();
    void setup_main_loop_integration(int event_fd);
}

// Global initialization functions  
bool init_async_event_system();
void cleanup_async_event_system();

// Socket option registration functions
void register_enhanced_external_async_handler();
bool validate_enhanced_external_async(const svalue_t* value);

// Main event loop integration function
void process_external_async_events();

#endif  // PACKAGES_EXTERNAL_EVENT_NOTIFIER_H_