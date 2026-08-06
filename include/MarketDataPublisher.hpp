#pragma once

#include <fstream>
#include <string>
#include <mutex>

#include "Logger.hpp"

class OrderBook;

class MarketDataPublisher
{
    public:
        explicit MarketDataPublisher(Logger& logger);
        void publish(const OrderBook& book);

    private:
        Logger& logger_;
        std::ofstream marketDataLog_;
        std::mutex marketDataMutex_;
};
