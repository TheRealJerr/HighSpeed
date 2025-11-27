#include <iostream>
#include <log/Log.hpp>

using namespace hspd;

int main() {
    // 创建 GlobalLogger 实例
    GlobalLogger& logger = GlobalLogger::instance();
    // 设置日志级别为 DEBUG
    logger.setLogLevel(LogLevel::DEBUG);
    // 设置日志输出方式为标准输出
    logger.setLogChoice(hspd::Choice::STDOUT);
    // 测试不同级别的日志输出
    logger.debug("This is a debug message with value: {}", 42);
    logger.info("This is an info message with value: {}", 42);
    logger.warn("This is a warning message with value: {}", 42);
    logger.error("This is an error message with value: {}", 42);
    logger.fatal("This is a fatal message with value: {}", 42);
    // 设置日志输出方式为文件输出，并指定文件路径
    logger.setLogChoice(hspd::Choice::FILE);
    logger.setFilePath("./test_log.txt");
    // 测试不同级别的日志输出到文件
    logger.debug("This is a debug message with value: {}", 42);
    logger.info("This is an info message with value: {}", 42);
    logger.warn("This is a warning message with value: {}", 42);
    logger.error("This is an error message with value: {}", 42);
    logger.fatal("This is a fatal message with value: {}", 42);
    // 设置日志级别为 INFO，测试是否过滤掉 DEBUG 级别的日志
    logger.setLogLevel(hspd::LogLevel::INFO);
    logger.debug("This debug message should not appear in the file.");
    logger.info("This info message should appear in the file.");
    std::cout << "All test messages have been sent to the logger." << std::endl;
    return 0;
}