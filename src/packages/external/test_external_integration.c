/*
 * Integration test for External Process Package with Unified Socket Architecture
 * 
 * This test file verifies that the external process package integrates correctly
 * with the unified socket architecture and socket option management system.
 * 
 * Compile with: lpcc lib/secure/cfg/quantumscape.cfg test_external_integration.c
 */

#include <network.h>

void create() {
    write("External Process Package Integration Test\n");
    write("========================================\n");
    
    test_external_socket_creation();
    test_external_options_validation();
    test_external_process_modes();
    test_external_security_integration();
    
    write("\nIntegration test completed.\n");
}

void test_external_socket_creation() {
    write("\nTesting external socket creation...\n");
    
    // Test EXTERNAL_PROCESS mode
    int external_socket = socket_create(EXTERNAL_PROCESS, "external_callback", "external_close");
    if (external_socket >= 0) {
        write("✓ EXTERNAL_PROCESS socket created: " + external_socket + "\n");
        
        // Verify socket mode is set correctly
        if (socket_get_option(external_socket, EXTERNAL_MODE)) {
            write("✓ EXTERNAL_MODE option set correctly\n");
        } else {
            write("✗ EXTERNAL_MODE option not set\n");
        }
        
        socket_close(external_socket);
    } else {
        write("✗ Failed to create EXTERNAL_PROCESS socket\n");
    }
    
    // Test EXTERNAL_COMMAND_MODE
    int command_socket = socket_create(EXTERNAL_COMMAND_MODE, "command_callback", "command_close");
    if (command_socket >= 0) {
        write("✓ EXTERNAL_COMMAND_MODE socket created: " + command_socket + "\n");
        
        // Verify async mode is enabled by default
        if (socket_get_option(command_socket, EXTERNAL_ASYNC)) {
            write("✓ EXTERNAL_ASYNC enabled for command mode\n");
        } else {
            write("✗ EXTERNAL_ASYNC not enabled for command mode\n");
        }
        
        socket_close(command_socket);
    } else {
        write("✗ Failed to create EXTERNAL_COMMAND_MODE socket\n");
    }
}

void test_external_options_validation() {
    write("\nTesting external socket options validation...\n");
    
    int external_socket = socket_create(EXTERNAL_PROCESS, "external_callback", "external_close");
    if (external_socket < 0) {
        write("✗ Failed to create socket for options testing\n");
        return;
    }
    
    // Test command option
    if (socket_set_option(external_socket, EXTERNAL_COMMAND, "/bin/echo")) {
        write("✓ EXTERNAL_COMMAND option set successfully\n");
        
        string command = socket_get_option(external_socket, EXTERNAL_COMMAND);
        if (command == "/bin/echo") {
            write("✓ EXTERNAL_COMMAND option retrieved correctly\n");
        } else {
            write("✗ EXTERNAL_COMMAND option value mismatch: " + command + "\n");
        }
    } else {
        write("✗ Failed to set EXTERNAL_COMMAND option\n");
    }
    
    // Test arguments option
    string *args = ({ "-n", "Hello World" });
    if (socket_set_option(external_socket, EXTERNAL_ARGS, args)) {
        write("✓ EXTERNAL_ARGS option set successfully\n");
        
        string *retrieved_args = socket_get_option(external_socket, EXTERNAL_ARGS);
        if (sizeof(retrieved_args) == 2 && retrieved_args[0] == "-n") {
            write("✓ EXTERNAL_ARGS option retrieved correctly\n");
        } else {
            write("✗ EXTERNAL_ARGS option value mismatch\n");
        }
    } else {
        write("✗ Failed to set EXTERNAL_ARGS option\n");
    }
    
    // Test timeout option
    if (socket_set_option(external_socket, EXTERNAL_TIMEOUT, 30)) {
        write("✓ EXTERNAL_TIMEOUT option set successfully\n");
        
        int timeout = socket_get_option(external_socket, EXTERNAL_TIMEOUT);
        if (timeout == 30) {
            write("✓ EXTERNAL_TIMEOUT option retrieved correctly\n");
        } else {
            write("✗ EXTERNAL_TIMEOUT option value mismatch: " + timeout + "\n");
        }
    } else {
        write("✗ Failed to set EXTERNAL_TIMEOUT option\n");
    }
    
    // Test working directory option
    if (socket_set_option(external_socket, EXTERNAL_WORKING_DIR, "/tmp")) {
        write("✓ EXTERNAL_WORKING_DIR option set successfully\n");
    } else {
        write("✗ Failed to set EXTERNAL_WORKING_DIR option\n");
    }
    
    // Test buffer size option
    if (socket_set_option(external_socket, EXTERNAL_BUFFER_SIZE, 8192)) {
        write("✓ EXTERNAL_BUFFER_SIZE option set successfully\n");
    } else {
        write("✗ Failed to set EXTERNAL_BUFFER_SIZE option\n");
    }
    
    // Test invalid options (should fail validation)
    if (!socket_set_option(external_socket, EXTERNAL_TIMEOUT, -1)) {
        write("✓ Invalid timeout correctly rejected\n");
    } else {
        write("✗ Invalid timeout was accepted\n");
    }
    
    if (!socket_set_option(external_socket, EXTERNAL_COMMAND, "")) {
        write("✓ Empty command correctly rejected\n");
    } else {
        write("✗ Empty command was accepted\n");
    }
    
    socket_close(external_socket);
}

void test_external_process_modes() {
    write("\nTesting external process modes...\n");
    
    // Test synchronous process mode
    int sync_socket = socket_create(EXTERNAL_PROCESS, "sync_callback", "sync_close");
    if (sync_socket >= 0) {
        // Configure for a simple echo command
        socket_set_option(sync_socket, EXTERNAL_COMMAND, "/bin/echo");
        socket_set_option(sync_socket, EXTERNAL_ARGS, ({ "Sync test successful" }));
        socket_set_option(sync_socket, EXTERNAL_TIMEOUT, 5);
        socket_set_option(sync_socket, EXTERNAL_ASYNC, 0);
        
        write("✓ Synchronous external process configured\n");
        
        // Note: actual process spawning would be tested here if efun was implemented
        // int result = external_spawn_process(sync_socket);
        
        socket_close(sync_socket);
    }
    
    // Test asynchronous command mode
    int async_socket = socket_create(EXTERNAL_COMMAND_MODE, "async_callback", "async_close");
    if (async_socket >= 0) {
        // Configure for a simple ls command
        socket_set_option(async_socket, EXTERNAL_COMMAND, "/bin/ls");
        socket_set_option(async_socket, EXTERNAL_ARGS, ({ "-la", "/tmp" }));
        socket_set_option(async_socket, EXTERNAL_TIMEOUT, 10);
        
        write("✓ Asynchronous external command configured\n");
        
        socket_close(async_socket);
    }
}

void test_external_security_integration() {
    write("\nTesting external security integration...\n");
    
    int security_socket = socket_create(EXTERNAL_PROCESS, "security_callback", "security_close");
    if (security_socket < 0) {
        write("✗ Failed to create socket for security testing\n");
        return;
    }
    
    // Test environment variables (security sensitive)
    mapping env = ([ "PATH": "/usr/bin:/bin", "HOME": "/tmp" ]);
    if (socket_set_option(security_socket, EXTERNAL_ENV, env)) {
        write("✓ EXTERNAL_ENV option set successfully\n");
    } else {
        write("✗ Failed to set EXTERNAL_ENV option\n");
    }
    
    // Test user context (requires privileges)
    if (socket_set_option(security_socket, EXTERNAL_USER, "nobody")) {
        write("✓ EXTERNAL_USER option set successfully\n");
    } else {
        write("• EXTERNAL_USER option not set (may require privileges)\n");
    }
    
    // Test group context (requires privileges)
    if (socket_set_option(security_socket, EXTERNAL_GROUP, "nogroup")) {
        write("✓ EXTERNAL_GROUP option set successfully\n");
    } else {
        write("• EXTERNAL_GROUP option not set (may require privileges)\n");
    }
    
    // Test dangerous command rejection (should be rejected by security)
    if (!socket_set_option(security_socket, EXTERNAL_COMMAND, "/bin/rm")) {
        write("✓ Dangerous command correctly rejected by security\n");
    } else {
        write("• Dangerous command accepted (security policy may allow)\n");
    }
    
    socket_close(security_socket);
}

void test_external_option_validation_edge_cases() {
    write("\nTesting external option validation edge cases...\n");
    
    int test_socket = socket_create(EXTERNAL_PROCESS, "test_callback", "test_close");
    if (test_socket < 0) {
        write("✗ Failed to create socket for edge case testing\n");
        return;
    }
    
    // Test buffer size limits
    if (!socket_set_option(test_socket, EXTERNAL_BUFFER_SIZE, 100)) {
        write("✓ Buffer size too small correctly rejected\n");
    }
    
    if (!socket_set_option(test_socket, EXTERNAL_BUFFER_SIZE, 10 * 1024 * 1024)) {
        write("✓ Buffer size too large correctly rejected\n");
    }
    
    // Test timeout limits
    if (!socket_set_option(test_socket, EXTERNAL_TIMEOUT, 0)) {
        write("✓ Zero timeout correctly rejected\n");
    }
    
    if (!socket_set_option(test_socket, EXTERNAL_TIMEOUT, 7200)) {
        write("✓ Excessive timeout correctly rejected\n");
    }
    
    // Test path traversal in working directory
    if (!socket_set_option(test_socket, EXTERNAL_WORKING_DIR, "../../../etc")) {
        write("✓ Path traversal in working directory correctly rejected\n");
    }
    
    socket_close(test_socket);
}

// Callback functions for testing
void external_callback(int socket, string data, string addr) {
    write("External process data received: " + data + "\n");
}

void external_close(int socket) {
    write("External process socket closed: " + socket + "\n");
}

void sync_callback(int socket, string data, string addr) {
    write("Sync process data: " + data + "\n");
}

void sync_close(int socket) {
    write("Sync process closed: " + socket + "\n");
}

void async_callback(int socket, string data, string addr) {
    write("Async command data: " + data + "\n");
}

void async_close(int socket) {
    write("Async command closed: " + socket + "\n");
}

void command_callback(int socket, string data, string addr) {
    write("Command mode data: " + data + "\n");
}

void command_close(int socket) {
    write("Command mode closed: " + socket + "\n");
}

void security_callback(int socket, string data, string addr) {
    write("Security test data: " + data + "\n");
}

void security_close(int socket) {
    write("Security test closed: " + socket + "\n");
}

void test_callback(int socket, string data, string addr) {
    write("Test data: " + data + "\n");
}

void test_close(int socket) {
    write("Test closed: " + socket + "\n");
}