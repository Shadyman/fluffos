/*
 * Test for File Monitor Implementation
 * Phase 1: inotify integration testing
 */

#include "file_monitor.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <unistd.h>

int main() {
    std::cout << "Testing FileMonitor inotify implementation..." << std::endl;
    
    // Initialize the file monitor system
    if (!init_file_monitor_system()) {
        std::cerr << "Failed to initialize file monitor system" << std::endl;
        return 1;
    }
    
    // Create a test directory and file
    std::string test_dir = "/tmp/fluffos_file_monitor_test";
    std::string test_file = test_dir + "/test_file.txt";
    
    // Create directory if it doesn't exist
    system(("mkdir -p " + test_dir).c_str());
    
    // Get monitor instance
    FileMonitor& monitor = FileMonitor::instance();
    
    // Test socket ID for testing
    int test_socket = 999;
    
    // Add watch for the test directory
    std::cout << "Adding watch for: " << test_dir << std::endl;
    if (!monitor.add_watch(test_socket, test_dir)) {
        std::cerr << "Failed to add watch for test directory" << std::endl;
        cleanup_file_monitor_system();
        return 1;
    }
    
    // Create a file to trigger events
    std::cout << "Creating test file: " << test_file << std::endl;
    std::ofstream file(test_file);
    file << "Initial content" << std::endl;
    file.close();
    
    // Give inotify time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Process events
    std::cout << "Processing file events..." << std::endl;
    std::vector<FileEvent> events = monitor.process_events();
    
    std::cout << "Found " << events.size() << " file events:" << std::endl;
    for (const auto& event : events) {
        std::cout << "  - Path: " << event.path << std::endl;
        std::cout << "    Name: " << event.name << std::endl;
        std::cout << "    Type: " << static_cast<int>(event.event_type) << std::endl;
        std::cout << "    Directory: " << (event.is_directory ? "yes" : "no") << std::endl;
        std::cout << "    Timestamp: " << event.timestamp << std::endl;
        std::cout << std::endl;
    }
    
    // Test file modification
    std::cout << "Modifying test file..." << std::endl;
    std::ofstream file2(test_file, std::ios::app);
    file2 << "Modified content" << std::endl;
    file2.close();
    
    // Give inotify time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Process more events
    events = monitor.process_events();
    std::cout << "Found " << events.size() << " additional file events after modification" << std::endl;
    
    // Test LPC interface functions
    std::cout << "Testing LPC interface functions..." << std::endl;
    
    int result = FileMonitorManager::external_monitor_path(test_socket, test_file, 0);
    std::cout << "external_monitor_path result: " << result << std::endl;
    
    // Test path validation
    std::cout << "Testing path validation..." << std::endl;
    bool valid1 = FileMonitorManager::validate_monitor_path("/tmp/valid_path");
    bool valid2 = FileMonitorManager::validate_monitor_path("../invalid_path");
    bool valid3 = FileMonitorManager::validate_monitor_path("/etc/passwd");
    
    std::cout << "  /tmp/valid_path: " << (valid1 ? "valid" : "invalid") << std::endl;
    std::cout << "  ../invalid_path: " << (valid2 ? "valid" : "invalid") << std::endl;
    std::cout << "  /etc/passwd: " << (valid3 ? "valid" : "invalid") << std::endl;
    
    // Test utility functions
    std::cout << "Testing utility functions..." << std::endl;
    bool safe1 = FileMonitorUtils::is_safe_path("/tmp/safe");
    bool safe2 = FileMonitorUtils::is_safe_path("../unsafe");
    bool safe3 = FileMonitorUtils::is_safe_path("/tmp//double_slash");
    
    std::cout << "  /tmp/safe: " << (safe1 ? "safe" : "unsafe") << std::endl;
    std::cout << "  ../unsafe: " << (safe2 ? "safe" : "unsafe") << std::endl;
    std::cout << "  /tmp//double_slash: " << (safe3 ? "safe" : "unsafe") << std::endl;
    
    // Cleanup
    std::cout << "Cleaning up..." << std::endl;
    monitor.remove_all_watches(test_socket);
    
    // Remove test files
    unlink(test_file.c_str());
    rmdir(test_dir.c_str());
    
    cleanup_file_monitor_system();
    
    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}