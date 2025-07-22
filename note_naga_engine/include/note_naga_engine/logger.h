#pragma once

#include <fstream>
#include <string>
#include <mutex>

class NoteNagaLogger {
public:
    enum class Level { INFO, WARNING, ERROR };

    static NoteNagaLogger& instance() {
        static NoteNagaLogger inst;
        return inst;
    }

    void log(Level level, const std::string& msg, const char* file);

    // Convenience methods
    void info(const std::string& msg, const char* file)    { log(Level::INFO,    msg, file); }
    void warning(const std::string& msg, const char* file) { log(Level::WARNING, msg, file); }
    void error(const std::string& msg, const char* file)   { log(Level::ERROR,   msg, file); }

private:
    NoteNagaLogger();
    virtual~NoteNagaLogger() {
        logfile_.close();
    }
    NoteNagaLogger(const NoteNagaLogger&) = delete;
    NoteNagaLogger& operator=(const NoteNagaLogger&) = delete;

    std::ofstream logfile_;
    std::mutex mutex_;

    static std::string currentDateTime();
    static std::string shortFileName(const std::string& path);
};

// Macros for easy logging (automatically adds file name)
#define NOTE_NAGA_LOG_INFO(msg)    NoteNagaLogger::instance().info(msg, __FILE__)
#define NOTE_NAGA_LOG_WARNING(msg) NoteNagaLogger::instance().warning(msg, __FILE__)
#define NOTE_NAGA_LOG_ERROR(msg)   NoteNagaLogger::instance().error(msg, __FILE__)