#pragma once

#include <ctime>
#include <fstream>
#include <string>

class Logger {
public:
    explicit Logger(const std::string& filename);
    ~Logger();
    void log(const std::string& message);

private:
    std::ofstream logFile;
    static std::string get_curr_time();
};
