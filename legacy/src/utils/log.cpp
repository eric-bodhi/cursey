#include "log.h"

#include <cstdlib>
#include <ctime>

Logger& Logger::get() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    if (const char* path = std::getenv("CURSEY_LOG"); path && *path) {
        log_file.open(path, std::ios::out | std::ios::app);
    }
}

void Logger::log(const std::string& message) {
    if (log_file.is_open()) {
        log_file << "[" << get_curr_time() << "] " << message << std::endl;
    }
}

std::string Logger::get_curr_time() {
    const std::time_t curr_time = std::time(nullptr);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&curr_time));
    return buf;
}
