#pragma once

#include <cstdint>
#include <atomic>
#include <fstream>
#include <string>
#include <mutex>

enum class LogType
{
    TRADE,
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger
{
    public:
        Logger();

        void logTrade(uint64_t buyOrderId,
                uint64_t sellOrderId,
                const std::string& symbol,
                double price,
                uint64_t quantity,
                const char* file,
                int line);

        void logMessage(LogType type,
                const std::string& message,
                const char* file,
                int line);
        void setEnabled(bool enabled);
    private:
        std::string currentTimestamp() const;
        std::string logLevelToString(LogType type) const;
        std::string fileName(const char* file) const;
        std::ofstream tradeLog_;
        std::ofstream debugLog_;
        std::mutex logMutex_;
        std::atomic<bool> enabled_{true};
};

#define LOG_DEBUG(logger, message) \
    (logger).logMessage(LogType::DEBUG, message, __FILE__, __LINE__)

#define LOG_INFO(logger, message) \
    (logger).logMessage(LogType::INFO, message, __FILE__, __LINE__)

#define LOG_WARNING(logger, message) \
    (logger).logMessage(LogType::WARNING, message, __FILE__, __LINE__)

#define LOG_ERROR(logger, message) \
    (logger).logMessage(LogType::ERROR, message, __FILE__, __LINE__)

#define LOG_TRADE(logger, buyId, sellId, symbol, price, qty) \
    (logger).logTrade(buyId, sellId, symbol, price, qty, __FILE__, __LINE__)
