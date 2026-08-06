#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include "Logger.hpp"

Logger::Logger()
{
    const std::filesystem::path logDirectory = "../logs";

    std::filesystem::create_directories(logDirectory);

    tradeLog_.open(logDirectory / "executions.csv",std::ios::out | std::ios::trunc);

    tradeLog_
        << "Timestamp,"
        << "BuyOrderId,"
        << "SellOrderId,"
        << "Symbol,"
        << "Price,"
        << "Quantity\n";

    debugLog_.open(logDirectory / "debug.log",std::ios::out | std::ios::trunc);
}

std::string Logger::currentTimestamp() const
{
    using namespace std::chrono;

    auto now = system_clock::now();

    auto time = system_clock::to_time_t(now);

    auto microseconds =
        duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()) % 1000000;

    std::tm tm{};
    localtime_r(&time, &tm);

    std::ostringstream stream;

    stream
        << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.'
        << std::setw(6)
        << std::setfill('0')
        << microseconds.count();

    return stream.str();
}

std::string Logger::logLevelToString(LogType type) const
{
    switch (type)
    {
        case LogType::DEBUG:
            return "DEBUG";

        case LogType::INFO:
            return "INFO";

        case LogType::WARNING:
            return "WARNING";

        case LogType::ERROR:
            return "ERROR";

        default:
            return "UNKNOWN";
    }
}

void Logger::logTrade(uint64_t buyOrderId,
        uint64_t sellOrderId,
        const std::string& symbol,
        double price,
        uint64_t quantity,
        const char* file,
        int line)
{
    if (!enabled_)
    {
        return;
    }
    std::lock_guard lock(logMutex_);
    tradeLog_
        << currentTimestamp() << ','
        << buyOrderId << ','
        << sellOrderId << ','
        << symbol << ','
        << price << ','
        << quantity
        << '\n';

    debugLog_
        << currentTimestamp()
        << " [INFO]"
        << " [" << fileName(file) << ':' << line << "] "
        << "TRADE BUY=" << buyOrderId
        << " SELL=" << sellOrderId
        << " SYMBOL=" << symbol
        << " PRICE=" << price
        << " QTY=" << quantity
        << '\n';
}

void Logger::logMessage(LogType type,
        const std::string& message,
        const char* file,
        int line)
{
    if (!enabled_)
    {
        return;
    }
    std::lock_guard lock(logMutex_);
    debugLog_
        << currentTimestamp()
        << " ["
        << logLevelToString(type)
        << "] "
        << "["
        << fileName(file)
        << ':'
        << line
        << "] "
        << message
        << '\n';
}

std::string Logger::fileName(const char* file) const
{
    return std::filesystem::path(file).filename().string();
}

void Logger::setEnabled(bool enabled)
{
    enabled_ = enabled;
}
