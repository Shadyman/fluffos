/*
 * HTTP package efuns for FluffOS
 * 
 * Core HTTP server and client functionality
 */
int http_start_server(int, string | function, mapping);
int http_stop_server(int);
int http_send_response(int, mapping);
int http_send_request(string, mapping, string | function | void);

/* WebSocket integration efuns */
int websocket_socket_create(int, string | function, string | function | void);
int websocket_send_message(int, string | buffer);
int websocket_send_binary(int, buffer);
int websocket_close_connection(int, int | void);
mapping websocket_get_info(int);

/* MQTT client efuns */
int mqtt_socket_create(string, string | function, string | function | void);
int mqtt_publish(int, string, string | buffer, int | void);
int mqtt_subscribe(int, string | string *, int | void);
int mqtt_unsubscribe(int, string | string *);
int mqtt_disconnect(int);
