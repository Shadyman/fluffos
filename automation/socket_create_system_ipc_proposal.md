# Socket Mode Extension: System IPC (High-Risk)

## Overview

This proposal covers **system-wide IPC mechanisms** that require **elevated security considerations** and should only be enabled in **controlled environments** with **strict master daemon validation**.

## High-Risk Socket Modes (Reserved Range: 60-69)

```c
// System-wide IPC modes (PACKAGE_EXTERNAL + enhanced security)
// WARNING: These modes require system-wide resources and careful security validation

EXTERNAL_MQUEUE = 60,        // POSIX message queues (/dev/mqueue)
EXTERNAL_SYSV_MSGQ = 61,     // System V message queues
EXTERNAL_SHMEM = 62,         // POSIX shared memory
EXTERNAL_SYSV_SHM = 63,      // System V shared memory
EXTERNAL_SIGNALFD = 64,      // Linux signalfd for signal handling
// Reserved for system IPC: 65-69
```

## Security Risks

### Message Queues (60-61)
- **System-wide namespace** - can conflict with existing queues
- **Persistent resources** - survive process death
- **Resource exhaustion** - limited system queue slots
- **Information disclosure** - potential access to existing queues

### Shared Memory (62-63)
- **Direct memory access** - memory corruption risks
- **System-wide segments** - namespace conflicts
- **Size limits** - difficult to enforce effectively
- **Information leakage** - potential access to existing segments

### Signal Handling (64)
- **Process signal interference** - can disrupt normal signal handling
- **Signal hijacking** - potential to intercept system signals
- **Process stability** - improper signal handling can crash processes

## Required Security Measures

### Master Daemon Validation
```lpc
// Enhanced validation using config file limits in /lib/secure/daemon/master.c
int valid_socket(object caller, int mode, string address, mixed extra) {
    string caller_uid = getuid(caller);
    
    // Check if system IPC is enabled at all
    if(!query_config("enable_system_ipc")) {
        return 0;
    }
    
    switch(mode) {
        case EXTERNAL_MQUEUE:
            return validate_mqueue_request(caller, caller_uid, address, extra);
            
        case EXTERNAL_SHMEM:
            return validate_shm_request(caller, caller_uid, address, extra);
            
        case EXTERNAL_SIGNALFD:
            return validate_signalfd_request(caller, caller_uid, address, extra);
    }
    return 0; // Deny by default
}

// Message Queue Validation
int validate_mqueue_request(object caller, string uid, string address, mixed extra) {
    // Only admins can create message queues
    if(!adminp(caller)) return 0;
    
    // Check user quota
    if(count_user_mqueues(uid) >= query_config("mqueue_max_per_user")) {
        log_security("MQUEUE quota exceeded for user: " + uid);
        return 0;
    }
    
    // Validate queue name against whitelist
    string *allowed_names = explode(query_config("mqueue_allowed_names"), ",");
    if(member(allowed_names, address) == -1) {
        log_security("MQUEUE invalid queue name: " + address + " by " + uid);
        return 0;
    }
    
    // Check message limits
    int max_messages = query_config("mqueue_max_messages");
    int max_msg_size = query_config("mqueue_max_message_size");
    
    if(extra["max_messages"] > max_messages || extra["max_message_size"] > max_msg_size) {
        log_security("MQUEUE limits exceeded by " + uid);
        return 0;
    }
    
    return 1;
}

// Shared Memory Validation  
int validate_shm_request(object caller, string uid, string address, mixed extra) {
    // Only admins can create shared memory
    if(!adminp(caller)) return 0;
    
    // Check user quota
    if(count_user_shm_segments(uid) >= query_config("shm_max_per_user")) {
        log_security("SHM quota exceeded for user: " + uid);
        return 0;
    }
    
    // Validate path - must be under configured base path
    string base_path = query_config("shm_base_path");
    if(sscanf(address, base_path + "/%s", string name) != 1) {
        log_security("SHM invalid path: " + address + " (must be under " + base_path + ")");
        return 0;
    }
    
    // Check name prefix whitelist
    string *allowed_prefixes = explode(query_config("shm_allowed_prefixes"), ",");
    int valid_prefix = 0;
    foreach(string prefix in allowed_prefixes) {
        if(sscanf(name, prefix + "_%*s") == 1) {
            valid_prefix = 1;
            break;
        }
    }
    if(!valid_prefix) {
        log_security("SHM invalid name prefix: " + name + " by " + uid);
        return 0;
    }
    
    // Check size limits
    int requested_size = extra["size"];
    if(requested_size > query_config("shm_max_size")) {
        log_security("SHM size too large: " + requested_size + " by " + uid);
        return 0;
    }
    
    // Check total memory quota for user
    if(get_user_shm_total(uid) + requested_size > query_config("ipc_user_quota")) {
        log_security("SHM total quota exceeded by " + uid);
        return 0;
    }
    
    return 1;
}

// Signal FD Validation
int validate_signalfd_request(object caller, string uid, string address, mixed extra) {
    // Only specific system daemons can use signal handling
    string *allowed_daemons = ({
        "/lib/secure/daemon/signal_handler",
        "/lib/secure/daemon/shutdown",
        "/lib/secure/daemon/master"
    });
    
    if(member(allowed_daemons, base_name(caller)) == -1) {
        log_security("SIGNALFD unauthorized daemon: " + base_name(caller));
        return 0;
    }
    
    // Check handler quota
    if(count_user_signal_handlers(uid) >= query_config("signalfd_max_handlers")) {
        log_security("SIGNALFD quota exceeded for user: " + uid);
        return 0;
    }
    
    // Validate requested signals against whitelist
    string *requested_signals = explode(address, ",");
    string *allowed_signals = explode(query_config("signalfd_allowed_signals"), ",");
    
    foreach(string signal in requested_signals) {
        if(member(allowed_signals, signal) == -1) {
            log_security("SIGNALFD invalid signal: " + signal + " by " + uid);
            return 0;
        }
    }
    
    return 1;
}
```

### Resource Tracking
```lpc
// Track system resource usage
mapping system_ipc_usage = ([]);

void track_ipc_resource(object caller, int mode, string address) {
    string key = sprintf("%s:%d:%s", getuid(caller), mode, address);
    system_ipc_usage[key] = time();
    
    // Cleanup old resources periodically
    call_out("cleanup_abandoned_ipc", 3600);
}
```

### Automatic Cleanup
```lpc
// Mandatory cleanup when sockets close
void external_socket_closed(int socket, int mode, string address) {
    switch(mode) {
        case EXTERNAL_MQUEUE:
            system("/bin/rm -f /dev/mqueue/" + address);
            break;
        case EXTERNAL_SHMEM:
            external_cleanup_shm(address);
            break;
    }
}
```

## Use Cases (Admin Only)

### Inter-MUD Communication
```lpc
// MUD cluster coordination (admin-only, uses config whitelist)
int cluster_mq = socket_create(EXTERNAL_MQUEUE, "handle_cluster_msg", "mq_error");
// Queue name "cluster" must be in mqueue_allowed_names config
// Path becomes: /dev/mqueue/mud/cluster (using mqueue_base_path)
socket_bind(cluster_mq, "cluster", ([
    "max_messages": 50,        // Must be <= mqueue_max_messages (100)
    "max_message_size": 4096   // Must be <= mqueue_max_message_size (8192)
]));

void handle_cluster_msg(int socket, mixed data) {
    mapping msg = json_decode(data);
    if(msg["type"] == "player_transfer") {
        coordinate_player_transfer(msg);
    }
}
```

### High-Performance Statistics
```lpc
// Shared statistics between processes (admin-only, config-restricted)
int stats_shm = socket_create(EXTERNAL_SHMEM, "stats_updated", "shm_error");
// Name "stats_live" uses allowed prefix "stats" from shm_allowed_prefixes
// Full path becomes: /tmp/mud/shm/stats_live (using shm_base_path)
socket_bind(stats_shm, "stats_live", ([
    "size": 65536  // 64KB, must be <= shm_max_size (1MB) and within user quota
]));

void update_live_stats() {
    mapping stats = ([
        "players": sizeof(users()),
        "uptime": uptime(),
        "memory": query_memory_usage(),
        "timestamp": time()
    ]);
    socket_write(stats_shm, json_encode(stats));
}
```

### Signal Coordination
```lpc
// System signal handling (daemon-only, restricted to whitelisted signals)
// Only callable from approved daemons like /lib/secure/daemon/signal_handler
int signal_fd = socket_create(EXTERNAL_SIGNALFD, "handle_signal", "signal_error");
// Signals must be in signalfd_allowed_signals: "SIGTERM,SIGUSR1,SIGUSR2"
socket_bind(signal_fd, "SIGTERM,SIGUSR1");  // Subset of allowed signals

void handle_signal(int socket, mixed signal_info) {
    switch(signal_info["signal"]) {
        case SIGTERM:
            log_system("Received SIGTERM, initiating graceful shutdown");
            initiate_graceful_shutdown();
            break;
        case SIGUSR1:
            log_system("Received SIGUSR1, reloading configuration");
            reload_configuration();
            break;
    }
}
```

### Configuration-Based Safety Examples
```lpc
// This would FAIL - queue name not in whitelist
socket_create(EXTERNAL_MQUEUE, "evil_callback", "error");
socket_bind(socket, "unauthorized_queue");  // DENIED: not in mqueue_allowed_names

// This would FAIL - path outside base directory  
socket_create(EXTERNAL_SHMEM, "hack_attempt", "error");
socket_bind(socket, "/etc/passwd");  // DENIED: not under shm_base_path

// This would FAIL - signal not whitelisted
socket_create(EXTERNAL_SIGNALFD, "signal_hack", "error");  
socket_bind(socket, "SIGKILL");  // DENIED: not in signalfd_allowed_signals

// This would FAIL - size exceeds limit
socket_create(EXTERNAL_SHMEM, "big_memory", "error");
socket_bind(socket, "temp_huge", (["size": 2097152]));  // DENIED: > shm_max_size (1MB)
```

## Implementation Requirements

### Compile-Time Flags
```cmake
# Enable only with explicit flag
option(ENABLE_SYSTEM_IPC "Enable high-risk system IPC socket modes" OFF)

if(ENABLE_SYSTEM_IPC)
    add_definitions(-DENABLE_EXTERNAL_MQUEUE)
    add_definitions(-DENABLE_EXTERNAL_SHMEM)
    add_definitions(-DENABLE_EXTERNAL_SIGNALFD)
endif()
```

### Runtime Configuration
```lpc
// In quantumscape.cfg - Safety limits and default paths
enable_system_ipc : 0                    // Disabled by default

// Message Queue Configuration
mqueue_base_path : /dev/mqueue/mud       // Default base path for queues
mqueue_max_per_user : 2                  // Maximum queues per user
mqueue_max_messages : 100                // Max messages per queue
mqueue_max_message_size : 8192           // Max message size (8KB)
mqueue_allowed_names : cluster,admin,backup,stats  // Whitelist of queue names

// Shared Memory Configuration  
shm_base_path : /tmp/mud/shm             // Default base path for shared memory
shm_max_per_user : 1                     // Maximum segments per user
shm_max_size : 1048576                   // Max segment size (1MB)
shm_max_total : 16777216                 // Max total shared memory (16MB)
shm_allowed_prefixes : stats,cache,temp  // Allowed name prefixes

// Signal Configuration
signalfd_allowed_signals : SIGTERM,SIGUSR1,SIGUSR2  // Whitelisted signals
signalfd_max_handlers : 1                // Max signal handlers per user

// General IPC Security
ipc_chroot_base : /mud/ipc              // Chroot base for all IPC operations
ipc_user_quota : 4194304                // 4MB total IPC quota per user
ipc_cleanup_interval : 3600             // Cleanup abandoned resources every hour
ipc_max_age : 86400                     // Max age for IPC resources (24 hours)
```

## Deployment Recommendations

### Production Environments
- **Disable by default** - only enable when specifically needed
- **Admin-only access** - never allow regular users
- **Resource monitoring** - track all system IPC usage
- **Regular cleanup** - automated cleanup of abandoned resources

### Development Environments
- **Sandboxed testing** - isolated from production systems
- **Resource limits** - strict quotas on all resources
- **Monitoring** - comprehensive logging of all IPC operations

## Conclusion

These system IPC socket modes provide powerful capabilities for **advanced MUD deployments** but require **extreme caution** and **comprehensive security measures**. They should only be considered for:

- **MUD clusters** requiring high-performance inter-process communication
- **Production environments** with dedicated system administrators
- **Specialized deployments** where the benefits outweigh the security risks

**Recommendation**: Implement these modes as a **separate optional package** that requires explicit compilation flags and runtime configuration to enable.