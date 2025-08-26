// Legacy compatibility function
int external_start(int, string | string *, string | function, string | function, string | function | void);

// Core external process functions
int external_spawn_process(int);
int external_kill_process(int);
int external_process_status(int);
// TODO: These functions are declared but not implemented - disabled for now
// int external_write_process(int, string);
// mixed external_read_process(int, int);
