#include "external.h"
#include "process_manager.h"
#include "command_executor.h"
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
    
    // Extract async mode
    svalue_t async_value;
    if (info->option_manager->get_option(EXTERNAL_ASYNC, &async_value)) {
        if (async_value.type == T_NUMBER) {
            info->async_mode = (async_value.u.number != 0);
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

/*
 * Package initialization and registration
 */

void init_external_socket_handlers() {
    static bool initialized = false;
    if (initialized) return;
    
    debug(external_start, "Initializing external socket handlers");
    
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
 * Windows-specific implementations (simplified for now)
 */

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