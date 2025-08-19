/* Compression efun stuff.
 * Started Wed Mar 21 01:52:25 PST 2001
 * by David Bennett (ddt@discworld.imaginary.com)
 */

#include "base/package_api.h"

#include "packages/core/file.h"

#include <zlib.h>

#ifdef PACKAGE_SOCKETS
#include "packages/sockets/socket_efuns.h"
#endif

#define GZ_EXTENSION ".gz"

enum { COMPRESS_BUF_SIZE = 8096 };

#ifdef F_COMPRESS_FILE
void f_compress_file() {
  int readb;
  int len;
  int const num_arg = st_num_arg;
  const char *input_file;
  const char *output_file;
  const char *real_input_file;
  const char *real_output_file;
  char *tmpout;
  gzFile out_file;
  FILE *in_file;
  char buf[4096];
  char outname[1024];

  // Not a string?  Error!
  if ((sp - num_arg + 1)->type != T_STRING) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  input_file = (sp - num_arg + 1)->u.string;
  if (num_arg == 2) {
    if (((sp - num_arg + 2)->type != T_STRING)) {
      pop_n_elems(num_arg);
      push_number(0);
      return;
    }
    output_file = (sp - num_arg + 2)->u.string;
  } else {
    len = strlen(input_file);
    if (!strcmp(input_file + len - strlen(GZ_EXTENSION), GZ_EXTENSION)) {
      // Already compressed...
      pop_n_elems(num_arg);
      push_number(0);
      return;
    }
    tmpout = new_string(strlen(input_file) + strlen(GZ_EXTENSION), "compress_file");
    strcpy(tmpout, input_file);
    strcat(tmpout, GZ_EXTENSION);
    output_file = tmpout;
  }

  real_output_file = check_valid_path(output_file, current_object, "compress_file", 1);
  if (!real_output_file) {
    FREE_MSTR(output_file);
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }
  // Copy it into our little buffer.
  strcpy(outname, real_output_file);
  // Free the old file.
  if (num_arg != 2) {
    FREE_MSTR(output_file);
  }
  output_file = outname;

  real_input_file = check_valid_path(input_file, current_object, "compress_file", 0);
  if (!real_input_file) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  in_file = fopen(real_input_file, "rb");
  if (!in_file) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  out_file = gzopen(output_file, "wb");
  if (!out_file) {
    fclose(in_file);
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  do {
    readb = fread(buf, 1, 4096, in_file);
    gzwrite(out_file, buf, readb);
  } while (readb == 4096);
  fclose(in_file);
  gzclose(out_file);

  unlink(real_input_file);

  pop_n_elems(num_arg);
  push_number(1);
}
#endif

#ifdef F_UNCOMPRESS_FILE
void f_uncompress_file() {
  int readb;
  int len;
  int const num_arg = st_num_arg;
  const char *input_file;
  const char *output_file;
  const char *real_input_file;
  const char *real_output_file;
  FILE *out_file;
  gzFile in_file;
  char buf[4196];
  char outname[1024];

  // Not a string?  Error!
  if ((sp - num_arg + 1)->type != T_STRING) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  input_file = (sp - num_arg + 1)->u.string;
  if (num_arg == 2) {
    if (((sp - num_arg + 2)->type != T_STRING)) {
      pop_n_elems(num_arg);
      push_number(0);
      return;
    }
    output_file = (sp - num_arg + 2)->u.string;
  } else {
    char *tmp;
    len = strlen(input_file);
    if (strcmp(input_file + len - strlen(GZ_EXTENSION), GZ_EXTENSION) != 0) {
      // Not compressed...
      pop_n_elems(num_arg);
      push_number(0);
      return;
    }
    tmp = new_string(len, "compress_file");
    strcpy(tmp, input_file);
    tmp[len - strlen(GZ_EXTENSION)] = 0;
    output_file = tmp;
  }

  real_output_file = check_valid_path(output_file, current_object, "compress_file", 1);
  if (!real_output_file) {
    if (num_arg != 2) {
      FREE_MSTR(output_file);
    }

    pop_n_elems(num_arg);
    push_number(0);
    return;
  }
  // Copy it into our little buffer.
  strcpy(outname, real_output_file);
  if (num_arg != 2) {
    FREE_MSTR(output_file);
  }
  output_file = outname;

  real_input_file = check_valid_path(input_file, current_object, "compress_file", 0);
  if (!real_input_file) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  in_file = gzopen(real_input_file, "rb");
  if (!in_file) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  out_file = fopen(output_file, "wb");
  if (!out_file) {
    gzclose(in_file);
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  do {
    readb = gzread(in_file, buf, 4096);
    fwrite(buf, 1, readb, out_file);
  } while (readb == 4096);
  gzclose(in_file);
  fclose(out_file);

  unlink(real_input_file);

  pop_n_elems(num_arg);
  push_number(1);
}
#endif

#ifdef F_COMPRESS
void f_compress() {
  unsigned char *buffer;
  unsigned char *input;
  int size;
  buffer_t *real_buffer;
  uLongf new_size;

  if (sp->type == T_STRING) {
    size = SVALUE_STRLEN(sp);
    input = (unsigned char *)sp->u.string;
  } else if (sp->type == T_BUFFER) {
    size = sp->u.buf->size;
    input = sp->u.buf->item;
  } else {
    pop_n_elems(st_num_arg);
    push_undefined();
    return;
  }

  new_size = compressBound(size);
  // Make it a little larger as specified in the docs.
  buffer = reinterpret_cast<unsigned char *>(DMALLOC(new_size, TAG_TEMPORARY, "compress"));
  compress(buffer, &new_size, input, size);

  // Shrink it down.
  pop_n_elems(st_num_arg);
  real_buffer = allocate_buffer(new_size);
  write_buffer(real_buffer, 0, reinterpret_cast<char *>(buffer), new_size);
  FREE(buffer);
  push_refed_buffer(real_buffer);
}
#endif

#ifdef F_UNCOMPRESS
static void *zlib_alloc(void * /*opaque*/, unsigned int items, unsigned int size) {
  return DCALLOC(items, size, TAG_TEMPORARY, "zlib_alloc");
}

static void zlib_free(void * /*opaque*/, void *address) { FREE(address); }

void f_uncompress() {
  z_stream *compressed;
  unsigned char compress_buf[COMPRESS_BUF_SIZE];
  unsigned char *output_data = nullptr;
  int len;
  int pos;
  buffer_t *buffer;
  int ret;

  if (sp->type == T_BUFFER) {
    buffer = sp->u.buf;
  } else {
    pop_n_elems(st_num_arg);
    push_undefined();
    return;
  }

  compressed =
      reinterpret_cast<z_stream *>(DMALLOC(sizeof(z_stream), TAG_INTERACTIVE, "start_compression"));
  compressed->next_in = buffer->item;
  compressed->avail_in = buffer->size;
  compressed->next_out = compress_buf;
  compressed->avail_out = COMPRESS_BUF_SIZE;
  compressed->zalloc = zlib_alloc;
  compressed->zfree = zlib_free;
  compressed->opaque = nullptr;

  if (inflateInit(compressed) != Z_OK) {
    FREE(compressed);
    pop_n_elems(st_num_arg);
    error("inflateInit failed");
  }

  len = 0;
  output_data = nullptr;
  do {
    ret = inflate(compressed, 0);
    if (ret == Z_OK || ret == Z_STREAM_END) {
      pos = len;
      len += COMPRESS_BUF_SIZE - compressed->avail_out;
      if (!output_data) {
        output_data = reinterpret_cast<unsigned char *>(DMALLOC(len, TAG_TEMPORARY, "uncompress"));
      } else {
        output_data = reinterpret_cast<unsigned char *>(
            DREALLOC(output_data, len, TAG_TEMPORARY, "uncompress"));
      }
      memcpy(output_data + pos, compress_buf, len - pos);
      compressed->next_out = compress_buf;
      compressed->avail_out = COMPRESS_BUF_SIZE;
    }
  } while (ret == Z_OK);

  inflateEnd(compressed);

  pop_n_elems(st_num_arg);

  if (ret == Z_STREAM_END) {
    buffer = allocate_buffer(len);
    write_buffer(buffer, 0, reinterpret_cast<char *>(output_data), len);
    FREE(output_data);
    push_refed_buffer(buffer);
    FREE(compressed);
  } else {
    FREE(compressed);
    error("inflate: no ZSTREAM_END\n");
  }
}
#endif

/* Socket compression stream efuns */

// Compression socket handlers
static int compress_socket_create_handler(enum socket_mode mode, svalue_t *read_callback, svalue_t *close_callback) {
  // For compression modes, we create the underlying socket and mark it as compressed
  // The actual compression will be handled by socket read/write wrappers
  
  enum socket_mode base_mode;
  switch (mode) {
    case STREAM_COMPRESSED:
      base_mode = STREAM;
      break;
    case STREAM_TLS_COMPRESSED:
      base_mode = STREAM_TLS;
      break;
    case DATAGRAM_COMPRESSED:
      base_mode = DATAGRAM;
      break;
    default:
      return -1;  // Invalid mode
  }
  
  // Create the underlying socket with the base mode
  // The socket system will set the S_COMPRESSED flag automatically
  return socket_create(base_mode, read_callback, close_callback);
}

// Initialize compression socket handlers
static void init_compress_socket_handlers() {
  static int initialized = 0;
  if (initialized) return;
  
#ifdef PACKAGE_SOCKETS
  // Register handlers for compression modes
  register_socket_create_handler(STREAM_COMPRESSED, compress_socket_create_handler);
  register_socket_create_handler(STREAM_TLS_COMPRESSED, compress_socket_create_handler);
  register_socket_create_handler(DATAGRAM_COMPRESSED, compress_socket_create_handler);
#endif
  
  initialized = 1;
}

#ifdef F_COMPRESS_SOCKET_CREATE
void f_compress_socket_create() {
  try {
    // Initialize handlers if not done already
    init_compress_socket_handlers();
    
    int mode = sp->u.number;
    
    // Validate compression socket mode
    if (mode != STREAM_COMPRESSED && mode != STREAM_TLS_COMPRESSED && mode != DATAGRAM_COMPRESSED) {
      error("compress_socket_create: Invalid compression socket mode %d\n", mode);
      return;
    }
    
    // Get the callback parameters
    svalue_t *close_callback = (st_num_arg >= 3) ? sp - 1 : nullptr;
    svalue_t *read_callback = sp - (st_num_arg >= 3 ? 2 : 1);
    
    // Call the compression handler directly
    int result = compress_socket_create_handler(static_cast<enum socket_mode>(mode), read_callback, close_callback);
    
    // Clean up stack and return result
    pop_n_elems(st_num_arg);
    push_number(result);
    
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("compress_socket_create: %s\n", e.what());
  }
}
#endif

#ifdef F_COMPRESS_SOCKET_WRITE
void f_compress_socket_write() {
  try {
    error("compress_socket_write: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("compress_socket_write: %s\n", e.what());
  }
}
#endif

#ifdef F_COMPRESS_SOCKET_READ
void f_compress_socket_read() {
  try {
    error("compress_socket_read: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("compress_socket_read: %s\n", e.what());
  }
}
#endif

#ifdef F_COMPRESS_SOCKET_FLUSH
void f_compress_socket_flush() {
  try {
    error("compress_socket_flush: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("compress_socket_flush: %s\n", e.what());
  }
}
#endif

#ifdef F_COMPRESS_SOCKET_ALGORITHM
void f_compress_socket_algorithm() {
  try {
    error("compress_socket_algorithm: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("compress_socket_algorithm: %s\n", e.what());
  }
}
#endif
