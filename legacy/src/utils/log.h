#pragma once

#include <fstream>
#include <string>

// Debug logger. Disabled unless the CURSEY_LOG environment variable names a
// file to write to, e.g. `CURSEY_LOG=/tmp/cursey.log ./cursey file.txt`.
// (The editor owns the terminal, so printing to stdout/stderr is not an
// option while it runs.)
class Logger {
public:
    static Logger& get();

    void log(const std::string& message);

private:
    Logger();
    std::ofstream log_file;
    static std::string get_curr_time();
};

inline void log_debug(const std::string& message) {
    Logger::get().log(message);
}
