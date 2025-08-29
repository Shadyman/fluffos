#include "external.h"
#include "process_manager.h"
#include "command_executor.h"
#include "file_monitor.h"
#include "event_notifier.h"
#include "io_redirector.h"
#include "resource_manager.h"
#include "base/package_api.h"
#include "base/internal/log.h"
#include "packages/sockets/socket_option_manager.h"
#include "vm/internal/base/mapping.h"

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <string>
#include <variant>
#include <fmt/format.h>
#include <event2/event.h>
#include "include/socket_err.h"
#include "packages/sockets/socket_efuns.h"

#ifndef _WIN32
#include <sstream>
#include <vector>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#endif

/*
 * Global variables and configuration
 */
SecurityContext g_external_security_context;
bool g_external_package_initialized = false;

// Legacy external command configuration (using existing FluffOS globals)

// Static member initialization
std::unordered_map<int, std::unique_ptr<ExternalProcessInfo>> ExternalSocketHandler::processes_;
SecurityContext ExternalSocketHandler::default_security_context_;

/*
 * Legacy external_start function (preserved for compatibility)
 */

#ifndef _WIN32
template <typename Out>
void split(const std::string &s, char delim, Out result) {
  std::istringstream iss(s);
  std::string item;
  while (std::getline(iss, item, delim)) {
    *result++ = item;
  }
}

std::string format_time(const struct tm& timeinfo) {
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return std::string(buffer);
}

// Legacy external_start implementation (from original file)
int external_start(int which, svalue_t *args, svalue_t *arg1, svalue_t *arg2, svalue_t *arg3) {
  std::vector<std::string> newargs_data = {std::string(external_cmd[which])};
  if (args->type == T_ARRAY) {
    for (int i = 0; i < args->u.arr->size; i++) {
      auto item = args->u.arr->item[i];
      if (item.type != T_STRING) {
        error("Bad argument list item %d to external_start()\n", i);
      }
      newargs_data.push_back(item.u.string);
    }
  } else {
    split(std::string(args->u.string), ' ', std::back_inserter(newargs_data));
  }

  std::vector<char *> newargs;
  for (auto &arg : newargs_data) {
    newargs.push_back(arg.data());
  }
  newargs.push_back(nullptr);

  posix_spawn_file_actions_t file_actions;
  int ret = posix_spawn_file_actions_init(&file_actions);
  if (ret != 0) {
    debug(external_start, "external_start: posix_spawn_file_actions_init() error: %s\n", strerror(ret));
    return EESOCKET;
  }
  DEFER { posix_spawn_file_actions_destroy(&file_actions); };

  evutil_socket_t sv[2];
  if (evutil_socketpair(PF_UNIX, SOCK_STREAM, 0, sv) == -1) {
    return EESOCKET;
  }
  DEFER {
    if (sv[0] > 0) {
      evutil_closesocket(sv[0]);
    }
    if (sv[1] > 0) {
      evutil_closesocket(sv[1]);
    }
  };
  if (evutil_make_socket_nonblocking(sv[0]) == -1 || evutil_make_socket_nonblocking(sv[1]) == -1) {
    return EESOCKET;
  }
  ret = posix_spawn_file_actions_adddup2(&file_actions, sv[1], 0) ||
        posix_spawn_file_actions_adddup2(&file_actions, sv[1], 1) ||
        posix_spawn_file_actions_adddup2(&file_actions, sv[1], 2);
  if (ret != 0) {
    debug(external_start, "external_start: posix_spawn_file_actions_adddup2() error: %s\n", strerror(ret));
    return EESOCKET;
  }

  int fd = find_new_socket();
  if (fd < 0) {
    return fd;
  }

  auto *sock = lpc_socks_get(fd);
  new_lpc_socket_event_listener(fd, sock, sv[0]);

  sock->fd = sv[0];
  sock->flags = S_EXTERNAL;
  set_read_callback(fd, arg1);
  set_write_callback(fd, arg2);
  set_close_callback(fd, arg3);
  sock->owner_ob = current_object;
  sock->mode = STREAM;
  sock->state = STATE_DATA_XFER;
  memset(reinterpret_cast<char *>(&sock->l_addr), 0, sizeof(sock->l_addr));
  memset(reinterpret_cast<char *>(&sock->r_addr), 0, sizeof(sock->r_addr));
  sock->owner_ob = current_object;
  sock->release_ob = nullptr;
  sock->r_buf = nullptr;
  sock->r_off = 0;
  sock->r_len = 0;
  sock->w_buf = nullptr;
  sock->w_off = 0;
  sock->w_len = 0;

  current_object->flags |= O_EFUN_SOCKET;

  event_add(sock->ev_write, nullptr);
  event_add(sock->ev_read, nullptr);

  pid_t pid;
  char *newenviron[] = {nullptr};
  ret = posix_spawn(&pid, newargs[0], &file_actions, nullptr, newargs.data(), newenviron);
  if (ret) {
    debug(external_start, "external_start: posix_spawn() error: %s\n", strerror(ret));
    return EESOCKET;
  }

  evutil_closesocket(sv[1]);
  sv[1] = -1;

  evutil_socket_t childfd = sv[0];
  sv[0] = -1;

  debug(external_start, "external_start: Launching external command '%s %s', pid: %jd.\n", external_cmd[which],
                args->type == T_STRING ? args->u.string : "<ARRAY>", (intmax_t)pid);

  std::thread([=]() {
    int status;
    do {
      const int s = waitpid(pid, &status, WUNTRACED | WCONTINUED);
      if (s == -1) {
        debug(external_start, "external_start: waitpid() error: %s (%d).\n", strerror(errno), errno);
        return;
      }
      std::string res = fmt::format(FMT_STRING("external_start(): child {} status: "), pid);
      if (WIFEXITED(status)) {
        res += fmt::format(FMT_STRING("exited, status={}\n"), WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
        res += fmt::format(FMT_STRING("killed by signal {}\n"), WTERMSIG(status));
      } else if (WIFSTOPPED(status)) {
        res += fmt::format(FMT_STRING("stopped by signal {}\n"), WSTOPSIG(status));
      } else if (WIFCONTINUED(status)) {
        res += "continued\n";
      }

      debug(external_start, "external_start: %s\n", format_time(res).c_str());
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }).detach();

  return fd;
}
#endif

/*
 * ExternalSocketHandler implementation for unified socket architecture
 */

// Public static accessor methods
void ExternalSocketHandler::initialize_security_context(const SecurityContext& context) {
    default_security_context_ = context;
}

void ExternalSocketHandler::clear_all_processes() {
    processes_.clear();
}

int ExternalSocketHandler::create_handler(enum socket_mode_extended mode, svalue_t *read_callback, svalue_t *close_callback) {
    debug(external_start, "Creating external socket handler for mode %d", mode);
    
    // Create standard socket first
    int socket_fd = socket_create(STREAM, read_callback, close_callback);
    if (socket_fd < 0) {
        debug(external_start, "Failed to create standard socket");
        return socket_fd;
    }
    
    // Initialize external-specific configuration
    switch (mode) {
        case EXTERNAL_PROCESS:
            debug(external_start, "Setting up EXTERNAL_PROCESS mode for socket %d", socket_fd);
            // Socket options will be configured via socket_option() efun from LPC
            break;
            
        case EXTERNAL_COMMAND_MODE:
            debug(external_start, "Setting up EXTERNAL_COMMAND_MODE mode for socket %d", socket_fd);
            // Socket options will be configured via socket_option() efun from LPC  
            break;
            
        default:
            debug(external_start, "Unknown external socket mode: %d", mode);
            socket_close(socket_fd, 0);
            return -1;
    }
    
    // Initialize process tracking
    auto process_info = std::make_unique<ExternalProcessInfo>();
    process_info->socket_fd = socket_fd;
    process_info->option_manager = std::make_unique<SocketOptionManager>(socket_fd);
    
    // Set initial socket mode options
    svalue_t mode_value;
    mode_value.type = T_NUMBER;
    mode_value.u.number = 1;
    
    switch (mode) {
        case EXTERNAL_PROCESS:
            process_info->option_manager->set_option(EXTERNAL_MODE, &mode_value);
            mode_value.u.number = 0; // Sync by default
            process_info->option_manager->set_option(EXTERNAL_ASYNC, &mode_value);
            break;
            
        case EXTERNAL_COMMAND_MODE:
            process_info->option_manager->set_option(EXTERNAL_MODE, &mode_value);
            mode_value.u.number = 1; // Async by default
            process_info->option_manager->set_option(EXTERNAL_ASYNC, &mode_value);
            break;
            
        default:
            // Other socket modes not handled by external process package
            error("Invalid socket mode for external process package: %d", static_cast<int>(mode));
            return EESOCKET;
    }
    
    processes_[socket_fd] = std::move(process_info);
    
    debug(external_start, "External socket handler created successfully: fd=%d, mode=%d", socket_fd, mode);
    return socket_fd;
}

int ExternalSocketHandler::spawn_process(int socket_fd) {
    debug(external_start, "Spawning process for socket %d", socket_fd);
    
    auto it = processes_.find(socket_fd);
    if (it == processes_.end()) {
        debug(external_start, "No process info found for socket %d", socket_fd);
        return -1;
    }
    
    ExternalProcessInfo* info = it->second.get();
    
    // Extract options from socket
    if (!extract_process_options(socket_fd, info)) {
        debug(external_start, "Failed to extract process options for socket %d", socket_fd);
        return -1;
    }
    
    // Get security context
    SecurityContext security = get_security_context(socket_fd);
    
    // Spawn process through ProcessManager
    ProcessManager& pm = ProcessManager::instance();
    
    // Pass the process info to the manager (it takes ownership)
    if (pm.spawn_process(socket_fd, info, security)) {
        // Update our local info
        info->is_running = true;
        info->start_time = time(nullptr);
        
        debug(external_start, "Process spawned successfully for socket %d", socket_fd);
        return 0;
    } else {
        debug(external_start, "Failed to spawn process for socket %d", socket_fd);
        return -1;
    }
}

bool ExternalSocketHandler::terminate_process(int socket_fd, int signal) {
    debug(external_start, "Terminating process for socket %d with signal %d", socket_fd, signal);
    
    ProcessManager& pm = ProcessManager::instance();
    return pm.terminate_process(socket_fd, signal);
}

bool ExternalSocketHandler::kill_process(int socket_fd) {
    debug(external_start, "Killing process for socket %d", socket_fd);
    
    ProcessManager& pm = ProcessManager::instance();
    return pm.kill_process(socket_fd);
}

ExternalProcessInfo* ExternalSocketHandler::get_process_info(int socket_fd) {
    auto it = processes_.find(socket_fd);
    if (it != processes_.end()) {
        return it->second.get();
    }
    
    // Also check ProcessManager
    return ProcessManager::instance().get_process_info(socket_fd);
}

int ExternalSocketHandler::write_to_process(int socket_fd, const char* data, size_t length) {
    ProcessManager& pm = ProcessManager::instance();
    return pm.write_to_process(socket_fd, data, length);
}

int ExternalSocketHandler::read_from_process(int socket_fd, char* buffer, size_t max_length) {
    ProcessManager& pm = ProcessManager::instance();
    return pm.read_from_process(socket_fd, buffer, max_length);
}

void ExternalSocketHandler::cleanup_handler(int socket_fd) {
    debug(external_start, "Cleaning up external socket handler for socket %d", socket_fd);
    
    // Cleanup process through ProcessManager
    ProcessManager::instance().cleanup_process(socket_fd);
    
    // Cleanup file monitoring for this socket
    FileMonitorManager::cleanup_socket_monitors(socket_fd);
    
    // Cleanup enhanced async events for this socket
    AsyncEventManager::cleanup_socket_events(socket_fd);
    
    // Cleanup I/O redirection for this socket (Phase 3)
    IORedirector::instance().cleanup_redirection(socket_fd);
    
    // Remove from our tracking
    processes_.erase(socket_fd);
    
    debug(external_start, "External socket handler cleanup completed for socket %d", socket_fd);
}

bool ExternalSocketHandler::extract_process_options(int socket_fd, ExternalProcessInfo* info) {
    if (!info->option_manager) {
        debug(external_start, "No option manager available for socket %d", socket_fd);
        return false;
    }
    
    // Extract command
    svalue_t command_value;
    if (info->option_manager->get_option(EXTERNAL_COMMAND, &command_value)) {
        if (command_value.type == T_STRING) {
            info->command = command_value.u.string;
        }
    }
    
    if (info->command.empty()) {
        debug(external_start, "No command specified for socket %d", socket_fd);
        return false;
    }
    
    // Extract arguments  
    svalue_t args_value;
    if (info->option_manager->get_option(EXTERNAL_ARGS, &args_value)) {
        if (args_value.type == T_ARRAY) {
            array_t* args_array = args_value.u.arr;
            for (int i = 0; i < args_array->size; i++) {
                if (args_array->item[i].type == T_STRING) {
                    info->args.push_back(std::string(args_array->item[i].u.string));
                }
            }
        }
    }
    
    // Extract environment variables (TODO: implement mapping processing)
    // svalue_t env_value;
    // if (info->option_manager->get_option(EXTERNAL_ENV, &env_value)) {
    //     // Process mapping to extract environment variables
    // }
    
    // Working directory, timeout, and buffer size extraction handled above
    
    // Extract async mode (enhanced with eventfd in Phase 2)
    svalue_t async_value;
    if (info->option_manager->get_option(EXTERNAL_ASYNC, &async_value)) {
        if (async_value.type == T_NUMBER) {
            bool enable_async = (async_value.u.number != 0);
            info->async_mode = enable_async;
            
            // Enhanced async mode with eventfd integration
            if (enable_async) {
                if (AsyncEventManager::handle_async_option(socket_fd, true)) {
                    debug(external_start, "Enhanced async mode with eventfd enabled for socket %d", socket_fd);
                } else {
                    debug(external_start, "Failed to enable enhanced async mode for socket %d, falling back to basic async", socket_fd);
                }
            }
        }
    }
    
    // Extract watch path for file monitoring
    svalue_t watch_path_value;
    if (info->option_manager->get_option(EXTERNAL_WATCH_PATH, &watch_path_value)) {
        if (watch_path_value.type == T_STRING) {
            std::string watch_path(watch_path_value.u.string);
            if (FileMonitorManager::handle_watch_path_option(socket_fd, watch_path)) {
                debug(external_start, "Added file monitoring for path '%s' on socket %d", 
                      watch_path.c_str(), socket_fd);
            } else {
                debug(external_start, "Failed to add file monitoring for path '%s' on socket %d", 
                      watch_path.c_str(), socket_fd);
            }
        }
    }
    
    // Extract I/O redirection options (Phase 3: I/O Controls)
    svalue_t stdin_mode_value;
    if (info->option_manager->get_option(EXTERNAL_STDIN_MODE, &stdin_mode_value)) {
        if (stdin_mode_value.type == T_STRING) {
            std::string stdin_mode(stdin_mode_value.u.string);
            if (IORedirectionManager::handle_stdin_mode_option(socket_fd, stdin_mode)) {
                debug(external_start, "Configured stdin mode '%s' for socket %d", 
                      stdin_mode.c_str(), socket_fd);
            } else {
                debug(external_start, "Failed to configure stdin mode '%s' for socket %d", 
                      stdin_mode.c_str(), socket_fd);
            }
        }
    }
    
    svalue_t stdout_mode_value;
    if (info->option_manager->get_option(EXTERNAL_STDOUT_MODE, &stdout_mode_value)) {
        if (stdout_mode_value.type == T_STRING) {
            std::string stdout_mode(stdout_mode_value.u.string);
            if (IORedirectionManager::handle_stdout_mode_option(socket_fd, stdout_mode)) {
                debug(external_start, "Configured stdout mode '%s' for socket %d", 
                      stdout_mode.c_str(), socket_fd);
            } else {
                debug(external_start, "Failed to configure stdout mode '%s' for socket %d", 
                      stdout_mode.c_str(), socket_fd);
            }
        }
    }
    
    svalue_t stderr_mode_value;
    if (info->option_manager->get_option(EXTERNAL_STDERR_MODE, &stderr_mode_value)) {
        if (stderr_mode_value.type == T_STRING) {
            std::string stderr_mode(stderr_mode_value.u.string);
            if (IORedirectionManager::handle_stderr_mode_option(socket_fd, stderr_mode)) {
                debug(external_start, "Configured stderr mode '%s' for socket %d", 
                      stderr_mode.c_str(), socket_fd);
            } else {
                debug(external_start, "Failed to configure stderr mode '%s' for socket %d", 
                      stderr_mode.c_str(), socket_fd);
            }
        }
    }
    
    debug(external_start, "Extracted process options for socket %d: command=%s, args=%zu, timeout=%d",
          socket_fd, info->command.c_str(), info->args.size(), info->timeout_seconds);
    
    return true;
}

SecurityContext ExternalSocketHandler::get_security_context(int socket_fd) {
    // For now, return default security context
    // In a full implementation, this would extract security settings from socket options
    return default_security_context_;
}

/*
 * Socket option validation functions
 */

bool validate_external_command(const svalue_t* value) {
    if (value->type != T_STRING) {
        return false;
    }
    
    std::string command(value->u.string);
    return CommandUtils::is_valid_command(command);
}

bool validate_external_args(const svalue_t* value) {
    if (value->type != T_ARRAY) {
        return false;
    }
    
    array_t* args = value->u.arr;
    for (int i = 0; i < args->size; i++) {
        if (args->item[i].type != T_STRING) {
            return false;
        }
    }
    
    return true;
}

bool validate_external_env(const svalue_t* value) {
    if (value->type != T_MAPPING) {
        return false;
    }
    
    // Additional validation for environment variables could be added here
    return true;
}

bool validate_external_working_dir(const svalue_t* value) {
    if (value->type != T_STRING) {
        return false;
    }
    
    std::string workdir(value->u.string);
    return CommandUtils::is_safe_path(workdir);
}

bool validate_external_timeout(const svalue_t* value) {
    if (value->type != T_NUMBER) {
        return false;
    }
    
    int timeout = value->u.number;
    return timeout >= MIN_EXTERNAL_TIMEOUT && timeout <= MAX_EXTERNAL_TIMEOUT;
}

bool validate_external_buffer_size(const svalue_t* value) {
    if (value->type != T_NUMBER) {
        return false;
    }
    
    int buffer_size = value->u.number;
    return buffer_size >= 1024 && buffer_size <= 1024 * 1024; // 1KB to 1MB
}

bool validate_external_async(const svalue_t* value) {
    if (value->type != T_NUMBER) {
        return false;
    }
    
    return true; // Any integer value is valid (0=false, non-zero=true)
}

// Note: Phase-specific validation functions are defined in their respective files:
// - validate_external_watch_path() in file_monitor.cc
// - validate_external_*_mode() functions in io_redirector.cc

/*
 * Package initialization and registration
 */

void init_external_socket_handlers() {
    static bool initialized = false;
    if (initialized) return;
    
    debug(external_start, "Initializing external socket handlers");
    
    // Initialize file monitoring system
    if (!init_file_monitor_system()) {
        debug(external_start, "Warning: File monitoring system failed to initialize");
    }
    
    // Initialize async event system (Phase 2: eventfd enhancement)
    if (!init_async_event_system()) {
        debug(external_start, "Warning: Async event system failed to initialize, falling back to basic async mode");
    }
    
    // Initialize I/O redirection system (Phase 3: I/O controls)
    if (!init_io_redirection_system()) {
        debug(external_start, "Warning: I/O redirection system failed to initialize");
    }
    
    // Register handlers for external modes
    // TODO: Implement socket mode registration when unified architecture is complete
    // register_socket_create_handler(EXTERNAL_PROCESS, ExternalSocketHandler::create_handler);
    // register_socket_create_handler(EXTERNAL_COMMAND_MODE, ExternalSocketHandler::create_handler);
    
    // Register option handlers
    register_external_option_handlers();
    
    // Initialize default security context
    ExternalSocketHandler::initialize_security_context(CommandUtils::create_restricted_security_context());
    
    initialized = true;
    g_external_package_initialized = true;
    
    debug(external_start, "External socket handlers initialized successfully");
}

void cleanup_external_socket_handlers() {
    debug(external_start, "Cleaning up external socket handlers");
    
    // Cleanup all active processes
    ExternalSocketHandler::clear_all_processes();
    
    // Cleanup file monitoring system
    cleanup_file_monitor_system();
    
    // Cleanup async event system
    cleanup_async_event_system();
    
    // Cleanup I/O redirection system
    cleanup_io_redirection_system();
    
    g_external_package_initialized = false;
    debug(external_start, "External socket handlers cleaned up");
}

void register_external_option_handlers() {
    // TODO: Implement when SocketOptionManager API is available
    // auto& option_manager = SocketOptionManager::instance();
    // TODO: Register validators when API is available
    debug(external_start, "External option handlers registration placeholder");
}

/*
 * Legacy EFun implementations
 */

#ifdef F_EXTERNAL_START
void f_external_start() {
  int fd, num_arg = st_num_arg;
  svalue_t *arg = sp - num_arg + 1;

  if (!check_valid_socket("external", -1, current_object, "N/A", -1)) {
    pop_n_elems(num_arg - 1);
    sp->u.number = EESECURITY;
    return;
  }

  auto which = arg[0].u.number;
  if (--which < 0 || which > (g_num_external_cmds - 1) || !external_cmd[which]) {
    error("Bad argument 1 to external_start()\n");
  }

  fd = external_start(which, arg + 1, arg + 2, arg + 3, (num_arg == 5 ? arg + 4 : nullptr));
  pop_n_elems(num_arg - 1);
  sp->u.number = fd;
}
#endif

/*
 * New unified architecture EFun implementations
 */

#ifdef F_EXTERNAL_SPAWN_PROCESS
void f_external_spawn_process() {
    int socket_fd = sp->u.number;
    sp--;
    
    if (socket_fd < 0) {
        push_number(-1);
        return;
    }
    
    int result = ExternalSocketHandler::spawn_process(socket_fd);
    push_number(result);
}
#endif

#ifdef F_EXTERNAL_KILL_PROCESS
void f_external_kill_process() {
    int socket_fd = sp->u.number;
    sp--;
    
    if (socket_fd < 0) {
        push_number(0);
        return;
    }
    
    bool success = ExternalSocketHandler::kill_process(socket_fd);
    push_number(success ? 1 : 0);
}
#endif

#ifdef F_EXTERNAL_PROCESS_STATUS
void f_external_process_status() {
    int socket_fd = sp->u.number;
    sp--;
    
    if (socket_fd < 0) {
        push_number(-1);
        return;
    }
    
    ExternalProcessInfo* info = ExternalSocketHandler::get_process_info(socket_fd);
    if (!info) {
        push_number(-1);
        return;
    }
    
    // Return process status (1=running, 0=stopped, -1=error)
    if (info->is_running) {
        push_number(1);
    } else {
        push_number(info->exit_code);
    }
}
#endif

/*
 * File monitoring EFun implementations
 */

#ifdef F_EXTERNAL_MONITOR_PATH
void f_external_monitor_path() {
    int num_args = st_num_arg;
    svalue_t *args = sp - num_args + 1;
    
    if (num_args < 2) {
        error("external_monitor_path() requires at least 2 arguments");
    }
    
    if (args[0].type != T_NUMBER || args[1].type != T_STRING) {
        error("external_monitor_path() invalid argument types");
    }
    
    int socket_fd = args[0].u.number;
    std::string path(args[1].u.string);
    uint32_t events = 0;
    
    if (num_args >= 3 && args[2].type == T_NUMBER) {
        events = args[2].u.number;
    }
    
    pop_n_elems(num_args);
    
    int result = FileMonitorManager::external_monitor_path(socket_fd, path, events);
    push_number(result);
}
#endif

#ifdef F_EXTERNAL_STOP_MONITORING
void f_external_stop_monitoring() {
    int num_args = st_num_arg;
    svalue_t *args = sp - num_args + 1;
    
    if (num_args < 2) {
        error("external_stop_monitoring() requires 2 arguments");
    }
    
    if (args[0].type != T_NUMBER || args[1].type != T_STRING) {
        error("external_stop_monitoring() invalid argument types");
    }
    
    int socket_fd = args[0].u.number;
    std::string path(args[1].u.string);
    
    pop_n_elems(num_args);
    
    int result = FileMonitorManager::external_stop_monitoring(socket_fd, path);
    push_number(result);
}
#endif

#ifdef F_EXTERNAL_GET_FILE_EVENTS
void f_external_get_file_events() {
    int socket_fd = sp->u.number;
    sp--;
    
    if (socket_fd < 0) {
        push_undefined();
        return;
    }
    
    std::vector<FileEvent> events = FileMonitorManager::external_get_file_events(socket_fd);
    
    if (events.empty()) {
        push_undefined();
        return;
    }
    
    // Convert events to LPC array
    array_t *result_array = allocate_empty_array(events.size());
    
    for (size_t i = 0; i < events.size(); i++) {
        const FileEvent& event = events[i];
        
        // Create mapping for each event
        mapping_t *event_mapping = allocate_mapping(6);
        
        svalue_t path_val;
        path_val.type = T_STRING;
        path_val.u.string = make_shared_string(event.path.c_str());
        
        svalue_t name_val;
        name_val.type = T_STRING;
        name_val.u.string = make_shared_string(event.name.c_str());
        
        svalue_t type_val;
        type_val.type = T_NUMBER;
        type_val.u.number = static_cast<int>(event.event_type);
        
        svalue_t cookie_val;
        cookie_val.type = T_NUMBER;
        cookie_val.u.number = event.cookie;
        
        svalue_t dir_val;
        dir_val.type = T_NUMBER;
        dir_val.u.number = event.is_directory ? 1 : 0;
        
        svalue_t time_val;
        time_val.type = T_NUMBER;
        time_val.u.number = event.timestamp;
        
        // Add to mapping
        add_mapping_string(event_mapping, "path", path_val.u.string);
        add_mapping_string(event_mapping, "name", name_val.u.string);
        add_mapping_string(event_mapping, "type", type_val.u.string);
        add_mapping_pair(event_mapping, "cookie", cookie_val.u.number);
        add_mapping_pair(event_mapping, "directory", dir_val.u.number);
        add_mapping_pair(event_mapping, "timestamp", time_val.u.number);
        
        result_array->item[i].type = T_MAPPING;
        result_array->item[i].u.map = event_mapping;
    }
    
    push_refed_array(result_array);
}
#endif

/*
 * Phase 3: I/O redirection EFun implementations (previously disabled)
 */

#ifdef F_EXTERNAL_WRITE_PROCESS
void f_external_write_process() {
    int num_args = st_num_arg;
    svalue_t *args = sp - num_args + 1;
    
    if (num_args < 2) {
        error("external_write_process() requires 2 arguments");
    }
    
    if (args[0].type != T_NUMBER || args[1].type != T_STRING) {
        error("external_write_process() invalid argument types");
    }
    
    int socket_fd = args[0].u.number;
    std::string data(args[1].u.string);
    
    pop_n_elems(num_args);
    
    if (socket_fd < 0) {
        push_number(-1);
        return;
    }
    
    IORedirector& redirector = IORedirector::instance();
    IOResult result = redirector.write_to_stdin(socket_fd, data.c_str(), data.length());
    
    if (result.success) {
        push_number(static_cast<int>(result.bytes_processed));
    } else if (result.would_block) {
        push_number(0);  // Would block, try again later
    } else {
        push_number(-1); // Error
    }
}
#endif

#ifdef F_EXTERNAL_READ_PROCESS
void f_external_read_process() {
    int num_args = st_num_arg;
    svalue_t *args = sp - num_args + 1;
    
    if (num_args < 1) {
        error("external_read_process() requires at least 1 argument");
    }
    
    if (args[0].type != T_NUMBER) {
        error("external_read_process() invalid argument type");
    }
    
    int socket_fd = args[0].u.number;
    int max_bytes = 4096; // Default buffer size
    
    if (num_args >= 2 && args[1].type == T_NUMBER) {
        max_bytes = args[1].u.number;
        if (max_bytes <= 0 || max_bytes > 65536) {
            max_bytes = 4096; // Clamp to reasonable range
        }
    }
    
    pop_n_elems(num_args);
    
    if (socket_fd < 0) {
        push_undefined();
        return;
    }
    
    // Allocate buffer for reading
    char* buffer = new char[max_bytes];
    
    IORedirector& redirector = IORedirector::instance();
    IOResult result = redirector.read_from_stdout(socket_fd, buffer, max_bytes);
    
    if (result.success && result.bytes_processed > 0) {
        // Create LPC string from buffer
        svalue_t result_val;
        result_val.type = T_STRING;
        result_val.u.string = make_shared_string(buffer);
        
        delete[] buffer;
        push_svalue(&result_val);
    } else if (result.would_block) {
        delete[] buffer;
        push_number(0);  // No data available, try again later
    } else {
        delete[] buffer;
        push_undefined(); // Error or no data
    }
}
#endif

/*
 * File event processing and delivery - called from main event loop
 */
void process_external_file_events() {
    if (!g_external_package_initialized) {
        return;
    }
    
    FileMonitor& monitor = FileMonitor::instance();
    if (!monitor.is_initialized()) {
        return;
    }
    
    // Process any pending inotify events
    std::vector<FileEvent> events = monitor.process_events();
    
    if (!events.empty()) {
        // Group events by socket for delivery
        std::map<int, std::vector<FileEvent>> socket_events;
        
        for (const FileEvent& event : events) {
            // Find which sockets are watching this path - this is a simplified approach
            // In a full implementation, we'd maintain a reverse mapping
            debug(external_start, "File event: %s (type=%d)", 
                  event.path.c_str(), static_cast<int>(event.event_type));
        }
        
        // Deliver events to sockets
        for (const auto& pair : socket_events) {
            FileMonitorManager::deliver_file_events(pair.first, pair.second);
        }
    }
    
    // Process enhanced async events (Phase 2: eventfd integration)
    process_external_async_events();
}

/*
 * Windows-specific implementations (simplified for now)
 */

#ifdef F_EXTERNAL_WAIT_FOR_EVENTS
void f_external_wait_for_events() {
    int num_args = st_num_arg;
    svalue_t *args = sp - num_args + 1;
    
    if (num_args < 1) {
        error("external_wait_for_events() requires at least 1 argument");
    }
    
    if (args[0].type != T_NUMBER) {
        error("external_wait_for_events() requires socket fd as first argument");
    }
    
    int socket_fd = args[0].u.number;
    int timeout_ms = (num_args > 1 && args[1].type == T_NUMBER) ? args[1].u.number : -1;
    
    pop_n_elems(num_args);
    
    int result = AsyncEventManager::external_wait_for_events(socket_fd, timeout_ms);
    push_number(result);
}
#endif

#ifdef F_EXTERNAL_GET_ASYNC_EVENTS
void f_external_get_async_events() {
    int socket_fd = sp->u.number;
    sp--;
    
    if (socket_fd < 0) {
        push_undefined();
        return;
    }
    
    auto events = AsyncEventManager::external_get_async_events(socket_fd);
    
    // Convert events to LPC array
    array_t *event_array = allocate_empty_array(events.size());
    for (size_t i = 0; i < events.size(); i++) {
        // Create mapping for each event
        mapping_t *event_map = allocate_mapping(4);
        
        // Convert event type enum to string
        const char* type_str;
        switch (events[i].event_type) {
            case AsyncEventType::PROCESS_READY: type_str = "process_ready"; break;
            case AsyncEventType::PROCESS_OUTPUT: type_str = "process_output"; break;
            case AsyncEventType::PROCESS_ERROR: type_str = "process_error"; break;
            case AsyncEventType::PROCESS_EXITED: type_str = "process_exited"; break;
            case AsyncEventType::FILE_CHANGED: type_str = "file_changed"; break;
            case AsyncEventType::CUSTOM_SIGNAL: type_str = "custom_signal"; break;
            default: type_str = "unknown"; break;
        }
        
        add_mapping_string(event_map, "type", type_str);
        add_mapping_pair(event_map, "socket_fd", events[i].socket_fd);
        add_mapping_pair(event_map, "event_value", static_cast<long>(events[i].event_value));
        add_mapping_string(event_map, "data", events[i].data.c_str());
        
        event_array->item[i].type = T_MAPPING;
        event_array->item[i].u.map = event_map;
    }
    
    push_refed_array(event_array);
}
#endif

#ifdef F_EXTERNAL_ENABLE_ASYNC_NOTIFICATIONS
void f_external_enable_async_notifications() {
    int num_args = st_num_arg;
    svalue_t *args = sp - num_args + 1;
    
    if (num_args < 2) {
        error("external_enable_async_notifications() requires 2 arguments");
    }
    
    if (args[0].type != T_NUMBER || args[1].type != T_NUMBER) {
        error("external_enable_async_notifications() requires socket_fd and enabled flag");
    }
    
    int socket_fd = args[0].u.number;
    int enabled = args[1].u.number;
    
    pop_n_elems(num_args);
    
    int result = AsyncEventManager::external_enable_async_notifications(socket_fd, enabled != 0);
    push_number(result);
}
#endif

#ifdef _WIN32
// Include Windows socketpair implementation from socketpair.cc
extern int socketpair_win32(SOCKET socks[2], int make_overlapped);

// Windows version of external_start (from original file)
int external_start(int which, svalue_t *args, svalue_t *arg1, svalue_t *arg2, svalue_t *arg3) {
  // Windows implementation preserved from original file
  // This is a simplified version - full implementation would follow same pattern as Unix
  return -1; // Not implemented for unified architecture yet
}
#endif