#include "log.h"

#include <iostream>

// Constructor: Open the log file in append mode
Logger::Logger(const std::string& filename) {
    logFile.open(filename, std::ios::out | std::ios::trunc);
    if (!logFile.is_open()) {
        std::cerr << "Error: Could not open log file: logfile.txt" << std::endl;
    }
}

// Destructor: Close the log file
Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

// Write a message to the log file
void Logger::log(const std::string& message) {
    if (logFile.is_open()) {
        logFile << "[" << get_curr_time() << "] " << message << std::endl;
    } else {
        std::cerr << "Log file is not open!" << std::endl;
    }
}

// Get the current time as a string
std::string Logger::get_curr_time() {
    std::time_t curr_time = std::time(nullptr);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&curr_time));
    return buf;
}
