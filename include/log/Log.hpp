#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <chrono>
#include <ctime>
#include <utility>
#include <fstream>
#include <memory>
#include <log/format.hpp>

namespace hspd {


    // 定义锁
    std::mutex g_log_mutex;

    enum class LogLevel {
        DEBUG,
        RELEASE,
        INFO,
        WARN,
        ERROR,
        FATAL,
    };
    
    inline std::string LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:
                return "DEBUG";
            case LogLevel::RELEASE:
                return "RELEASE";
            case LogLevel::INFO:
                return "INFO";
            case LogLevel::WARN:
                return "WARN";
            case LogLevel::ERROR:
                return "ERROR";
            case LogLevel::FATAL:
                return "FATAL";
            default:
                return "UNKNOWN";
        }
    }

    // 定义日志的格式
    struct LogFormat {
        std::string_view level;
        std::string_view time;
        std::string_view file;
        std::string_view line;
        std::string_view message;
    };

    constexpr std::string_view kLogFormat = "[{level}]:[{time}]:[{file}]:[{line}]:[{message}]";



    template <class DerivedLogger> 
    class Logger {
    public:
        Logger(LogFormat format, LogLevel min_level) : format_(std::move(format)), min_level_(min_level) {}

        template <typename ...Args>
        void log(LogLevel level, std::string_view file, int line, std::string_view formatStr, Args&&... args) {
            if (level < min_level_) {
                return;
            }

            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm now_tm = *std::localtime(&now_time_t);

            std::stringstream time_ss;
            time_ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
           
            std::stringstream log_ss;
            log_ss << "[" << LevelToString(level) << "]"
                   << "[" << time_ss.str() << "]"
                   << "[" << file << "]"
                   << "[" << line << "] : "
                   << format(formatStr, std::forward<Args>(args)...)
                   << std::endl;

            static_cast<DerivedLogger*>(this)->output(log_ss.str());
        }

    protected:
        LogFormat format_;
        LogLevel min_level_;

    private:
    };

    class StdoutLogger : public Logger<StdoutLogger> {
    public:
        StdoutLogger(LogFormat format, LogLevel min_level)
            : Logger<StdoutLogger>(std::move(format), min_level) {}

        void output(const std::string& log_message) {
            std::lock_guard<std::mutex> lock(g_log_mutex);
            std::cout << log_message;
        }
    };

    class FileLogger : public Logger<FileLogger> {
    public:
        FileLogger(LogFormat format, LogLevel min_level, const std::string& file_path)
            : Logger<FileLogger>(std::move(format), min_level), file_path_(file_path) {
            file_.open(file_path_, std::ios::app);
        }

        ~FileLogger() {
            file_.close();
        }

        void output(const std::string& log_message) {
            std::lock_guard<std::mutex> lock(g_log_mutex);
            file_ << log_message;
        }

    private:
        std::string file_path_;
        std::ofstream file_;
    };

    class LoggerFactory
    {
    public:
        template <typename Derived, typename... Args>
        static auto createLogger(std::string_view format = kLogFormat,
                                 LogLevel min_level = LogLevel::DEBUG, Args&&... args)
        -> std::shared_ptr<Logger<Derived>>
        {
            LogFormat log_format;
            std::stringstream ss(format.data());
            std::string token;

            while (std::getline(ss, token, ',')) {
                auto pos = token.find('=');
                if (pos != std::string::npos) {
                    std::string_view key = token.substr(0, pos);
                    std::string_view value = token.substr(pos + 1);

                    if (key == "level") {
                        log_format.level = value;
                    } else if (key == "time") {
                        log_format.time = value;
                    } else if (key == "file") {
                        log_format.file = value;
                    } else if (key == "line") {
                        log_format.line = value;
                    } else if (key == "message") {
                        log_format.message = value;
                    }
                }
            }

            auto logger = std::make_shared<Derived>(log_format, min_level, std::forward<Args>(args)...);
            return std::dynamic_pointer_cast<Logger<Derived>>(logger);
        }
    };

    enum class Choice
    {
        STDOUT,
        FILE,
        NONE,
    };
    

    class GlobalLogger {
        // 构造函数
        GlobalLogger() {
            if (g_choice == Choice::STDOUT && g_logger == nullptr)
                g_logger = LoggerFactory::createLogger<StdoutLogger>(kLogFormat, g_min_level);
            if (g_choice == Choice::FILE && g_file_logger == nullptr)
                g_file_logger = LoggerFactory::createLogger<FileLogger>(kLogFormat, g_min_level, g_file_path);
        }

        void flush() {
            if (g_choice == Choice::STDOUT)
                g_logger = LoggerFactory::createLogger<StdoutLogger>(kLogFormat, g_min_level);
            else if (g_choice == Choice::FILE)
                g_file_logger = LoggerFactory::createLogger<FileLogger>(kLogFormat, g_min_level, g_file_path);
        }

    public:
        static GlobalLogger& instance() {
            if (g_instance == nullptr) {
                g_instance.reset(new GlobalLogger());
            }
            return *g_instance;
        }

        void setLogLevel(LogLevel level) {
            if (g_min_level == level)
                return;
            g_min_level = level;
            flush();
        }

        void setLogFile(const std::string& file_path) {
            if (g_file_path == file_path)
                return;
            g_file_path = file_path;
            flush();
        }

        void setLogChoice(Choice choice) {
            if (g_choice == choice)
                return;
            g_choice = choice;
            flush();
        }

        void setFilePath(const std::string& file_path) {
            if (g_file_path == file_path)
                return;
            g_file_path = file_path;
            flush();
        }

        template <typename ...Args>
        void debug(std::string_view fmt, const std::string& file, int line, Args&&... args) {
            if (g_choice == Choice::STDOUT)
                g_logger->log(LogLevel::DEBUG, file, line, fmt, std::forward<Args>(args)...);
            else if (g_choice == Choice::FILE)
                g_file_logger->log(LogLevel::DEBUG, file, line, fmt, std::forward<Args>(args)...);
        }

        template <typename ...Args>
        void release(std::string_view fmt, const std::string& file, int line, Args&&... args) {
            if (g_choice == Choice::STDOUT)
                g_logger->log(LogLevel::RELEASE, file, line, fmt, std::forward<Args>(args)...);
            else if (g_choice == Choice::FILE)
                g_file_logger->log(LogLevel::RELEASE, file, line, fmt, std::forward<Args>(args)...);
        }

        template <typename ...Args>
        void info(std::string_view fmt, const std::string& file, int line, Args&&... args) {
            if (g_choice == Choice::STDOUT)
                g_logger->log(LogLevel::INFO, file, line, fmt, std::forward<Args>(args)...);
            else if (g_choice == Choice::FILE)
                g_file_logger->log(LogLevel::INFO, file, line, fmt, std::forward<Args>(args)...);
        }

        template <typename ...Args>
        void warn(std::string_view fmt, const std::string& file, int line, Args&&... args) {
            if (g_choice == Choice::STDOUT)
                g_logger->log(LogLevel::WARN, file, line, fmt, std::forward<Args>(args)...);
            else if (g_choice == Choice::FILE)
                g_file_logger->log(LogLevel::WARN, file, line, fmt, std::forward<Args>(args)...);
        }

        template <typename ...Args>
        void error(std::string_view fmt, const std::string& file, int line, Args&&... args) {
            if (g_choice == Choice::STDOUT)
                g_logger->log(LogLevel::ERROR, file, line, fmt, std::forward<Args>(args)...);
            else if (g_choice == Choice::FILE)
                g_file_logger->log(LogLevel::ERROR, file, line, fmt, std::forward<Args>(args)...);
        }

        template <typename ...Args>
        void fatal(std::string_view fmt, const std::string& file, int line, Args&&... args) {
            if (g_choice == Choice::STDOUT)
                g_logger->log(LogLevel::FATAL, file, line, fmt, std::forward<Args>(args)...);
            else if (g_choice == Choice::FILE)
                g_file_logger->log(LogLevel::FATAL, file, line, fmt, std::forward<Args>(args)...);
        }

    private:
        std::shared_ptr<Logger<StdoutLogger>> g_logger = nullptr;
        std::shared_ptr<Logger<FileLogger>> g_file_logger = nullptr;
        Choice g_choice = Choice::STDOUT;
        std::string g_file_path = "./log.txt";
        inline static std::unique_ptr<GlobalLogger> g_instance = nullptr;
        LogLevel g_min_level = LogLevel::DEBUG;
    };

// 定义宏, 支持无参数

#define LOG_DEBUG(fmt, ...) hspd::GlobalLogger::instance().debug(fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_RELEASE(fmt, ...) hspd::GlobalLogger::instance().release(fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) hspd::GlobalLogger::instance().info(fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) hspd::GlobalLogger::instance().warn(fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) hspd::GlobalLogger::instance().error(fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) hspd::GlobalLogger::instance().fatal(fmt, __FILE__, __LINE__, ##__VA_ARGS__)



#define ENABLE_LOG_DEBUG() hspd::GlobalLogger::instance().setLogLevel(hspd::LogLevel::DEBUG)
#define ENABLE_LOG_RELEASE() hspd::GlobalLogger::instance().setLogLevel(hspd::LogLevel::RELEASE)
#define ENABLE_LOG_INFO() hspd::GlobalLogger::instance().setLogLevel(hspd::LogLevel::INFO)
#define ENABLE_LOG_WARN() hspd::GlobalLogger::instance().setLogLevel(hspd::LogLevel::WARN)
#define ENABLE_LOG_ERROR() hspd::GlobalLogger::instance().setLogLevel(hspd::LogLevel::ERROR)
#define ENABLE_LOG_FATAL() hspd::GlobalLogger::instance().setLogLevel(hspd::LogLevel::FATAL)

#define ENABLE_LOG_FILE(FILE_PATH) \
    hspd::GlobalLogger::instance().setLogFile(FILE_PATH); \
    hspd::GlobalLogger::instance().setLogChoice(hspd::Choice::FILE) \


} // namespace hspd




#endif // LOG_HPP
