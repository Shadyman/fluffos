// Legacy compatibility function
int external_start(int, string | string *, string | function, string | function, string | function | void);

// Core external process functions
int external_spawn_process(int);
int external_kill_process(int);
int external_process_status(int);