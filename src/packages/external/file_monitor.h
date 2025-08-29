#ifndef PACKAGES_EXTERNAL_FILE_MONITOR_H_
#define PACKAGES_EXTERNAL_FILE_MONITOR_H_

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>

// Forward declarations for FluffOS types
struct svalue_t;

#ifndef _WIN32
#include <sys/inotify.h>
#endif

/*
 * File Monitor for External Process Package
 * 
 * This class provides inotify-based file system monitoring for the
 * FluffOS unified socket architecture. It integrates with the external
 * package to provide real-time file change notifications through the
 * EXTERNAL_WATCH_PATH socket option (143).
 * 
 * Features:
 * - Real-time file and directory monitoring using inotify
 * - Multiple watch descriptors per socket
 * - Event filtering and delivery through socket callbacks
 * - Automatic cleanup on socket closure
 * - Thread-safe event processing
 * - Resource management and limits
 */

// File event types matching inotify events
enum class FileEventType {
    CREATED = 0x01,      // IN_CREATE
    MODIFIED = 0x02,     // IN_MODIFY
    DELETED = 0x04,      // IN_DELETE
    MOVED_FROM = 0x08,   // IN_MOVED_FROM
    MOVED_TO = 0x10,     // IN_MOVED_TO
    OPENED = 0x20,       // IN_OPEN
    CLOSED = 0x40,       // IN_CLOSE
    ATTRIB = 0x80,       // IN_ATTRIB
    ALL_EVENTS = 0xFF    // Combination of all events
};

// File event structure for delivery to LPC callbacks
struct FileEvent {
    std::string path;           // Full path to the file/directory
    std::string name;           // Filename (for directory events)
    FileEventType event_type;   // Type of event that occurred
    uint32_t cookie;            // Cookie for MOVE events
    bool is_directory;          // Whether the event target is a directory
    time_t timestamp;           // When the event occurred
    
    FileEvent() : event_type(FileEventType::MODIFIED), cookie(0), 
                  is_directory(false), timestamp(0) {}
};

// Watch descriptor information
struct WatchInfo {
    int watch_fd;               // inotify watch descriptor
    std::string path;           // Path being watched
    uint32_t mask;              // Event mask
    int socket_fd;              // Associated socket
    bool recursive;             // Whether to watch subdirectories
    
    WatchInfo() : watch_fd(-1), mask(0), socket_fd(-1), recursive(false) {}
};

/*
 * FileMonitor Class - Main inotify integration
 * 
 * Provides inotify-based file system monitoring with integration
 * into the FluffOS socket system for event delivery.
 */
class FileMonitor {
public:
    FileMonitor();
    ~FileMonitor();
    
    // Initialization and cleanup
    bool initialize();
    void shutdown();
    bool is_initialized() const { return inotify_fd_ != -1; }
    
    // Watch management
    bool add_watch(int socket_fd, const std::string& path, uint32_t mask = IN_ALL_EVENTS);
    bool remove_watch(int socket_fd, const std::string& path);
    void remove_all_watches(int socket_fd);
    
    // Event processing
    std::vector<FileEvent> process_events();
    bool has_pending_events() const;
    
    // Configuration
    void set_max_watches(size_t max_watches) { max_watches_ = max_watches; }
    void set_recursive_watch(bool recursive) { default_recursive_ = recursive; }
    
    // Statistics and debugging
    size_t get_watch_count() const { return watches_.size(); }
    size_t get_socket_watch_count(int socket_fd) const;
    std::vector<std::string> get_watched_paths(int socket_fd) const;
    
    // Static instance management
    static FileMonitor& instance();
    static bool initialize_global_monitor();
    static void shutdown_global_monitor();

private:
    int inotify_fd_;                                    // inotify file descriptor
    std::map<int, WatchInfo> watches_;                  // watch_fd -> WatchInfo
    std::unordered_map<std::string, int> path_to_wd_;   // path -> watch_fd mapping
    std::map<int, std::vector<int>> socket_watches_;    // socket_fd -> watch_fds
    
    // Configuration
    size_t max_watches_;
    bool default_recursive_;
    static std::unique_ptr<FileMonitor> global_instance_;
    
    // Helper methods
    bool create_inotify_instance();
    void cleanup_inotify_instance();
    uint32_t convert_event_mask(uint32_t lpc_mask);
    FileEventType convert_inotify_event(uint32_t inotify_mask);
    bool is_valid_path(const std::string& path);
    std::string resolve_full_path(const std::string& path);
    
    // Event processing helpers
    FileEvent parse_inotify_event(const struct inotify_event* event, const std::string& base_path);
    void cleanup_watch_descriptor(int watch_fd);
    void cleanup_socket_watches(int socket_fd);
};

/*
 * FileMonitorManager - Integration with External Package
 * 
 * Manages file monitoring for external process sockets and handles
 * the EXTERNAL_WATCH_PATH socket option integration.
 */
class FileMonitorManager {
public:
    // Socket option handling
    static bool handle_watch_path_option(int socket_fd, const std::string& path);
    static bool remove_watch_path_option(int socket_fd, const std::string& path);
    static void cleanup_socket_monitors(int socket_fd);
    
    // Event delivery to socket callbacks
    static void deliver_file_events(int socket_fd, const std::vector<FileEvent>& events);
    
    // LPC interface functions
    static int external_monitor_path(int socket_fd, const std::string& path, uint32_t events);
    static int external_stop_monitoring(int socket_fd, const std::string& path);
    static std::vector<FileEvent> external_get_file_events(int socket_fd);
    
    // Validation and security
    static bool validate_monitor_path(const std::string& path);
    static bool is_path_allowed(const std::string& path);
    
private:
    static std::map<int, std::vector<FileEvent>> pending_events_;
    static void queue_events_for_socket(int socket_fd, const std::vector<FileEvent>& events);
};

/*
 * Utility functions for file monitoring integration
 */
namespace FileMonitorUtils {
    // Event mask conversions
    uint32_t lpc_events_to_inotify_mask(uint32_t lpc_events);
    uint32_t inotify_mask_to_lpc_events(uint32_t inotify_mask);
    
    // Path utilities
    bool is_safe_path(const std::string& path);
    bool path_within_limits(const std::string& path);
    std::string normalize_path(const std::string& path);
    
    // Event formatting for LPC
    std::string format_event_for_lpc(const FileEvent& event);
    std::vector<std::string> events_to_lpc_array(const std::vector<FileEvent>& events);
}

// Global initialization functions
bool init_file_monitor_system();
void cleanup_file_monitor_system();

// Socket option registration functions
void register_external_watch_path_handler();
bool validate_external_watch_path(const svalue_t* value);

#endif  // PACKAGES_EXTERNAL_FILE_MONITOR_H_