#pragma once

#include <cstdint>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

class OutputWriter
{
    public:
        explicit OutputWriter(std::ostream& output);
        void setOutput(std::ostream& output);

        void writeTrade(uint64_t buyOrderId,
                uint64_t sellOrderId,
                double price,
                uint64_t quantity,
                uint64_t outputSequence = 0);

        void flushTrades();

        void beginSymbol(const std::string& symbol);

        void beginBuyOrders();

        void beginSellOrders();

        void writeOrder(uint64_t orderId,
                double price,
                uint64_t quantity);

    private:
        struct PendingTrade
        {
            uint64_t sequence;
            std::string line;
        };

        std::ostream* output_;
        std::mutex outputMutex_;
        std::vector<PendingTrade> pendingTrades_;
};
