#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <fstream>
#include <string>
#include <mutex>

/**
 * @brief A simple logger for the Note Naga Engine. Logs messages to file and console.
 * 
 * @example Usage:
 * NOTE_NAGA_LOG_INFO("This is an info message.");
 * NOTE_NAGA_LOG_WARNING("This is a warning message.");
 * NOTE_NAGA_LOG_ERROR("This is an error message.");
 */
class NOTE_NAGA_ENGINE_API NoteNagaLogger {
public:
    /**
     * @brief Log levels for the Note Naga Engine logger.
     * These levels can be used to categorize log messages.
     * - INFO: General information messages.
     * - WARNING: Messages indicating potential issues.
     * - ERROR: Messages indicating errors that need attention.
     */
    enum class Level { INFO, WARNING, ERROR };

    static NoteNagaLogger& instance() {
        static NoteNagaLogger inst;
        return inst;
    }

    /**
     * @brief Logs a message with the specified level and file name.
     * @param level The log level (INFO, WARNING, ERROR).
     * @param msg The message to log.
     * @param file The source file name where the log is called.
     */
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