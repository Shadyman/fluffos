/*
 * HTTP package efuns for FluffOS
 * 
 * Core HTTP server and client functionality
 */
int http_start_server(int, string | function, mapping);
int http_stop_server(int);
int http_send_response(int, mapping);
int http_send_request(string, mapping, string | function | void);
