// Legacy compatibility function
int external_start(int, string | string *, string | function, string | function, string | function | void);

// Core external process functions
int external_spawn_process(int);
int external_kill_process(int);
int external_process_status(int);

// File monitoring functions (Phase 1: inotify implementation)
int external_monitor_path(int, string, int | void);
int external_stop_monitoring(int, string);
mixed *external_get_file_events(int);

// Enhanced async functions (Phase 2: eventfd integration)
int external_wait_for_events(int, int | void);
mixed *external_get_async_events(int);
int external_enable_async_notifications(int, int);

// I/O redirection functions (Phase 3: I/O controls - NOW ENABLED)
int external_write_process(int, string);
mixed external_read_process(int, int | void);
