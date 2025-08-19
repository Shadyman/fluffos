/* Compression efun specifications.
 * Started Wed Mar 21 01:52:25 PST 2001
 * by David Bennett (ddt@discworld.imaginary.com)
 * Extended for socket compression support
 */

int compress_file(string, string | void);
int uncompress_file(string, string | void);

buffer compress(string | buffer);
buffer uncompress(string | buffer);

/* Socket compression stream efuns */
int compress_socket_create(int, string | function, string | function | void);
int compress_socket_write(int, mixed);
buffer compress_socket_read(int);
int compress_socket_flush(int);
string compress_socket_algorithm(int);
