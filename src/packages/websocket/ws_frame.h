#ifndef PACKAGES_WEBSOCKET_WS_FRAME_H_
#define PACKAGES_WEBSOCKET_WS_FRAME_H_

/*
 * WebSocket Frame Processing
 * 
 * Low-level WebSocket frame parsing, generation, and manipulation
 * according to RFC 6455 specification.
 */

#include "packages/websocket/websocket.h"
#include <vector>
#include <string>
#include <cstdint>

/*
 * WebSocket Frame Constants (RFC 6455)
 */

// Frame structure constants
#define WS_FRAME_HEADER_MIN_SIZE 2
#define WS_FRAME_HEADER_MAX_SIZE 14
#define WS_FRAME_MASK_SIZE 4

// Frame flags
#define WS_FRAME_FLAG_FIN    0x80
#define WS_FRAME_FLAG_RSV1   0x40
#define WS_FRAME_FLAG_RSV2   0x20
#define WS_FRAME_FLAG_RSV3   0x10
#define WS_FRAME_FLAG_OPCODE 0x0F
#define WS_FRAME_FLAG_MASK   0x80
#define WS_FRAME_FLAG_LEN    0x7F

// Extended payload length indicators
#define WS_FRAME_LEN_16_BIT  126
#define WS_FRAME_LEN_64_BIT  127

// Maximum payload sizes
#define WS_FRAME_MAX_SMALL_PAYLOAD  125
#define WS_FRAME_MAX_MEDIUM_PAYLOAD 65535
#define WS_FRAME_MAX_LARGE_PAYLOAD  0x7FFFFFFFFFFFFFFFULL

/*
 * WebSocket Frame Parsing Results
 */
enum ws_frame_parse_result {
    WS_FRAME_PARSE_SUCCESS = 0,
    WS_FRAME_PARSE_INCOMPLETE = 1,
    WS_FRAME_PARSE_ERROR = -1,
    WS_FRAME_PARSE_INVALID_OPCODE = -2,
    WS_FRAME_PARSE_INVALID_LENGTH = -3,
    WS_FRAME_PARSE_INVALID_MASK = -4,
    WS_FRAME_PARSE_PROTOCOL_ERROR = -5,
    WS_FRAME_PARSE_TOO_LARGE = -6
};

/*
 * WebSocket Frame Validation Results
 */
enum ws_frame_validation_result {
    WS_FRAME_VALID = 0,
    WS_FRAME_INVALID_OPCODE = -1,
    WS_FRAME_INVALID_RSV = -2,
    WS_FRAME_INVALID_LENGTH = -3,
    WS_FRAME_INVALID_CONTROL_FRAME = -4,
    WS_FRAME_INVALID_CONTINUATION = -5,
    WS_FRAME_INVALID_UTF8 = -6
};

/*
 * WebSocket Frame Parser
 */
class WebSocketFrameParser {
private:
    // Parser state
    enum parse_state {
        STATE_HEADER,
        STATE_EXTENDED_LENGTH,
        STATE_MASK,
        STATE_PAYLOAD,
        STATE_COMPLETE
    };
    
    parse_state state_;
    ws_frame current_frame_;
    std::vector<uint8_t> buffer_;
    size_t bytes_needed_;
    size_t bytes_parsed_;
    
    // Parsing configuration
    size_t max_frame_size_;
    bool require_masking_;
    bool validate_utf8_;
    
public:
    WebSocketFrameParser(size_t max_frame_size = WS_FRAME_MAX_LARGE_PAYLOAD,
                        bool require_masking = false, bool validate_utf8 = true);
    ~WebSocketFrameParser();
    
    // Frame parsing
    ws_frame_parse_result parse(const uint8_t* data, size_t len, 
                               size_t& bytes_consumed);
    ws_frame_parse_result parse(const std::vector<uint8_t>& data, 
                               size_t& bytes_consumed);
    
    // Get parsed frame
    bool has_complete_frame() const { return state_ == STATE_COMPLETE; }
    const ws_frame& get_frame() const { return current_frame_; }
    ws_frame take_frame();
    
    // Parser state
    void reset();
    bool is_parsing() const { return state_ != STATE_COMPLETE; }
    size_t get_bytes_needed() const { return bytes_needed_; }
    
    // Configuration
    void set_max_frame_size(size_t max_size) { max_frame_size_ = max_size; }
    void set_require_masking(bool require) { require_masking_ = require; }
    void set_validate_utf8(bool validate) { validate_utf8_ = validate; }
    
private:
    // Parsing helpers
    ws_frame_parse_result parse_header(const uint8_t* data, size_t len, size_t& consumed);
    ws_frame_parse_result parse_extended_length(const uint8_t* data, size_t len, size_t& consumed);
    ws_frame_parse_result parse_mask(const uint8_t* data, size_t len, size_t& consumed);
    ws_frame_parse_result parse_payload(const uint8_t* data, size_t len, size_t& consumed);
    
    // Validation helpers
    bool is_valid_opcode(uint8_t opcode) const;
    bool is_control_frame(uint8_t opcode) const;
    bool validate_control_frame() const;
    bool validate_continuation_frame() const;
    
    // Utility functions
    uint64_t read_big_endian_64(const uint8_t* data) const;
    uint16_t read_big_endian_16(const uint8_t* data) const;
    void unmask_payload();
};

/*
 * WebSocket Frame Builder
 */
class WebSocketFrameBuilder {
private:
    // Default configuration
    bool auto_mask_;
    uint32_t mask_key_;
    size_t max_frame_size_;
    
public:
    WebSocketFrameBuilder(bool auto_mask = false, 
                         size_t max_frame_size = WS_FRAME_MAX_LARGE_PAYLOAD);
    ~WebSocketFrameBuilder();
    
    // Frame building
    std::vector<uint8_t> build_text_frame(const std::string& text, bool fin = true,
                                         bool mask = false, uint32_t mask_key = 0);
    std::vector<uint8_t> build_binary_frame(const std::vector<uint8_t>& data, bool fin = true,
                                           bool mask = false, uint32_t mask_key = 0);
    std::vector<uint8_t> build_close_frame(uint16_t close_code = WS_CLOSE_NORMAL,
                                          const std::string& reason = "",
                                          bool mask = false, uint32_t mask_key = 0);
    std::vector<uint8_t> build_ping_frame(const std::string& payload = "",
                                         bool mask = false, uint32_t mask_key = 0);
    std::vector<uint8_t> build_pong_frame(const std::string& payload = "",
                                         bool mask = false, uint32_t mask_key = 0);
    std::vector<uint8_t> build_continuation_frame(const std::vector<uint8_t>& data, 
                                                 bool fin = true, bool mask = false,
                                                 uint32_t mask_key = 0);
    
    // Generic frame building
    std::vector<uint8_t> build_frame(const ws_frame& frame);
    std::vector<uint8_t> build_frame(ws_frame_opcode opcode, 
                                    const std::vector<uint8_t>& payload,
                                    bool fin = true, bool mask = false,
                                    uint32_t mask_key = 0);
    
    // Configuration
    void set_auto_mask(bool auto_mask) { auto_mask_ = auto_mask; }
    void set_default_mask_key(uint32_t mask_key) { mask_key_ = mask_key; }
    void set_max_frame_size(size_t max_size) { max_frame_size_ = max_size; }
    
    // Utility functions
    static uint32_t generate_mask_key();
    static std::vector<uint8_t> mask_payload(const std::vector<uint8_t>& payload, 
                                            uint32_t mask_key);
    static void mask_payload_inplace(std::vector<uint8_t>& payload, uint32_t mask_key);
    
private:
    // Building helpers
    std::vector<uint8_t> build_header(ws_frame_opcode opcode, uint64_t payload_len,
                                     bool fin, bool mask, uint32_t mask_key);
    void write_big_endian_16(std::vector<uint8_t>& buffer, uint16_t value);
    void write_big_endian_64(std::vector<uint8_t>& buffer, uint64_t value);
    
    // Validation helpers
    bool validate_payload_size(uint64_t size) const;
    bool validate_control_frame_payload(ws_frame_opcode opcode, uint64_t size) const;
};

/*
 * WebSocket Frame Utilities
 */
class WebSocketFrameUtils {
public:
    // Frame validation
    static ws_frame_validation_result validate_frame(const ws_frame& frame);
    static bool is_valid_opcode(ws_frame_opcode opcode);
    static bool is_control_opcode(ws_frame_opcode opcode);
    static bool is_data_opcode(ws_frame_opcode opcode);
    
    // Frame analysis
    static bool is_control_frame(const ws_frame& frame);
    static bool is_data_frame(const ws_frame& frame);
    static bool is_continuation_frame(const ws_frame& frame);
    static bool is_final_frame(const ws_frame& frame);
    
    // Payload utilities
    static std::string extract_text_payload(const ws_frame& frame);
    static std::vector<uint8_t> extract_binary_payload(const ws_frame& frame);
    static bool is_valid_utf8(const std::vector<uint8_t>& data);
    
    // Close frame utilities
    static bool parse_close_payload(const std::vector<uint8_t>& payload,
                                  uint16_t& close_code, std::string& reason);
    static std::vector<uint8_t> build_close_payload(uint16_t close_code,
                                                   const std::string& reason);
    
    // Frame fragmentation
    static std::vector<ws_frame> fragment_message(const std::vector<uint8_t>& message,
                                                 ws_frame_opcode opcode,
                                                 size_t max_fragment_size);
    static bool reassemble_message(const std::vector<ws_frame>& fragments,
                                  std::vector<uint8_t>& message);
    
    // Frame conversion
    static mapping_t* frame_to_mapping(const ws_frame& frame);
    static bool mapping_to_frame(const mapping_t* mapping, ws_frame& frame);
    
    // Debug and display
    static std::string frame_to_string(const ws_frame& frame);
    static void dump_frame(const ws_frame& frame, std::string& output);
    static std::string opcode_to_string(ws_frame_opcode opcode);
    
    // Statistical analysis
    static size_t calculate_frame_overhead(uint64_t payload_size, bool masked);
    static double calculate_overhead_ratio(uint64_t payload_size, bool masked);
};

/*
 * WebSocket Frame Stream Processor
 * 
 * Handles continuous streams of WebSocket frames with fragmentation support
 */
class WebSocketFrameStream {
private:
    WebSocketFrameParser parser_;
    std::vector<ws_frame> pending_fragments_;
    ws_frame_opcode expected_continuation_opcode_;
    bool in_fragmented_message_;
    
    // Stream configuration
    size_t max_message_size_;
    bool auto_reassemble_;
    
public:
    WebSocketFrameStream(size_t max_message_size = 1024 * 1024,
                        bool auto_reassemble = true);
    ~WebSocketFrameStream();
    
    // Stream processing
    std::vector<ws_frame> process_data(const uint8_t* data, size_t len);
    std::vector<ws_frame> process_data(const std::vector<uint8_t>& data);
    
    // Fragmentation handling
    bool has_pending_fragments() const { return !pending_fragments_.empty(); }
    bool is_in_fragmented_message() const { return in_fragmented_message_; }
    void clear_fragments();
    
    // Configuration
    void set_max_message_size(size_t max_size) { max_message_size_ = max_size; }
    void set_auto_reassemble(bool auto_reassemble) { auto_reassemble_ = auto_reassemble; }
    
    // Statistics
    size_t get_fragment_count() const { return pending_fragments_.size(); }
    size_t get_fragmented_message_size() const;
    
private:
    bool add_fragment(const ws_frame& frame);
    ws_frame reassemble_fragments();
    bool validate_fragment_sequence(const ws_frame& frame);
};

/*
 * Global Frame Processing Functions
 */

// Parse single frame from raw data
ws_frame_parse_result parse_websocket_frame(const uint8_t* data, size_t len,
                                           ws_frame& frame, size_t& bytes_consumed);

// Build frame to raw data
std::vector<uint8_t> build_websocket_frame(ws_frame_opcode opcode,
                                         const std::vector<uint8_t>& payload,
                                         bool fin = true, bool mask = false);

// Frame validation
bool validate_websocket_frame(const ws_frame& frame);

// Utility conversions
std::vector<uint8_t> string_to_frame_payload(const std::string& str);
std::string frame_payload_to_string(const std::vector<uint8_t>& payload);

#endif  // PACKAGES_WEBSOCKET_WS_FRAME_H_