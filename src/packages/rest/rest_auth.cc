/**
 * rest_auth.cc - REST authentication implementation
 *
 * JWT and authentication functionality
 */

#include "rest.h"
#include <nlohmann/json.hpp>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cstring>
#include <sstream>
#include <iomanip>

/**
 * Base64 encode implementation
 */
std::string rest_base64_encode(const std::string &input) {
    static const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    int val = 0, valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (encoded.size() % 4) {
        encoded.push_back('=');
    }
    
    return encoded;
}

/**
 * Base64 decode implementation
 */
std::string rest_base64_decode(const std::string &input) {
    static const int base64_decode_table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    std::string decoded;
    int val = 0, valb = -8;
    
    for (unsigned char c : input) {
        if (base64_decode_table[c] == -1) break;
        val = (val << 6) + base64_decode_table[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decoded;
}

/**
 * HMAC-SHA256 implementation
 */
std::string rest_hmac_sha256(const std::string &data, const std::string &key) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    
    HMAC(EVP_sha256(), 
         key.c_str(), key.length(),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         digest, &digest_len);
    
    return std::string(reinterpret_cast<char*>(digest), digest_len);
}

/**
 * Create JWT token implementation
 */
char *rest_jwt_create_impl(mapping_t *payload, const char *secret) {
    if (!payload || !secret) {
        return nullptr;
    }
    
    try {
        // Create JWT header
        nlohmann::json header;
        header["typ"] = "JWT";
        header["alg"] = "HS256";
        
        std::string header_str = header.dump();
        std::string header_b64 = rest_base64_encode(header_str);
        
        // Remove base64 padding
        while (!header_b64.empty() && header_b64.back() == '=') {
            header_b64.pop_back();
        }
        
        // Create JWT payload
        nlohmann::json payload_json;
        
        // Convert mapping to JSON
        for (int i = 0; i < payload->table_size; i++) {
            mapping_node_t *node = payload->table[i];
            while (node) {
                if (node->values[0].type == T_STRING) {
                    std::string key = node->values[0].u.string;
                    
                    if (node->values[1].type == T_STRING) {
                        payload_json[key] = node->values[1].u.string;
                    } else if (node->values[1].type == T_NUMBER) {
                        payload_json[key] = node->values[1].u.number;
                    } else if (node->values[1].type == T_REAL) {
                        payload_json[key] = node->values[1].u.real;
                    }
                }
                node = node->next;
            }
        }
        
        // Add standard claims if not present
        if (payload_json.find("iat") == payload_json.end()) {
            payload_json["iat"] = time(nullptr);
        }
        
        std::string payload_str = payload_json.dump();
        std::string payload_b64 = rest_base64_encode(payload_str);
        
        // Remove base64 padding
        while (!payload_b64.empty() && payload_b64.back() == '=') {
            payload_b64.pop_back();
        }
        
        // Create signature
        std::string signing_input = header_b64 + "." + payload_b64;
        std::string signature_raw = rest_hmac_sha256(signing_input, secret);
        std::string signature_b64 = rest_base64_encode(signature_raw);
        
        // Remove base64 padding
        while (!signature_b64.empty() && signature_b64.back() == '=') {
            signature_b64.pop_back();
        }
        
        // Combine JWT
        std::string jwt = header_b64 + "." + payload_b64 + "." + signature_b64;
        
        // Return allocated string
        char *result = static_cast<char*>(malloc(jwt.length() + 1));
        strcpy(result, jwt.c_str());
        return result;
        
    } catch (const std::exception &e) {
        return nullptr;
    }
}

/**
 * Verify JWT token implementation
 */
mapping_t *rest_jwt_verify_impl(const char *token, const char *secret) {
    if (!token || !secret) {
        return nullptr;
    }
    
    try {
        std::string token_str = token;
        
        // Split token into parts
        size_t first_dot = token_str.find('.');
        size_t second_dot = token_str.find('.', first_dot + 1);
        
        if (first_dot == std::string::npos || second_dot == std::string::npos) {
            return nullptr; // Invalid token format
        }
        
        std::string header_b64 = token_str.substr(0, first_dot);
        std::string payload_b64 = token_str.substr(first_dot + 1, second_dot - first_dot - 1);
        std::string signature_b64 = token_str.substr(second_dot + 1);
        
        // Add padding back to base64 strings
        while (header_b64.length() % 4) header_b64 += "=";
        while (payload_b64.length() % 4) payload_b64 += "=";
        while (signature_b64.length() % 4) signature_b64 += "=";
        
        // Verify signature
        std::string signing_input = token_str.substr(0, second_dot);
        std::string expected_signature_raw = rest_hmac_sha256(signing_input, secret);
        std::string expected_signature_b64 = rest_base64_encode(expected_signature_raw);
        
        // Remove padding for comparison
        while (!expected_signature_b64.empty() && expected_signature_b64.back() == '=') {
            expected_signature_b64.pop_back();
        }
        
        std::string received_signature_b64 = signature_b64;
        while (!received_signature_b64.empty() && received_signature_b64.back() == '=') {
            received_signature_b64.pop_back();
        }
        
        if (expected_signature_b64 != received_signature_b64) {
            return nullptr; // Signature verification failed
        }
        
        // Decode and parse payload
        std::string payload_str = rest_base64_decode(payload_b64);
        nlohmann::json payload_json = nlohmann::json::parse(payload_str);
        
        // Convert JSON to mapping
        mapping_t *result = allocate_mapping(payload_json.size());
        
        for (auto it = payload_json.begin(); it != payload_json.end(); ++it) {
            svalue_t key, value;
            key.type = T_STRING;
            key.u.string = make_shared_string(it.key().c_str());
            
            if (it.value().is_string()) {
                value.type = T_STRING;
                value.u.string = make_shared_string(it.value().get<std::string>().c_str());
            } else if (it.value().is_number_integer()) {
                value.type = T_NUMBER;
                value.u.number = it.value().get<int>();
            } else if (it.value().is_number_float()) {
                value.type = T_REAL;
                value.u.real = it.value().get<double>();
            } else {
                value.type = T_NUMBER;
                value.u.number = 0; // Default for unsupported types
            }
            
            svalue_t *entry = find_for_insert(result, &key, 0); *entry = value;
            free_string(key.u.string);
            if (value.type == T_STRING) {
                free_string(value.u.string);
            }
        }
        
        // Check expiration if present
        svalue_t *exp_val = find_string_in_mapping(result, "exp");
        if (exp_val && exp_val->type == T_NUMBER) {
            time_t now = time(nullptr);
            if (exp_val->u.number < now) {
                free_mapping(result);
                return nullptr; // Token expired
            }
        }
        
        return result;
        
    } catch (const std::exception &e) {
        return nullptr;
    }
}