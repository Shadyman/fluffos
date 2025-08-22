#ifndef SOCKET_OPTIONS_H_
#define SOCKET_OPTIONS_H_

/*
 * Comprehensive Socket Options for Unified Socket Architecture
 * 
 * This header defines all socket options used across the FluffOS 
 * unified socket system. Options are organized by protocol/feature 
 * family with reserved ranges for future expansion.
 * 
 * Range Allocation:
 * 0-99:     Core/Legacy socket options
 * 100-119:  HTTP/HTTPS options
 * 110-119:  REST server configuration
 * 120-129:  WebSocket options
 * 130-139:  MQTT options
 * 140-149:  External process options
 * 200-219:  Cache system options
 * 300-319:  Apache integration options
 * 400-499:  Reserved for future protocols
 * 500-999:  Reserved for custom implementations
 */

// Streamlined socket options with consistent SOCKET_OPT_ naming
enum socket_options {
    // Core socket options with perfect backwards compatibility
    SOCKET_OPT_INVALID = 0,            // Legacy: Original value 0
    SOCKET_OPT_TLS_VERIFY_PEER = 1,     // Legacy: Original value 1
    SOCKET_OPT_TLS_SNI_HOSTNAME = 2,    // Legacy: Original value 2
    SOCKET_OPT_KEEPALIVE = 3,           // Core socket options start at 3
    SOCKET_OPT_NODELAY = 4,
    SOCKET_OPT_REUSEADDR = 5,
    SOCKET_OPT_RCVBUF = 6,
    SOCKET_OPT_SNDBUF = 7,
    SOCKET_OPT_TIMEOUT = 8,
    SOCKET_OPT_LINGER = 9,
    SOCKET_OPT_BROADCAST = 10,
    
    // Authentication and security (20-39)
    SOCKET_OPT_AUTH_TOKEN = 20,
    SOCKET_OPT_AUTH_USERNAME = 21,
    SOCKET_OPT_AUTH_PASSWORD = 22,
    SOCKET_OPT_AUTH_REALM = 23,
    SOCKET_OPT_AUTH_DIGEST = 24,
    SOCKET_OPT_AUTH_BASIC = 25,
    SOCKET_OPT_AUTH_BEARER = 26,
    SOCKET_OPT_SECURITY_POLICY = 27,
    
    // General protocol options (40-99)
    SOCKET_OPT_PROTOCOL_VERSION = 40,
    SOCKET_OPT_ENCODING = 41,
    SOCKET_OPT_COMPRESSION = 42,
    SOCKET_OPT_BUFFER_SIZE = 43,
    SOCKET_OPT_MAX_CONNECTIONS = 44,
    SOCKET_OPT_CONNECTION_POOL = 45,
    SOCKET_OPT_RETRY_COUNT = 46,
    SOCKET_OPT_RETRY_DELAY = 47,
    SOCKET_OPT_DEBUG_LEVEL = 48,
    SOCKET_OPT_LOG_REQUESTS = 49,
    
    // HTTP/HTTPS options: 100-119 (consistent protocol naming)
    HTTP_HEADERS = 100,
    HTTP_METHOD = 101,
    HTTP_URL = 102,
    HTTP_BODY = 103,
    HTTP_TIMEOUT = 104,
    HTTP_USER_AGENT = 105,
    HTTP_FOLLOW_REDIRECTS = 106,
    HTTP_MAX_REDIRECTS = 107,
    HTTP_CONNECT_TIMEOUT = 108,
    HTTP_READ_TIMEOUT = 109,
    
    // REST configuration: 110-119
    REST_ROUTER_CONFIG = 110,
    REST_ADD_ROUTE = 111,
    REST_OPENAPI_INFO = 112,
    REST_JWT_SECRET = 113,
    REST_DOCS_PATH = 114,
    REST_VALIDATION_SCHEMA = 115,
    REST_MIDDLEWARE = 116,
    REST_ERROR_HANDLER = 117,
    REST_CORS_CONFIG = 118,
    REST_RATE_LIMIT = 119,
    
    // WebSocket options: 120-129
    WS_PROTOCOL = 120,
    WS_EXTENSIONS = 121,
    WS_ORIGIN = 122,
    WS_MAX_MESSAGE_SIZE = 123,
    WS_PING_INTERVAL = 124,
    WS_PONG_TIMEOUT = 125,
    WS_COMPRESSION = 126,
    WS_SUBPROTOCOL = 127,
    WS_FRAME_SIZE_LIMIT = 128,
    WS_AUTO_PING = 129,
    
    // MQTT options: 130-139
    MQTT_BROKER = 130,
    MQTT_CLIENT_ID = 131,
    MQTT_USERNAME = 132,
    MQTT_PASSWORD = 133,
    MQTT_KEEP_ALIVE = 134,
    MQTT_QOS = 135,
    MQTT_RETAIN = 136,
    MQTT_CLEAN_SESSION = 137,
    MQTT_WILL_TOPIC = 138,
    MQTT_WILL_MESSAGE = 139,
    
    // External options: 140-149
    EXTERNAL_COMMAND = 140,
    EXTERNAL_ARGS = 141,
    EXTERNAL_ENV = 142,
    EXTERNAL_WATCH_PATH = 143,
    EXTERNAL_WORKING_DIR = 144,
    EXTERNAL_USER = 145,
    EXTERNAL_GROUP = 146,
    EXTERNAL_TIMEOUT = 147,
    EXTERNAL_BUFFER_SIZE = 148,
    EXTERNAL_ASYNC = 149,
    
    // Advanced external options: 150-159
    EXTERNAL_STDIN_MODE = 150,
    EXTERNAL_STDOUT_MODE = 151,
    EXTERNAL_STDERR_MODE = 152,
    EXTERNAL_SHELL = 153,
    EXTERNAL_PRIORITY = 154,
    EXTERNAL_CPU_LIMIT = 155,
    EXTERNAL_MEMORY_LIMIT = 156,
    EXTERNAL_FILE_LIMIT = 157,
    EXTERNAL_SANDBOX = 158,
    EXTERNAL_RESTART_POLICY = 159,
    
    // Database integration: 160-179
    DB_CONNECTION_STRING = 160,
    DB_POOL_SIZE = 161,
    DB_TIMEOUT = 162,
    DB_RETRY_POLICY = 163,
    DB_TRANSACTION_MODE = 164,
    DB_CHARSET = 165,
    DB_SSL_MODE = 166,
    DB_PREPARED_STATEMENTS = 167,
    DB_RESULT_FORMAT = 168,
    DB_BATCH_SIZE = 169,
    
    // Message Queue options: 180-199
    MQ_EXCHANGE = 180,
    MQ_ROUTING_KEY = 181,
    MQ_QUEUE_NAME = 182,
    MQ_DURABLE = 183,
    MQ_EXCLUSIVE = 184,
    MQ_AUTO_DELETE = 185,
    MQ_PREFETCH_COUNT = 186,
    MQ_ACK_MODE = 187,
    MQ_PRIORITY = 188,
    MQ_TTL = 189,
    
    // Cache options: 200-219 (consistent protocol naming)
    CACHE_ENABLE = 200,
    CACHE_TTL = 201,
    CACHE_MAX_SIZE = 202,
    CACHE_KEY_PATTERN = 203,
    CACHE_HEADERS = 204,
    CACHE_METHODS = 205,
    CACHE_EXCLUDE_PATTERNS = 206,
    CACHE_THREAD_POOL_SIZE = 207,
    CACHE_INVALIDATE_PATTERN = 208,
    CACHE_STATISTICS = 209,
    CACHE_COMPRESSION = 210,
    CACHE_STORAGE_BACKEND = 211,
    CACHE_CLEANUP_INTERVAL = 212,
    CACHE_MAX_MEMORY = 213,
    CACHE_PERSISTENCE = 214,
    CACHE_REPLICATION = 215,
    
    // Load balancing: 220-239
    LB_STRATEGY = 220,
    LB_HEALTH_CHECK_URL = 221,
    LB_HEALTH_CHECK_INTERVAL = 222,
    LB_MAX_FAILURES = 223,
    LB_FAILOVER_TIMEOUT = 224,
    LB_STICKY_SESSIONS = 225,
    LB_SESSION_AFFINITY = 226,
    LB_WEIGHTED_ROUTING = 227,
    LB_BACKUP_SERVERS = 228,
    LB_CIRCUIT_BREAKER = 229,
    
    // Monitoring and metrics: 240-259
    MONITOR_ENABLE = 240,
    MONITOR_METRICS_PATH = 241,
    MONITOR_SAMPLE_RATE = 242,
    MONITOR_MAX_SAMPLES = 243,
    MONITOR_ALERT_THRESHOLD = 244,
    MONITOR_ALERT_WEBHOOK = 245,
    MONITOR_LOG_LEVEL = 246,
    MONITOR_TRACE_REQUESTS = 247,
    MONITOR_PERFORMANCE_LOG = 248,
    MONITOR_ERROR_TRACKING = 249,
    
    // Rate limiting: 260-279
    RATE_LIMIT_REQUESTS_PER_SECOND = 260,
    RATE_LIMIT_REQUESTS_PER_MINUTE = 261,
    RATE_LIMIT_REQUESTS_PER_HOUR = 262,
    RATE_LIMIT_BURST_SIZE = 263,
    RATE_LIMIT_WINDOW_SIZE = 264,
    RATE_LIMIT_ALGORITHM = 265,
    RATE_LIMIT_KEY_GENERATOR = 266,
    RATE_LIMIT_ERROR_RESPONSE = 267,
    RATE_LIMIT_HEADERS = 268,
    RATE_LIMIT_SKIP_PATHS = 269,
    
    // Content processing: 280-299
    CONTENT_TYPE_DETECTION = 280,
    CONTENT_ENCODING = 281,
    CONTENT_CHARSET = 282,
    CONTENT_VALIDATION = 283,
    CONTENT_SANITIZATION = 284,
    CONTENT_TRANSFORMATION = 285,
    CONTENT_FILTERING = 286,
    CONTENT_SIZE_LIMIT = 287,
    CONTENT_STREAMING = 288,
    CONTENT_BUFFERING = 289,
    
    // Apache integration: 300-319
    SO_APACHE_FASTCGI_ENABLE = 300,
    SO_APACHE_FASTCGI_SOCKET = 301,
    SO_APACHE_PROXY_ENABLE = 302,
    SO_APACHE_PROXY_UPSTREAM = 303,
    SO_APACHE_STATIC_HANDOFF = 304,
    SO_APACHE_DYNAMIC_HANDOFF = 305,
    SO_APACHE_CGI_ENV_VARS = 306,
    SO_APACHE_AUTH_PASSTHROUGH = 307,
    SO_APACHE_SSL_TERMINATION = 308,
    SO_APACHE_LOAD_BALANCE = 309,
    SO_APACHE_VIRTUAL_HOST = 310,
    SO_APACHE_DOCUMENT_ROOT = 311,
    SO_APACHE_INDEX_FILES = 312,
    SO_APACHE_ERROR_PAGES = 313,
    SO_APACHE_ACCESS_LOG = 314,
    SO_APACHE_ERROR_LOG = 315,
    
    // SSL/TLS advanced options: 320-339
    TLS_CIPHER_SUITES = 320,
    TLS_PROTOCOL_VERSION = 321,
    TLS_CERTIFICATE_FILE = 322,
    TLS_PRIVATE_KEY_FILE = 323,
    TLS_CA_BUNDLE = 324,
    TLS_VERIFY_DEPTH = 325,
    TLS_SESSION_CACHE = 326,
    TLS_SESSION_TIMEOUT = 327,
    TLS_RENEGOTIATION = 328,
    TLS_ALPN_PROTOCOLS = 329,
    TLS_SNI_CALLBACK = 330,
    TLS_OCSP_STAPLING = 331,
    TLS_HSTS_HEADER = 332,
    TLS_CLIENT_CERT_REQUIRED = 333,
    
    // HTTP/2 and HTTP/3 options: 340-359
    HTTP2_ENABLE = 340,
    HTTP2_MAX_STREAMS = 341,
    HTTP2_WINDOW_SIZE = 342,
    HTTP2_FRAME_SIZE = 343,
    HTTP2_HEADER_TABLE_SIZE = 344,
    HTTP2_PUSH_ENABLE = 345,
    HTTP2_PRIORITY_ENABLE = 346,
    HTTP3_ENABLE = 347,
    HTTP3_MAX_STREAMS = 348,
    HTTP3_QUIC_VERSION = 349,
    
    // GraphQL options: 360-379
    GRAPHQL_SCHEMA = 360,
    GRAPHQL_ENDPOINT = 361,
    GRAPHQL_PLAYGROUND = 362,
    GRAPHQL_INTROSPECTION = 363,
    GRAPHQL_MAX_DEPTH = 364,
    GRAPHQL_MAX_COMPLEXITY = 365,
    GRAPHQL_TIMEOUT = 366,
    GRAPHQL_CACHE_ENABLE = 367,
    GRAPHQL_BATCH_ENABLE = 368,
    GRAPHQL_SUBSCRIPTIONS = 369,
    
    // gRPC options: 380-399
    GRPC_SERVICE_CONFIG = 380,
    GRPC_MAX_MESSAGE_SIZE = 381,
    GRPC_KEEPALIVE_TIME = 382,
    GRPC_KEEPALIVE_TIMEOUT = 383,
    GRPC_REFLECTION_ENABLE = 384,
    GRPC_HEALTH_CHECK = 385,
    GRPC_COMPRESSION = 386,
    GRPC_METADATA = 387,
    GRPC_DEADLINE = 388,
    GRPC_RETRY_POLICY = 389,
    
    // Reserved ranges for future expansion
    // 400-499: Future protocol options
    // 500-599: Custom implementation options
    // 600-699: Third-party integration options
    // 700-799: Advanced networking options
    // 800-899: Development and debugging options
    // 900-999: Experimental features
    
    // Internal/system options (1000+)
    SOCKET_OPT_SOCKET_MODE = 1000,       // Core socket mode (keep SOCKET_OPT_)
    HTTP_SERVER_MODE = 1001,             // Protocol-specific modes
    HTTP_CLIENT_MODE = 1002,
    REST_MODE = 1003,                    // Keep existing protocol naming
    WS_MODE = 1004,
    MQTT_MODE = 1005,
    EXTERNAL_MODE = 1006
};

/*
 * Option Categories - for validation and management
 */
enum socket_option_category {
    OPTION_CATEGORY_CORE = 0,
    OPTION_CATEGORY_AUTH = 1,
    OPTION_CATEGORY_HTTP = 2,
    OPTION_CATEGORY_REST = 3,
    OPTION_CATEGORY_WEBSOCKET = 4,
    OPTION_CATEGORY_MQTT = 5,
    OPTION_CATEGORY_EXTERNAL = 6,
    OPTION_CATEGORY_DATABASE = 7,
    OPTION_CATEGORY_CACHE = 8,
    OPTION_CATEGORY_APACHE = 9,
    OPTION_CATEGORY_TLS = 10,
    OPTION_CATEGORY_MONITORING = 11,
    OPTION_CATEGORY_INTERNAL = 12
};

/*
 * Option Value Types - for validation and conversion
 */
enum socket_option_type {
    OPTION_TYPE_INTEGER = 0,
    OPTION_TYPE_STRING = 1,
    OPTION_TYPE_BOOLEAN = 2,
    OPTION_TYPE_FLOAT = 3,
    OPTION_TYPE_MAPPING = 4,
    OPTION_TYPE_ARRAY = 5,
    OPTION_TYPE_MIXED = 6
};

/*
 * Option Access Levels - for security
 */
enum socket_option_access {
    OPTION_ACCESS_PUBLIC = 0,      // Any object can read/write
    OPTION_ACCESS_OWNER = 1,       // Only socket owner can read/write
    OPTION_ACCESS_PRIVILEGED = 2,  // Only privileged objects can read/write
    OPTION_ACCESS_SYSTEM = 3,      // Only system can read/write
    OPTION_ACCESS_READONLY = 4     // Read-only after initial set
};

/*
 * Socket Mode Extensions - additional modes for unified architecture
 */
enum socket_mode_extended {
    // Basic modes (preserve existing values)
    SOCKET_MUD = 0,
    SOCKET_STREAM = 1,
    SOCKET_DATAGRAM = 2,
    SOCKET_STREAM_BINARY = 3,
    SOCKET_DATAGRAM_BINARY = 4,
    SOCKET_STREAM_TLS = 5,
    SOCKET_STREAM_TLS_BINARY = 6,
    
    // Extended modes for unified architecture
    HTTP_SERVER = 10,
    HTTP_CLIENT = 11,
    HTTPS_SERVER = 12,
    HTTPS_CLIENT = 13,
    REST_SERVER = 14,
    REST_CLIENT = 15,
    WEBSOCKET_SERVER = 16,
    WEBSOCKET_CLIENT = 17,
    WEBSOCKET_TLS_SERVER = 18,
    WEBSOCKET_TLS_CLIENT = 19,
    MQTT_CLIENT = 20,
    MQTT_TLS_CLIENT = 21,
    EXTERNAL_PROCESS = 22,
    EXTERNAL_COMMAND_MODE = 23,
    DATABASE_CLIENT = 24,
    GRAPHQL_SERVER = 25,
    GRAPHQL_CLIENT = 26,
    GRPC_SERVER = 27,
    GRPC_CLIENT = 28
};

/*
 * Utility macros for option management
 */
#define IS_CORE_SOCKET_OPTION(opt)  ((opt) >= 1 && (opt) <= 99)   // All SOCKET_OPT_ options
#define IS_LEGACY_OPTION(opt)       ((opt) >= 1 && (opt) <= 2)    // Legacy TLS only
#define IS_HTTP_OPTION(opt)         ((opt) >= 100 && (opt) <= 119)
#define IS_REST_OPTION(opt)         ((opt) >= 110 && (opt) <= 119)
#define IS_WS_OPTION(opt)           ((opt) >= 120 && (opt) <= 129)
#define IS_MQTT_OPTION(opt)         ((opt) >= 130 && (opt) <= 139)
#define IS_EXTERNAL_OPTION(opt)     ((opt) >= 140 && (opt) <= 159)
#define IS_CACHE_OPTION(opt)        ((opt) >= 200 && (opt) <= 219)
#define IS_APACHE_OPTION(opt)       ((opt) >= 300 && (opt) <= 319)
#define IS_TLS_OPTION(opt)          ((opt) >= 320 && (opt) <= 339)
#define IS_INTERNAL_OPTION(opt)     ((opt) >= 1000)

#define GET_OPTION_CATEGORY(opt) \
    (IS_HTTP_OPTION(opt) ? OPTION_CATEGORY_HTTP : \
     IS_REST_OPTION(opt) ? OPTION_CATEGORY_REST : \
     IS_WS_OPTION(opt) ? OPTION_CATEGORY_WEBSOCKET : \
     IS_MQTT_OPTION(opt) ? OPTION_CATEGORY_MQTT : \
     IS_EXTERNAL_OPTION(opt) ? OPTION_CATEGORY_EXTERNAL : \
     IS_CACHE_OPTION(opt) ? OPTION_CATEGORY_CACHE : \
     IS_APACHE_OPTION(opt) ? OPTION_CATEGORY_APACHE : \
     IS_TLS_OPTION(opt) ? OPTION_CATEGORY_TLS : \
     IS_INTERNAL_OPTION(opt) ? OPTION_CATEGORY_INTERNAL : \
     OPTION_CATEGORY_CORE)

/*
 * Default option values - used when option is not explicitly set
 */
#define DEFAULT_HTTP_TIMEOUT 30000
#define DEFAULT_HTTP_MAX_REDIRECTS 5
#define DEFAULT_HTTP_USER_AGENT "FluffOS/3.0"
#define DEFAULT_WS_MAX_MESSAGE_SIZE 65536
#define DEFAULT_WS_PING_INTERVAL 30
#define DEFAULT_MQTT_KEEP_ALIVE 60
#define DEFAULT_MQTT_QOS 0
#define DEFAULT_CACHE_TTL 300
#define DEFAULT_EXTERNAL_TIMEOUT 30
#define DEFAULT_DB_TIMEOUT 30
#define DEFAULT_RATE_LIMIT_REQUESTS_PER_SECOND 100

/*
 * Option validation constraints
 */
#define MIN_HTTP_TIMEOUT 1000
#define MAX_HTTP_TIMEOUT 300000
#define MIN_HTTP_MAX_REDIRECTS 0
#define MAX_HTTP_MAX_REDIRECTS 20
#define MIN_WS_MESSAGE_SIZE 128
#define MAX_WS_MESSAGE_SIZE 16777216  // 16MB
#define MIN_MQTT_KEEP_ALIVE 10
#define MAX_MQTT_KEEP_ALIVE 3600
#define MIN_CACHE_TTL 1
#define MAX_CACHE_TTL 86400
#define MIN_EXTERNAL_TIMEOUT 1
#define MAX_EXTERNAL_TIMEOUT 3600

#endif  // SOCKET_OPTIONS_H_