
/*
 * socket efuns
 */
    int socket_create(int, string | function, string | function | void);
    int socket_bind(int, int, string | void);
    int socket_listen(int, string | function);
    int socket_accept(int, string | function, string | function);
    int socket_connect(int, string, string | function, string | function);
    int socket_write(int, mixed, string | void);
    int socket_close(int);
    int socket_release(int, object, string | function);
    int socket_acquire(int, string | function, string | function, string | function);
    string socket_error(int);
    string socket_address(int | object, int default: 0);
    void socket_set_option(int, int, mixed);
    mixed socket_get_option(int, int);
    /*
     * ret[0] = (int) fd
     * ret[1] = (string) state
     * ret[2] = (string) mode
     * ret[3] = (string) local address
     * ret[4] = (string) remote address
     * ret[5] = (object) owner
     */
    mixed *socket_status(void | int);

/*
 * HTTP socket efuns - Phase 2 Unified Socket Architecture
 * HTTPHandler implementation now complete - functions re-enabled
 */
    int socket_set_http_mode(int, mapping | void);
    mapping socket_http_request(int);
    int socket_http_response(int, int, string | void, mapping | void);
    int socket_http_error(int, int, string | void);
    int socket_http_json(int, string, int | void);
    int socket_http_redirect(int, string, int | void);
    mapping socket_http_headers(int);
    int socket_is_http_mode(int);
    int socket_http_reset(int);
    int socket_http_keepalive(int);
