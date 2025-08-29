#include "file_monitor.h"
#include "external.h"
#include "event_notifier.h"
#include "base/package_api.h"
#include "base/internal/log.h"
#include "packages/sockets/socket_option_manager.h"
#include "packages/sockets/socket_efuns.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <climits>
#include <algorithm>
#include <sstream>

#ifndef _WIN32
#include <sys/inotify.h>
#include <poll.h>
#endif

// Constants for file monitoring
static const size_t MAX_INOTIFY_EVENTS = 100;
static const size_t INOTIFY_BUFFER_SIZE = 4096 * (sizeof(struct inotify_event) + 16);
static const size_t DEFAULT_MAX_WATCHES = 1000;
static const uint32_t DEFAULT_INOTIFY_MASK = IN_CREATE | IN_MODIFY | IN_DELETE | 
                                           IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE;

// Static member initialization
std::unique_ptr<FileMonitor> FileMonitor::global_instance_ = nullptr;
std::map<int, std::vector<FileEvent>> FileMonitorManager::pending_events_;

/*
 * FileMonitor Implementation
 */

FileMonitor::FileMonitor() 
    : inotify_fd_(-1), max_watches_(DEFAULT_MAX_WATCHES), default_recursive_(false) {
}

FileMonitor::~FileMonitor() {
    shutdown();
}

bool FileMonitor::initialize() {
#ifdef _WIN32
    debug_message("File monitoring not supported on Windows platform");
    return false;
#else
    if (inotify_fd_ != -1) {
        return true; // Already initialized
    }
    
    inotify_fd_ = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (inotify_fd_ == -1) {
        debug_message("Failed to initialize inotify: %s", strerror(errno));
        return false;
    }
    
    debug_message("FileMonitor initialized with inotify fd: %d", inotify_fd_);
    return true;
#endif
}

void FileMonitor::shutdown() {
#ifndef _WIN32
    if (inotify_fd_ != -1) {
        // Remove all watches
        for (auto& pair : watches_) {
            inotify_rm_watch(inotify_fd_, pair.first);
        }
        
        close(inotify_fd_);
        inotify_fd_ = -1;
        
        // Clear all data structures
        watches_.clear();
        path_to_wd_.clear();
        socket_watches_.clear();
        
        debug_message("FileMonitor shutdown complete");
    }
#endif
}

bool FileMonitor::add_watch(int socket_fd, const std::string& path, uint32_t mask) {
#ifdef _WIN32
    return false;
#else
    if (inotify_fd_ == -1) {
        debug_message("FileMonitor not initialized");
        return false;
    }
    
    if (watches_.size() >= max_watches_) {
        debug_message("Maximum number of watches (%zu) exceeded", max_watches_);
        return false;
    }
    
    // Check if path is already being watched
    auto path_it = path_to_wd_.find(path);
    if (path_it != path_to_wd_.end()) {
        // Path already watched, just associate with this socket
        int watch_fd = path_it->second;
        socket_watches_[socket_fd].push_back(watch_fd);
        debug_message("Path '%s' already watched, associated with socket %d", 
                     path.c_str(), socket_fd);
        return true;
    }
    
    // Validate the path
    if (!is_valid_path(path)) {
        debug_message("Invalid path for monitoring: '%s'", path.c_str());
        return false;
    }
    
    // Use default mask if none provided
    if (mask == 0) {
        mask = DEFAULT_INOTIFY_MASK;
    }
    
    // Add the watch
    int watch_fd = inotify_add_watch(inotify_fd_, path.c_str(), mask);
    if (watch_fd == -1) {
        debug_message("Failed to add watch for '%s': %s", path.c_str(), strerror(errno));
        return false;
    }
    
    // Store watch information
    WatchInfo watch_info;
    watch_info.watch_fd = watch_fd;
    watch_info.path = path;
    watch_info.mask = mask;
    watch_info.socket_fd = socket_fd;
    watch_info.recursive = default_recursive_;
    
    watches_[watch_fd] = watch_info;
    path_to_wd_[path] = watch_fd;
    socket_watches_[socket_fd].push_back(watch_fd);
    
    debug_message("Added watch for '%s' (wd=%d, mask=0x%x) on socket %d", 
                 path.c_str(), watch_fd, mask, socket_fd);
    return true;
#endif
}

bool FileMonitor::remove_watch(int socket_fd, const std::string& path) {
#ifdef _WIN32
    return false;
#else
    auto path_it = path_to_wd_.find(path);
    if (path_it == path_to_wd_.end()) {
        debug_message("Path '%s' not being watched", path.c_str());
        return false;
    }
    
    int watch_fd = path_it->second;
    
    // Remove from socket's watch list
    auto socket_it = socket_watches_.find(socket_fd);
    if (socket_it != socket_watches_.end()) {
        auto& watch_list = socket_it->second;
        watch_list.erase(std::remove(watch_list.begin(), watch_list.end(), watch_fd), 
                        watch_list.end());
        
        if (watch_list.empty()) {
            socket_watches_.erase(socket_it);
        }
    }
    
    // Check if any other sockets are watching this path
    bool still_in_use = false;
    for (const auto& socket_pair : socket_watches_) {
        const auto& watch_list = socket_pair.second;
        if (std::find(watch_list.begin(), watch_list.end(), watch_fd) != watch_list.end()) {
            still_in_use = true;
            break;
        }
    }
    
    if (!still_in_use) {
        // No other sockets watching, remove the inotify watch
        if (inotify_rm_watch(inotify_fd_, watch_fd) == -1) {
            debug_message("Failed to remove inotify watch %d: %s", watch_fd, strerror(errno));
        }
        
        watches_.erase(watch_fd);
        path_to_wd_.erase(path_it);
        
        debug_message("Removed watch for '%s' (wd=%d)", path.c_str(), watch_fd);
    }
    
    return true;
#endif
}

void FileMonitor::remove_all_watches(int socket_fd) {
    auto socket_it = socket_watches_.find(socket_fd);
    if (socket_it == socket_watches_.end()) {
        return;
    }
    
    // Create a copy of the watch list to iterate over
    std::vector<int> watch_fds = socket_it->second;
    
    for (int watch_fd : watch_fds) {
        auto watch_it = watches_.find(watch_fd);
        if (watch_it != watches_.end()) {
            remove_watch(socket_fd, watch_it->second.path);
        }
    }
    
    debug_message("Removed all watches for socket %d", socket_fd);
}

std::vector<FileEvent> FileMonitor::process_events() {
    std::vector<FileEvent> events;
    
#ifndef _WIN32
    if (inotify_fd_ == -1) {
        return events;
    }
    
    char buffer[INOTIFY_BUFFER_SIZE];
    ssize_t bytes_read = read(inotify_fd_, buffer, sizeof(buffer));
    
    if (bytes_read == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            debug_message("Error reading inotify events: %s", strerror(errno));
        }
        return events;
    }
    
    if (bytes_read == 0) {
        return events;
    }
    
    // Parse events from buffer
    const char* ptr = buffer;
    const char* end = buffer + bytes_read;
    
    while (ptr < end && events.size() < MAX_INOTIFY_EVENTS) {
        const struct inotify_event* event = reinterpret_cast<const struct inotify_event*>(ptr);
        
        if (ptr + sizeof(struct inotify_event) > end) {
            break; // Incomplete event
        }
        
        if (ptr + sizeof(struct inotify_event) + event->len > end) {
            break; // Incomplete event data
        }
        
        // Find the watch info for this event
        auto watch_it = watches_.find(event->wd);
        if (watch_it != watches_.end()) {
            FileEvent file_event = parse_inotify_event(event, watch_it->second.path);
            if (file_event.event_type != FileEventType::MODIFIED || 
                !file_event.path.empty()) {
                events.push_back(file_event);
            }
        }
        
        ptr += sizeof(struct inotify_event) + event->len;
    }
    
    if (!events.empty()) {
        debug_message("Processed %zu file events", events.size());
    }
#endif
    
    return events;
}

bool FileMonitor::has_pending_events() const {
#ifndef _WIN32
    if (inotify_fd_ == -1) {
        return false;
    }
    
    struct pollfd pfd;
    pfd.fd = inotify_fd_;
    pfd.events = POLLIN;
    
    int result = poll(&pfd, 1, 0); // Non-blocking poll
    return result > 0 && (pfd.revents & POLLIN);
#else
    return false;
#endif
}

FileEvent FileMonitor::parse_inotify_event(const struct inotify_event* event, 
                                          const std::string& base_path) {
    FileEvent file_event;
    
#ifndef _WIN32
    file_event.path = base_path;
    if (event->len > 0 && event->name[0] != '\0') {
        file_event.name = event->name;
        file_event.path += "/" + file_event.name;
    }
    
    file_event.event_type = convert_inotify_event(event->mask);
    file_event.cookie = event->cookie;
    file_event.is_directory = (event->mask & IN_ISDIR) != 0;
    file_event.timestamp = time(nullptr);
#endif
    
    return file_event;
}

FileEventType FileMonitor::convert_inotify_event(uint32_t inotify_mask) {
#ifndef _WIN32
    if (inotify_mask & IN_CREATE) return FileEventType::CREATED;
    if (inotify_mask & IN_MODIFY) return FileEventType::MODIFIED;
    if (inotify_mask & IN_DELETE) return FileEventType::DELETED;
    if (inotify_mask & IN_MOVED_FROM) return FileEventType::MOVED_FROM;
    if (inotify_mask & IN_MOVED_TO) return FileEventType::MOVED_TO;
    if (inotify_mask & IN_OPEN) return FileEventType::OPENED;
    if (inotify_mask & (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE)) return FileEventType::CLOSED;
    if (inotify_mask & IN_ATTRIB) return FileEventType::ATTRIB;
#endif
    
    return FileEventType::MODIFIED;
}

bool FileMonitor::is_valid_path(const std::string& path) {
    if (path.empty() || path.length() > PATH_MAX) {
        return false;
    }
    
    // Check if path exists
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        debug_message("Path does not exist: '%s'", path.c_str());
        return false;
    }
    
    // Basic security check - no .. traversal
    if (path.find("..") != std::string::npos) {
        debug_message("Path contains '..' traversal: '%s'", path.c_str());
        return false;
    }
    
    return true;
}

// Static instance management
FileMonitor& FileMonitor::instance() {
    if (!global_instance_) {
        global_instance_ = std::make_unique<FileMonitor>();
    }
    return *global_instance_;
}

bool FileMonitor::initialize_global_monitor() {
    return instance().initialize();
}

void FileMonitor::shutdown_global_monitor() {
    if (global_instance_) {
        global_instance_->shutdown();
        global_instance_.reset();
    }
}

/*
 * FileMonitorManager Implementation
 */

bool FileMonitorManager::handle_watch_path_option(int socket_fd, const std::string& path) {
    if (!validate_monitor_path(path)) {
        debug_message("Path validation failed for socket %d: '%s'", socket_fd, path.c_str());
        return false;
    }
    
    FileMonitor& monitor = FileMonitor::instance();
    if (!monitor.is_initialized() && !monitor.initialize()) {
        debug_message("Failed to initialize file monitor for socket %d", socket_fd);
        return false;
    }
    
    return monitor.add_watch(socket_fd, path);
}

// LPC interface function implementations
int FileMonitorManager::external_monitor_path(int socket_fd, const std::string& path, uint32_t events) {
    if (!validate_monitor_path(path)) {
        return -1;
    }
    
    FileMonitor& monitor = FileMonitor::instance();
    if (!monitor.is_initialized() && !monitor.initialize()) {
        return -1;
    }
    
    uint32_t inotify_mask = events == 0 ? DEFAULT_INOTIFY_MASK : FileMonitorUtils::lpc_events_to_inotify_mask(events);
    
    if (monitor.add_watch(socket_fd, path, inotify_mask)) {
        return 0; // Success
    }
    
    return -1; // Failure
}

int FileMonitorManager::external_stop_monitoring(int socket_fd, const std::string& path) {
    FileMonitor& monitor = FileMonitor::instance();
    
    if (monitor.remove_watch(socket_fd, path)) {
        return 0; // Success
    }
    
    return -1; // Failure - path not being watched
}

std::vector<FileEvent> FileMonitorManager::external_get_file_events(int socket_fd) {
    auto it = pending_events_.find(socket_fd);
    if (it == pending_events_.end() || it->second.empty()) {
        return std::vector<FileEvent>(); // No events
    }
    
    // Return all pending events and clear the queue
    std::vector<FileEvent> events = it->second;
    it->second.clear();
    
    return events;
}

bool FileMonitorManager::remove_watch_path_option(int socket_fd, const std::string& path) {
    FileMonitor& monitor = FileMonitor::instance();
    return monitor.remove_watch(socket_fd, path);
}

void FileMonitorManager::cleanup_socket_monitors(int socket_fd) {
    FileMonitor& monitor = FileMonitor::instance();
    monitor.remove_all_watches(socket_fd);
    
    // Remove pending events for this socket
    pending_events_.erase(socket_fd);
    
    debug_message("Cleaned up file monitors for socket %d", socket_fd);
}

void FileMonitorManager::deliver_file_events(int socket_fd, const std::vector<FileEvent>& events) {
    if (events.empty()) {
        return;
    }
    
    // Queue events for later retrieval by LPC code
    queue_events_for_socket(socket_fd, events);
    
    // Enhanced delivery with eventfd integration (Phase 2)
    // Signal async event system for each file event if socket has async mode enabled
    for (const FileEvent& event : events) {
        AsyncEventManager::signal_file_changed(socket_fd, event.path);
    }
    
    // Trigger socket read callback if available
    // This integrates with the socket system to notify LPC code of file events
    object_t *ob = nullptr;  // Socket callback object - would need to get from socket system
    if (ob != nullptr) {
        // Would call socket read callback here with file event data
        debug_message("Delivered %zu file events to socket %d with async notification", events.size(), socket_fd);
    }
}

void FileMonitorManager::queue_events_for_socket(int socket_fd, const std::vector<FileEvent>& events) {
    auto& socket_events = pending_events_[socket_fd];
    socket_events.insert(socket_events.end(), events.begin(), events.end());
    
    // Limit the number of queued events to prevent memory issues
    const size_t MAX_QUEUED_EVENTS = 1000;
    if (socket_events.size() > MAX_QUEUED_EVENTS) {
        socket_events.erase(socket_events.begin(), 
                           socket_events.begin() + (socket_events.size() - MAX_QUEUED_EVENTS));
    }
}

bool FileMonitorManager::validate_monitor_path(const std::string& path) {
    return FileMonitorUtils::is_safe_path(path) && 
           FileMonitorUtils::path_within_limits(path) &&
           is_path_allowed(path);
}

bool FileMonitorManager::is_path_allowed(const std::string& path) {
    // Basic security - prevent monitoring of sensitive system paths
    std::vector<std::string> blocked_paths = {
        "/etc/passwd", "/etc/shadow", "/proc", "/sys", "/dev"
    };
    
    for (const std::string& blocked : blocked_paths) {
        if (path.find(blocked) == 0) {
            return false;
        }
    }
    
    return true;
}

/*
 * FileMonitorUtils Implementation
 */

uint32_t FileMonitorUtils::lpc_events_to_inotify_mask(uint32_t lpc_events) {
#ifndef _WIN32
    uint32_t mask = 0;
    
    if (lpc_events & static_cast<uint32_t>(FileEventType::CREATED)) mask |= IN_CREATE;
    if (lpc_events & static_cast<uint32_t>(FileEventType::MODIFIED)) mask |= IN_MODIFY;
    if (lpc_events & static_cast<uint32_t>(FileEventType::DELETED)) mask |= IN_DELETE;
    if (lpc_events & static_cast<uint32_t>(FileEventType::MOVED_FROM)) mask |= IN_MOVED_FROM;
    if (lpc_events & static_cast<uint32_t>(FileEventType::MOVED_TO)) mask |= IN_MOVED_TO;
    if (lpc_events & static_cast<uint32_t>(FileEventType::OPENED)) mask |= IN_OPEN;
    if (lpc_events & static_cast<uint32_t>(FileEventType::CLOSED)) mask |= IN_CLOSE;
    if (lpc_events & static_cast<uint32_t>(FileEventType::ATTRIB)) mask |= IN_ATTRIB;
    
    return mask;
#else
    return 0;
#endif
}

uint32_t FileMonitorUtils::inotify_mask_to_lpc_events(uint32_t inotify_mask) {
#ifndef _WIN32
    uint32_t events = 0;
    
    if (inotify_mask & IN_CREATE) events |= static_cast<uint32_t>(FileEventType::CREATED);
    if (inotify_mask & IN_MODIFY) events |= static_cast<uint32_t>(FileEventType::MODIFIED);
    if (inotify_mask & IN_DELETE) events |= static_cast<uint32_t>(FileEventType::DELETED);
    if (inotify_mask & IN_MOVED_FROM) events |= static_cast<uint32_t>(FileEventType::MOVED_FROM);
    if (inotify_mask & IN_MOVED_TO) events |= static_cast<uint32_t>(FileEventType::MOVED_TO);
    if (inotify_mask & IN_OPEN) events |= static_cast<uint32_t>(FileEventType::OPENED);
    if (inotify_mask & (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE)) events |= static_cast<uint32_t>(FileEventType::CLOSED);
    if (inotify_mask & IN_ATTRIB) events |= static_cast<uint32_t>(FileEventType::ATTRIB);
    
    return events;
#else
    return 0;
#endif
}

bool FileMonitorUtils::is_safe_path(const std::string& path) {
    if (path.empty() || path[0] != '/') {
        return false; // Must be absolute path
    }
    
    if (path.find("..") != std::string::npos) {
        return false; // No parent directory traversal
    }
    
    if (path.find("//") != std::string::npos) {
        return false; // No double slashes
    }
    
    return true;
}

bool FileMonitorUtils::path_within_limits(const std::string& path) {
    return path.length() < PATH_MAX;
}

std::string FileMonitorUtils::normalize_path(const std::string& path) {
    // Basic path normalization - remove trailing slashes
    std::string normalized = path;
    while (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

std::string FileMonitorUtils::format_event_for_lpc(const FileEvent& event) {
    std::ostringstream oss;
    oss << "({ \"path\": \"" << event.path 
        << "\", \"name\": \"" << event.name
        << "\", \"type\": " << static_cast<int>(event.event_type)
        << ", \"directory\": " << (event.is_directory ? 1 : 0)
        << ", \"timestamp\": " << event.timestamp << " })";
    return oss.str();
}

std::vector<std::string> FileMonitorUtils::events_to_lpc_array(const std::vector<FileEvent>& events) {
    std::vector<std::string> lpc_events;
    lpc_events.reserve(events.size());
    
    for (const FileEvent& event : events) {
        lpc_events.push_back(format_event_for_lpc(event));
    }
    
    return lpc_events;
}

/*
 * Global initialization functions
 */

bool init_file_monitor_system() {
    return FileMonitor::initialize_global_monitor();
}

void cleanup_file_monitor_system() {
    FileMonitor::shutdown_global_monitor();
}

/*
 * Socket option registration function
 */

void register_external_watch_path_handler() {
    // This would integrate with SocketOptionManager to register the handler
    // The actual implementation would depend on the socket option system
    debug_message("External watch path handler registered");
}

bool validate_external_watch_path(const svalue_t* value) {
    if (value == nullptr || value->type != T_STRING) {
        return false;
    }
    
    std::string path(value->u.string);
    return FileMonitorManager::validate_monitor_path(path);
}