/*
 * WebSocket Frame Processing Implementation
 * 
 * Low-level WebSocket frame parsing, generation, and manipulation
 * according to RFC 6455 specification.
 */

#include "packages/websocket/ws_frame.h"
#include "packages/websocket/websocket.h"
#include "base/internal/log.h"
#include "vm/internal/simulate.h"

#include <cstring>
#include <algorithm>
#include <random>
#include <chrono>

/*
 * WebSocket Frame Parser Implementation
 */

WebSocketFrameParser::WebSocketFrameParser(size_t max_frame_size, bool require_masking, 
                                         bool validate_utf8)
    : state_(STATE_HEADER), max_frame_size_(max_frame_size),
      require_masking_(require_masking), validate_utf8_(validate_utf8),
      bytes_needed_(2), bytes_parsed_(0) {
    buffer_.reserve(WS_FRAME_HEADER_MAX_SIZE);
}

WebSocketFrameParser::~WebSocketFrameParser() = default;

ws_frame_parse_result WebSocketFrameParser::parse(const uint8_t* data, size_t len, 
                                                 size_t& bytes_consumed) {
    bytes_consumed = 0;
    
    while (len > 0 && state_ != STATE_COMPLETE) {
        size_t consumed = 0;
        ws_frame_parse_result result = WS_FRAME_PARSE_SUCCESS;
        
        switch (state_) {
            case STATE_HEADER:
                result = parse_header(data, len, consumed);
                break;
            case STATE_EXTENDED_LENGTH:
                result = parse_extended_length(data, len, consumed);
                break;
            case STATE_MASK:
                result = parse_mask(data, len, consumed);
                break;
            case STATE_PAYLOAD:
                result = parse_payload(data, len, consumed);
                break;
            case STATE_COMPLETE:
                return WS_FRAME_PARSE_SUCCESS;
        }
        
        if (result != WS_FRAME_PARSE_SUCCESS) {
            return result;
        }
        
        data += consumed;
        len -= consumed;
        bytes_consumed += consumed;
    }
    
    return state_ == STATE_COMPLETE ? WS_FRAME_PARSE_SUCCESS : WS_FRAME_PARSE_INCOMPLETE;
}

ws_frame_parse_result WebSocketFrameParser::parse(const std::vector<uint8_t>& data, 
                                                 size_t& bytes_consumed) {
    return parse(data.data(), data.size(), bytes_consumed);
}

ws_frame WebSocketFrameParser::take_frame() {
    ws_frame frame = current_frame_;
    reset();
    return frame;
}

void WebSocketFrameParser::reset() {
    state_ = STATE_HEADER;
    current_frame_ = ws_frame();
    buffer_.clear();
    bytes_needed_ = 2;
    bytes_parsed_ = 0;
}

ws_frame_parse_result WebSocketFrameParser::parse_header(const uint8_t* data, size_t len, 
                                                        size_t& consumed) {
    consumed = 0;
    
    // Need at least 2 bytes for basic header
    while (buffer_.size() < 2 && len > 0) {
        buffer_.push_back(*data++);
        len--;
        consumed++;
    }
    
    if (buffer_.size() < 2) {
        return WS_FRAME_PARSE_INCOMPLETE;
    }
    
    // Parse first byte
    uint8_t byte1 = buffer_[0];
    current_frame_.fin = (byte1 & WS_FRAME_FLAG_FIN) != 0;
    current_frame_.rsv1 = (byte1 & WS_FRAME_FLAG_RSV1) != 0;
    current_frame_.rsv2 = (byte1 & WS_FRAME_FLAG_RSV2) != 0;
    current_frame_.rsv3 = (byte1 & WS_FRAME_FLAG_RSV3) != 0;
    current_frame_.opcode = static_cast<ws_frame_opcode>(byte1 & WS_FRAME_FLAG_OPCODE);
    
    // Validate opcode
    if (!is_valid_opcode(static_cast<uint8_t>(current_frame_.opcode))) {
        return WS_FRAME_PARSE_INVALID_OPCODE;
    }
    
    // Parse second byte
    uint8_t byte2 = buffer_[1];
    current_frame_.masked = (byte2 & WS_FRAME_FLAG_MASK) != 0;
    uint8_t payload_len = byte2 & WS_FRAME_FLAG_LEN;
    
    // Check masking requirement
    if (require_masking_ && !current_frame_.masked) {
        return WS_FRAME_PARSE_INVALID_MASK;
    }
    
    // Determine payload length handling
    if (payload_len < WS_FRAME_LEN_16_BIT) {
        current_frame_.payload_length = payload_len;
        state_ = current_frame_.masked ? STATE_MASK : STATE_PAYLOAD;
    } else if (payload_len == WS_FRAME_LEN_16_BIT) {
        bytes_needed_ = 2;
        state_ = STATE_EXTENDED_LENGTH;
    } else if (payload_len == WS_FRAME_LEN_64_BIT) {
        bytes_needed_ = 8;
        state_ = STATE_EXTENDED_LENGTH;
    }
    
    // Validate control frame constraints
    if (is_control_frame(static_cast<uint8_t>(current_frame_.opcode))) {
        if (!current_frame_.fin) {
            return WS_FRAME_PARSE_PROTOCOL_ERROR;
        }
        if (current_frame_.payload_length > 125) {
            return WS_FRAME_PARSE_PROTOCOL_ERROR;
        }
    }
    
    return WS_FRAME_PARSE_SUCCESS;
}

ws_frame_parse_result WebSocketFrameParser::parse_extended_length(const uint8_t* data, 
                                                                 size_t len, size_t& consumed) {
    consumed = 0;
    
    // Collect needed bytes for extended length
    while (buffer_.size() < (2 + bytes_needed_) && len > 0) {
        buffer_.push_back(*data++);
        len--;
        consumed++;
    }
    
    if (buffer_.size() < (2 + bytes_needed_)) {
        return WS_FRAME_PARSE_INCOMPLETE;
    }
    
    // Parse extended length
    if (bytes_needed_ == 2) {
        current_frame_.payload_length = read_big_endian_16(&buffer_[2]);
        if (current_frame_.payload_length < 126) {
            return WS_FRAME_PARSE_PROTOCOL_ERROR; // Must use minimal encoding
        }
    } else if (bytes_needed_ == 8) {
        current_frame_.payload_length = read_big_endian_64(&buffer_[2]);
        if (current_frame_.payload_length < 65536) {
            return WS_FRAME_PARSE_PROTOCOL_ERROR; // Must use minimal encoding
        }
        if ((current_frame_.payload_length >> 63) != 0) {
            return WS_FRAME_PARSE_INVALID_LENGTH; // MSB must be 0
        }
    }
    
    // Check frame size limit
    if (current_frame_.payload_length > max_frame_size_) {
        return WS_FRAME_PARSE_TOO_LARGE;
    }
    
    state_ = current_frame_.masked ? STATE_MASK : STATE_PAYLOAD;
    return WS_FRAME_PARSE_SUCCESS;
}

ws_frame_parse_result WebSocketFrameParser::parse_mask(const uint8_t* data, size_t len, 
                                                      size_t& consumed) {
    consumed = 0;
    
    size_t header_size = 2 + bytes_needed_;
    
    // Collect mask bytes
    while (buffer_.size() < (header_size + 4) && len > 0) {
        buffer_.push_back(*data++);
        len--;
        consumed++;
    }
    
    if (buffer_.size() < (header_size + 4)) {
        return WS_FRAME_PARSE_INCOMPLETE;
    }
    
    // Extract mask key
    current_frame_.mask_key = 0;
    for (int i = 0; i < 4; i++) {
        current_frame_.mask_key |= static_cast<uint32_t>(buffer_[header_size + i]) << (24 - i * 8);
    }
    
    state_ = STATE_PAYLOAD;
    return WS_FRAME_PARSE_SUCCESS;
}

ws_frame_parse_result WebSocketFrameParser::parse_payload(const uint8_t* data, size_t len, 
                                                         size_t& consumed) {
    consumed = 0;
    
    // Reserve space for payload
    if (current_frame_.payload.empty() && current_frame_.payload_length > 0) {
        current_frame_.payload.reserve(current_frame_.payload_length);
    }
    
    // Collect payload bytes
    size_t remaining = current_frame_.payload_length - current_frame_.payload.size();
    size_t to_copy = std::min(remaining, len);
    
    if (to_copy > 0) {
        current_frame_.payload.insert(current_frame_.payload.end(), data, data + to_copy);
        consumed = to_copy;
    }
    
    // Check if payload is complete
    if (current_frame_.payload.size() == current_frame_.payload_length) {
        // Unmask payload if needed
        if (current_frame_.masked) {
            unmask_payload();
        }
        
        // Validate UTF-8 for text frames
        if (validate_utf8_ && current_frame_.opcode == WS_OPCODE_TEXT) {
            if (!WebSocketFrameUtils::is_valid_utf8(current_frame_.payload)) {
                return WS_FRAME_PARSE_INVALID_LENGTH; // Invalid UTF-8
            }
        }
        
        state_ = STATE_COMPLETE;
    }
    
    return WS_FRAME_PARSE_SUCCESS;
}

bool WebSocketFrameParser::is_valid_opcode(uint8_t opcode) const {
    switch (opcode) {
        case WS_OPCODE_CONTINUATION:
        case WS_OPCODE_TEXT:
        case WS_OPCODE_BINARY:
        case WS_OPCODE_CLOSE:
        case WS_OPCODE_PING:
        case WS_OPCODE_PONG:
            return true;
        default:
            return false;
    }
}

bool WebSocketFrameParser::is_control_frame(uint8_t opcode) const {
    return opcode >= 0x8;
}

bool WebSocketFrameParser::validate_control_frame() const {
    return current_frame_.fin && current_frame_.payload_length <= 125;
}

bool WebSocketFrameParser::validate_continuation_frame() const {
    return current_frame_.opcode == WS_OPCODE_CONTINUATION;
}

uint64_t WebSocketFrameParser::read_big_endian_64(const uint8_t* data) const {
    uint64_t value = 0;
    for (int i = 0; i < 8; i++) {
        value = (value << 8) | data[i];
    }
    return value;
}

uint16_t WebSocketFrameParser::read_big_endian_16(const uint8_t* data) const {
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

void WebSocketFrameParser::unmask_payload() {
    if (current_frame_.payload.empty()) {
        return;
    }
    
    uint8_t mask_bytes[4];
    mask_bytes[0] = (current_frame_.mask_key >> 24) & 0xFF;
    mask_bytes[1] = (current_frame_.mask_key >> 16) & 0xFF;
    mask_bytes[2] = (current_frame_.mask_key >> 8) & 0xFF;
    mask_bytes[3] = current_frame_.mask_key & 0xFF;
    
    for (size_t i = 0; i < current_frame_.payload.size(); i++) {
        current_frame_.payload[i] ^= mask_bytes[i % 4];
    }
}

/*
 * WebSocket Frame Builder Implementation
 */

WebSocketFrameBuilder::WebSocketFrameBuilder(bool auto_mask, size_t max_frame_size)
    : auto_mask_(auto_mask), mask_key_(0), max_frame_size_(max_frame_size) {
    if (auto_mask_) {
        mask_key_ = generate_mask_key();
    }
}

WebSocketFrameBuilder::~WebSocketFrameBuilder() = default;

std::vector<uint8_t> WebSocketFrameBuilder::build_text_frame(const std::string& text, 
                                                           bool fin, bool mask, 
                                                           uint32_t mask_key) {
    std::vector<uint8_t> payload(text.begin(), text.end());
    return build_frame(WS_OPCODE_TEXT, payload, fin, mask || auto_mask_, 
                      mask ? mask_key : mask_key_);
}

std::vector<uint8_t> WebSocketFrameBuilder::build_binary_frame(const std::vector<uint8_t>& data, 
                                                             bool fin, bool mask, 
                                                             uint32_t mask_key) {
    return build_frame(WS_OPCODE_BINARY, data, fin, mask || auto_mask_, 
                      mask ? mask_key : mask_key_);
}

std::vector<uint8_t> WebSocketFrameBuilder::build_close_frame(uint16_t close_code,
                                                            const std::string& reason,
                                                            bool mask, uint32_t mask_key) {
    std::vector<uint8_t> payload;
    
    if (close_code != 0) {
        payload.resize(2);
        payload[0] = (close_code >> 8) & 0xFF;
        payload[1] = close_code & 0xFF;
        
        if (!reason.empty()) {
            size_t reason_len = std::min(reason.length(), size_t(123)); // Max 125 - 2 for code
            payload.insert(payload.end(), reason.begin(), reason.begin() + reason_len);
        }
    }
    
    return build_frame(WS_OPCODE_CLOSE, payload, true, mask || auto_mask_, 
                      mask ? mask_key : mask_key_);
}

std::vector<uint8_t> WebSocketFrameBuilder::build_ping_frame(const std::string& payload,
                                                           bool mask, uint32_t mask_key) {
    if (payload.length() > 125) {
        return std::vector<uint8_t>(); // Invalid ping payload
    }
    
    std::vector<uint8_t> ping_payload(payload.begin(), payload.end());
    return build_frame(WS_OPCODE_PING, ping_payload, true, mask || auto_mask_, 
                      mask ? mask_key : mask_key_);
}

std::vector<uint8_t> WebSocketFrameBuilder::build_pong_frame(const std::string& payload,
                                                           bool mask, uint32_t mask_key) {
    if (payload.length() > 125) {
        return std::vector<uint8_t>(); // Invalid pong payload
    }
    
    std::vector<uint8_t> pong_payload(payload.begin(), payload.end());
    return build_frame(WS_OPCODE_PONG, pong_payload, true, mask || auto_mask_, 
                      mask ? mask_key : mask_key_);
}

std::vector<uint8_t> WebSocketFrameBuilder::build_continuation_frame(const std::vector<uint8_t>& data, 
                                                                   bool fin, bool mask,
                                                                   uint32_t mask_key) {
    return build_frame(WS_OPCODE_CONTINUATION, data, fin, mask || auto_mask_, 
                      mask ? mask_key : mask_key_);
}

std::vector<uint8_t> WebSocketFrameBuilder::build_frame(const ws_frame& frame) {
    return build_frame(frame.opcode, frame.payload, frame.fin, 
                      frame.masked, frame.mask_key);
}

std::vector<uint8_t> WebSocketFrameBuilder::build_frame(ws_frame_opcode opcode, 
                                                      const std::vector<uint8_t>& payload,
                                                      bool fin, bool mask, uint32_t mask_key) {
    if (!validate_payload_size(payload.size())) {
        return std::vector<uint8_t>(); // Payload too large
    }
    
    if (!validate_control_frame_payload(opcode, payload.size())) {
        return std::vector<uint8_t>(); // Invalid control frame payload
    }
    
    // Build header
    std::vector<uint8_t> frame = build_header(opcode, payload.size(), fin, mask, mask_key);
    
    // Add payload
    if (!payload.empty()) {
        if (mask) {
            std::vector<uint8_t> masked_payload = mask_payload(payload, mask_key);
            frame.insert(frame.end(), masked_payload.begin(), masked_payload.end());
        } else {
            frame.insert(frame.end(), payload.begin(), payload.end());
        }
    }
    
    return frame;
}

uint32_t WebSocketFrameBuilder::generate_mask_key() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;
    
    return dis(gen);
}

std::vector<uint8_t> WebSocketFrameBuilder::mask_payload(const std::vector<uint8_t>& payload, 
                                                       uint32_t mask_key) {
    std::vector<uint8_t> masked = payload;
    mask_payload_inplace(masked, mask_key);
    return masked;
}

void WebSocketFrameBuilder::mask_payload_inplace(std::vector<uint8_t>& payload, 
                                                uint32_t mask_key) {
    uint8_t mask_bytes[4];
    mask_bytes[0] = (mask_key >> 24) & 0xFF;
    mask_bytes[1] = (mask_key >> 16) & 0xFF;
    mask_bytes[2] = (mask_key >> 8) & 0xFF;
    mask_bytes[3] = mask_key & 0xFF;
    
    for (size_t i = 0; i < payload.size(); i++) {
        payload[i] ^= mask_bytes[i % 4];
    }
}

std::vector<uint8_t> WebSocketFrameBuilder::build_header(ws_frame_opcode opcode, 
                                                        uint64_t payload_len,
                                                        bool fin, bool mask, 
                                                        uint32_t mask_key) {
    std::vector<uint8_t> header;
    
    // First byte: FIN + RSV + OPCODE
    uint8_t byte1 = static_cast<uint8_t>(opcode);
    if (fin) {
        byte1 |= WS_FRAME_FLAG_FIN;
    }
    header.push_back(byte1);
    
    // Second byte: MASK + length
    uint8_t byte2 = 0;
    if (mask) {
        byte2 |= WS_FRAME_FLAG_MASK;
    }
    
    if (payload_len < 126) {
        byte2 |= static_cast<uint8_t>(payload_len);
        header.push_back(byte2);
    } else if (payload_len <= 65535) {
        byte2 |= 126;
        header.push_back(byte2);
        write_big_endian_16(header, static_cast<uint16_t>(payload_len));
    } else {
        byte2 |= 127;
        header.push_back(byte2);
        write_big_endian_64(header, payload_len);
    }
    
    // Add mask if needed
    if (mask) {
        header.push_back((mask_key >> 24) & 0xFF);
        header.push_back((mask_key >> 16) & 0xFF);
        header.push_back((mask_key >> 8) & 0xFF);
        header.push_back(mask_key & 0xFF);
    }
    
    return header;
}

void WebSocketFrameBuilder::write_big_endian_16(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

void WebSocketFrameBuilder::write_big_endian_64(std::vector<uint8_t>& buffer, uint64_t value) {
    for (int i = 7; i >= 0; i--) {
        buffer.push_back((value >> (i * 8)) & 0xFF);
    }
}

bool WebSocketFrameBuilder::validate_payload_size(uint64_t size) const {
    return size <= max_frame_size_;
}

bool WebSocketFrameBuilder::validate_control_frame_payload(ws_frame_opcode opcode, 
                                                          uint64_t size) const {
    if (opcode >= WS_OPCODE_CLOSE) { // Control frames
        return size <= 125;
    }
    return true;
}

/*
 * WebSocket Frame Utilities Implementation
 */

ws_frame_validation_result WebSocketFrameUtils::validate_frame(const ws_frame& frame) {
    // Validate opcode
    if (!is_valid_opcode(frame.opcode)) {
        return WS_FRAME_INVALID_OPCODE;
    }
    
    // Validate RSV bits (must be 0 unless extension defines meaning)
    if (frame.rsv1 || frame.rsv2 || frame.rsv3) {
        return WS_FRAME_INVALID_RSV;
    }
    
    // Validate control frames
    if (is_control_frame(frame)) {
        if (!frame.fin) {
            return WS_FRAME_INVALID_CONTROL_FRAME;
        }
        if (frame.payload_length > 125) {
            return WS_FRAME_INVALID_CONTROL_FRAME;
        }
    }
    
    // Validate UTF-8 for text frames
    if (frame.opcode == WS_OPCODE_TEXT && !is_valid_utf8(frame.payload)) {
        return WS_FRAME_INVALID_UTF8;
    }
    
    return WS_FRAME_VALID;
}

bool WebSocketFrameUtils::is_valid_opcode(ws_frame_opcode opcode) {
    switch (opcode) {
        case WS_OPCODE_CONTINUATION:
        case WS_OPCODE_TEXT:
        case WS_OPCODE_BINARY:
        case WS_OPCODE_CLOSE:
        case WS_OPCODE_PING:
        case WS_OPCODE_PONG:
            return true;
        default:
            return false;
    }
}

bool WebSocketFrameUtils::is_control_opcode(ws_frame_opcode opcode) {
    return static_cast<uint8_t>(opcode) >= 0x8;
}

bool WebSocketFrameUtils::is_data_opcode(ws_frame_opcode opcode) {
    return static_cast<uint8_t>(opcode) < 0x8;
}

bool WebSocketFrameUtils::is_control_frame(const ws_frame& frame) {
    return is_control_opcode(frame.opcode);
}

bool WebSocketFrameUtils::is_data_frame(const ws_frame& frame) {
    return is_data_opcode(frame.opcode);
}

bool WebSocketFrameUtils::is_continuation_frame(const ws_frame& frame) {
    return frame.opcode == WS_OPCODE_CONTINUATION;
}

bool WebSocketFrameUtils::is_final_frame(const ws_frame& frame) {
    return frame.fin;
}

std::string WebSocketFrameUtils::extract_text_payload(const ws_frame& frame) {
    if (frame.opcode != WS_OPCODE_TEXT) {
        return "";
    }
    return std::string(frame.payload.begin(), frame.payload.end());
}

std::vector<uint8_t> WebSocketFrameUtils::extract_binary_payload(const ws_frame& frame) {
    if (frame.opcode != WS_OPCODE_BINARY) {
        return std::vector<uint8_t>();
    }
    return frame.payload;
}

bool WebSocketFrameUtils::is_valid_utf8(const std::vector<uint8_t>& data) {
    // Basic UTF-8 validation
    for (size_t i = 0; i < data.size(); ) {
        uint8_t byte = data[i];
        
        if (byte < 0x80) {
            // ASCII character
            i++;
        } else if ((byte >> 5) == 0x06) {
            // 2-byte sequence
            if (i + 1 >= data.size() || (data[i + 1] >> 6) != 0x02) {
                return false;
            }
            i += 2;
        } else if ((byte >> 4) == 0x0E) {
            // 3-byte sequence
            if (i + 2 >= data.size() || 
                (data[i + 1] >> 6) != 0x02 || 
                (data[i + 2] >> 6) != 0x02) {
                return false;
            }
            i += 3;
        } else if ((byte >> 3) == 0x1E) {
            // 4-byte sequence
            if (i + 3 >= data.size() || 
                (data[i + 1] >> 6) != 0x02 || 
                (data[i + 2] >> 6) != 0x02 || 
                (data[i + 3] >> 6) != 0x02) {
                return false;
            }
            i += 4;
        } else {
            return false;
        }
    }
    
    return true;
}

bool WebSocketFrameUtils::parse_close_payload(const std::vector<uint8_t>& payload,
                                             uint16_t& close_code, std::string& reason) {
    if (payload.empty()) {
        close_code = WS_CLOSE_NO_STATUS;
        reason.clear();
        return true;
    }
    
    if (payload.size() < 2) {
        return false; // Invalid close payload
    }
    
    close_code = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
    
    if (payload.size() > 2) {
        reason = std::string(payload.begin() + 2, payload.end());
        
        // Validate UTF-8 in reason
        std::vector<uint8_t> reason_bytes(payload.begin() + 2, payload.end());
        if (!is_valid_utf8(reason_bytes)) {
            return false;
        }
    } else {
        reason.clear();
    }
    
    return true;
}

std::vector<uint8_t> WebSocketFrameUtils::build_close_payload(uint16_t close_code,
                                                            const std::string& reason) {
    std::vector<uint8_t> payload;
    
    if (close_code != 0) {
        payload.push_back((close_code >> 8) & 0xFF);
        payload.push_back(close_code & 0xFF);
        
        if (!reason.empty()) {
            size_t reason_len = std::min(reason.length(), size_t(123));
            payload.insert(payload.end(), reason.begin(), reason.begin() + reason_len);
        }
    }
    
    return payload;
}

std::vector<ws_frame> WebSocketFrameUtils::fragment_message(const std::vector<uint8_t>& message,
                                                          ws_frame_opcode opcode,
                                                          size_t max_fragment_size) {
    std::vector<ws_frame> fragments;
    
    if (message.size() <= max_fragment_size) {
        // No fragmentation needed
        ws_frame frame;
        frame.fin = true;
        frame.opcode = opcode;
        frame.payload = message;
        frame.payload_length = message.size();
        fragments.push_back(frame);
        return fragments;
    }
    
    // Create fragments
    size_t offset = 0;
    bool first_fragment = true;
    
    while (offset < message.size()) {
        size_t fragment_size = std::min(max_fragment_size, message.size() - offset);
        bool is_final = (offset + fragment_size >= message.size());
        
        ws_frame frame;
        frame.fin = is_final;
        frame.opcode = first_fragment ? opcode : WS_OPCODE_CONTINUATION;
        frame.payload = std::vector<uint8_t>(message.begin() + offset, 
                                           message.begin() + offset + fragment_size);
        frame.payload_length = fragment_size;
        
        fragments.push_back(frame);
        
        offset += fragment_size;
        first_fragment = false;
    }
    
    return fragments;
}

bool WebSocketFrameUtils::reassemble_message(const std::vector<ws_frame>& fragments,
                                            std::vector<uint8_t>& message) {
    if (fragments.empty()) {
        return false;
    }
    
    message.clear();
    
    // Validate fragment sequence
    for (size_t i = 0; i < fragments.size(); i++) {
        const ws_frame& frame = fragments[i];
        
        if (i == 0) {
            // First fragment must not be continuation
            if (frame.opcode == WS_OPCODE_CONTINUATION) {
                return false;
            }
        } else {
            // Subsequent fragments must be continuation
            if (frame.opcode != WS_OPCODE_CONTINUATION) {
                return false;
            }
        }
        
        // Only last fragment should have FIN set
        if (frame.fin && i != fragments.size() - 1) {
            return false;
        }
        
        // Non-final fragments should not have FIN set
        if (!frame.fin && i == fragments.size() - 1) {
            return false;
        }
        
        // Append payload
        message.insert(message.end(), frame.payload.begin(), frame.payload.end());
    }
    
    return true;
}

mapping_t* WebSocketFrameUtils::frame_to_mapping(const ws_frame& frame) {
    mapping_t* mapping = allocate_mapping(8);
    
    add_mapping_pair(mapping, "fin", frame.fin ? 1 : 0);
    add_mapping_pair(mapping, "rsv1", frame.rsv1 ? 1 : 0);
    add_mapping_pair(mapping, "rsv2", frame.rsv2 ? 1 : 0);
    add_mapping_pair(mapping, "rsv3", frame.rsv3 ? 1 : 0);
    add_mapping_pair(mapping, "opcode", static_cast<int>(frame.opcode));
    add_mapping_pair(mapping, "masked", frame.masked ? 1 : 0);
    add_mapping_pair(mapping, "payload_length", static_cast<int>(frame.payload_length));
    add_mapping_pair(mapping, "mask_key", static_cast<int>(frame.mask_key));
    
    // Convert payload to buffer or string depending on opcode
    if (frame.opcode == WS_OPCODE_TEXT) {
        std::string text(frame.payload.begin(), frame.payload.end());
        add_mapping_string(mapping, "payload", text.c_str());
    } else {
        buffer_t* buffer = allocate_buffer(frame.payload.size());
        memcpy(buffer->item, frame.payload.data(), frame.payload.size());
        add_mapping_buffer(mapping, "payload", buffer);
    }
    
    return mapping;
}

bool WebSocketFrameUtils::mapping_to_frame(const mapping_t* mapping, ws_frame& frame) {
    if (!mapping) {
        return false;
    }
    
    svalue_t* value;
    
    if ((value = find_mapping_value(mapping, "fin")) && value->type == T_NUMBER) {
        frame.fin = value->u.number != 0;
    }
    
    if ((value = find_mapping_value(mapping, "opcode")) && value->type == T_NUMBER) {
        frame.opcode = static_cast<ws_frame_opcode>(value->u.number);
    }
    
    if ((value = find_mapping_value(mapping, "masked")) && value->type == T_NUMBER) {
        frame.masked = value->u.number != 0;
    }
    
    if ((value = find_mapping_value(mapping, "mask_key")) && value->type == T_NUMBER) {
        frame.mask_key = static_cast<uint32_t>(value->u.number);
    }
    
    // Extract payload
    if ((value = find_mapping_value(mapping, "payload"))) {
        if (value->type == T_STRING) {
            std::string text = value->u.string;
            frame.payload = std::vector<uint8_t>(text.begin(), text.end());
        } else if (value->type == T_BUFFER) {
            buffer_t* buffer = value->u.buf;
            frame.payload = std::vector<uint8_t>(buffer->item, buffer->item + buffer->size);
        }
        frame.payload_length = frame.payload.size();
    }
    
    return true;
}

std::string WebSocketFrameUtils::frame_to_string(const ws_frame& frame) {
    std::string result = "WebSocket Frame:\n";
    result += "  FIN: " + std::to_string(frame.fin) + "\n";
    result += "  Opcode: " + opcode_to_string(frame.opcode) + " (" + 
              std::to_string(static_cast<int>(frame.opcode)) + ")\n";
    result += "  Masked: " + std::to_string(frame.masked) + "\n";
    result += "  Payload Length: " + std::to_string(frame.payload_length) + "\n";
    
    if (frame.masked) {
        result += "  Mask Key: 0x" + std::to_string(frame.mask_key) + "\n";
    }
    
    if (frame.opcode == WS_OPCODE_TEXT) {
        std::string text(frame.payload.begin(), frame.payload.end());
        result += "  Text Payload: " + text + "\n";
    } else if (!frame.payload.empty()) {
        result += "  Binary Payload: " + std::to_string(frame.payload.size()) + " bytes\n";
    }
    
    return result;
}

void WebSocketFrameUtils::dump_frame(const ws_frame& frame, std::string& output) {
    output = frame_to_string(frame);
}

std::string WebSocketFrameUtils::opcode_to_string(ws_frame_opcode opcode) {
    switch (opcode) {
        case WS_OPCODE_CONTINUATION: return "CONTINUATION";
        case WS_OPCODE_TEXT: return "TEXT";
        case WS_OPCODE_BINARY: return "BINARY";
        case WS_OPCODE_CLOSE: return "CLOSE";
        case WS_OPCODE_PING: return "PING";
        case WS_OPCODE_PONG: return "PONG";
        default: return "UNKNOWN";
    }
}

size_t WebSocketFrameUtils::calculate_frame_overhead(uint64_t payload_size, bool masked) {
    size_t overhead = 2; // Basic header
    
    if (payload_size >= 126) {
        overhead += (payload_size >= 65536) ? 8 : 2; // Extended length
    }
    
    if (masked) {
        overhead += 4; // Mask key
    }
    
    return overhead;
}

double WebSocketFrameUtils::calculate_overhead_ratio(uint64_t payload_size, bool masked) {
    if (payload_size == 0) {
        return 1.0;
    }
    
    size_t overhead = calculate_frame_overhead(payload_size, masked);
    return static_cast<double>(overhead) / (payload_size + overhead);
}

/*
 * WebSocket Frame Stream Processor Implementation
 */

WebSocketFrameStream::WebSocketFrameStream(size_t max_message_size, bool auto_reassemble)
    : parser_(max_message_size), expected_continuation_opcode_(WS_OPCODE_TEXT),
      in_fragmented_message_(false), max_message_size_(max_message_size),
      auto_reassemble_(auto_reassemble) {
}

WebSocketFrameStream::~WebSocketFrameStream() = default;

std::vector<ws_frame> WebSocketFrameStream::process_data(const uint8_t* data, size_t len) {
    std::vector<ws_frame> complete_frames;
    
    size_t offset = 0;
    while (offset < len) {
        size_t bytes_consumed = 0;
        ws_frame_parse_result result = parser_.parse(data + offset, len - offset, bytes_consumed);
        
        if (result == WS_FRAME_PARSE_SUCCESS && parser_.has_complete_frame()) {
            ws_frame frame = parser_.take_frame();
            
            if (auto_reassemble_ && 
                (frame.opcode == WS_OPCODE_CONTINUATION || !frame.fin)) {
                if (add_fragment(frame)) {
                    // Fragment added, check if message is complete
                    if (frame.fin) {
                        ws_frame complete_frame = reassemble_fragments();
                        complete_frames.push_back(complete_frame);
                    }
                } else {
                    // Invalid fragment sequence
                    clear_fragments();
                }
            } else {
                complete_frames.push_back(frame);
            }
        } else if (result != WS_FRAME_PARSE_INCOMPLETE) {
            // Parse error
            break;
        }
        
        offset += bytes_consumed;
        
        if (bytes_consumed == 0) {
            break; // Avoid infinite loop
        }
    }
    
    return complete_frames;
}

std::vector<ws_frame> WebSocketFrameStream::process_data(const std::vector<uint8_t>& data) {
    return process_data(data.data(), data.size());
}

void WebSocketFrameStream::clear_fragments() {
    pending_fragments_.clear();
    in_fragmented_message_ = false;
}

size_t WebSocketFrameStream::get_fragmented_message_size() const {
    size_t total_size = 0;
    for (const auto& fragment : pending_fragments_) {
        total_size += fragment.payload_length;
    }
    return total_size;
}

bool WebSocketFrameStream::add_fragment(const ws_frame& frame) {
    if (!validate_fragment_sequence(frame)) {
        return false;
    }
    
    pending_fragments_.push_back(frame);
    
    if (pending_fragments_.size() == 1) {
        // First fragment
        expected_continuation_opcode_ = frame.opcode;
        in_fragmented_message_ = true;
    }
    
    // Check message size limit
    if (get_fragmented_message_size() > max_message_size_) {
        clear_fragments();
        return false;
    }
    
    return true;
}

ws_frame WebSocketFrameStream::reassemble_fragments() {
    ws_frame complete_frame;
    
    if (pending_fragments_.empty()) {
        return complete_frame;
    }
    
    // Set frame properties from first fragment
    complete_frame.fin = true;
    complete_frame.opcode = pending_fragments_[0].opcode;
    complete_frame.masked = false; // Reassembled frames are not masked
    
    // Combine payloads
    for (const auto& fragment : pending_fragments_) {
        complete_frame.payload.insert(complete_frame.payload.end(),
                                    fragment.payload.begin(), fragment.payload.end());
    }
    
    complete_frame.payload_length = complete_frame.payload.size();
    
    clear_fragments();
    return complete_frame;
}

bool WebSocketFrameStream::validate_fragment_sequence(const ws_frame& frame) {
    if (pending_fragments_.empty()) {
        // First fragment must not be continuation
        return frame.opcode != WS_OPCODE_CONTINUATION;
    } else {
        // Subsequent fragments must be continuation
        return frame.opcode == WS_OPCODE_CONTINUATION;
    }
}

/*
 * Global Frame Processing Functions
 */

ws_frame_parse_result parse_websocket_frame(const uint8_t* data, size_t len,
                                           ws_frame& frame, size_t& bytes_consumed) {
    WebSocketFrameParser parser;
    ws_frame_parse_result result = parser.parse(data, len, bytes_consumed);
    
    if (result == WS_FRAME_PARSE_SUCCESS && parser.has_complete_frame()) {
        frame = parser.get_frame();
    }
    
    return result;
}

std::vector<uint8_t> build_websocket_frame(ws_frame_opcode opcode,
                                         const std::vector<uint8_t>& payload,
                                         bool fin, bool mask) {
    WebSocketFrameBuilder builder(mask);
    return builder.build_frame(opcode, payload, fin, mask);
}

bool validate_websocket_frame(const ws_frame& frame) {
    return WebSocketFrameUtils::validate_frame(frame) == WS_FRAME_VALID;
}

std::vector<uint8_t> string_to_frame_payload(const std::string& str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::string frame_payload_to_string(const std::vector<uint8_t>& payload) {
    return std::string(payload.begin(), payload.end());
}