int external_start(int, string | string *, string | function, string | function, string | function | void);

/* External process integration socket efuns */
int external_socket_create(int, string | function, string | function | void);
int external_eventfd_signal(int, int);
int external_eventfd_read(int);
int external_inotify_add_watch(int, string, int);
int external_inotify_remove_watch(int, int);
mapping external_inotify_read_events(int);
