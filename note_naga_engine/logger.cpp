#include <note_naga_engine/logger.h>

#include <iostream>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <ctime>

NoteNagaLogger::NoteNagaLogger() {
    std::filesystem::create_directories("logs");
    logfile_.open("logs/note_naga.log", std::ios::app);
    if (!logfile_) std::cerr << "Failed to open log file!\n";
}

void NoteNagaLogger::log(Level level, const std::string &msg, const char *file) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string levelStr;
    switch (level) {
    case Level::INFO:
        levelStr = "INFO   ";
        break;
    case Level::WARNING:
        levelStr = "WARNING";
        break;
    case Level::ERROR:
        levelStr = "ERROR  ";
        break;
    }
    std::ostringstream oss;
    oss << currentDateTime() << " [" << levelStr << "] " << shortFileName(file) << ": "
        << msg << std::endl;
    std::string out = oss.str();

    std::cout << out;
    logfile_ << out;
    logfile_.flush();
}

std::string NoteNagaLogger::currentDateTime() {
    std::ostringstream ss;
    std::time_t t = std::time(nullptr);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string NoteNagaLogger::shortFileName(const std::string &path) {
    auto slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}