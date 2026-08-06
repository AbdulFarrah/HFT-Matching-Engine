#pragma once

#include <cstdint>
#include <ostream>
#include <string>

class OutputWriter
{
    public:
        explicit OutputWriter(std::ostream& output);

        void writeTrade(uint64_t buyOrderId,
                uint64_t sellOrderId,
                double price,
                uint64_t quantity);

        void beginSymbol(const std::string& symbol);

        void beginBuyOrders();

        void beginSellOrders();

        void writeOrder(uint64_t orderId,
                double price,
                uint64_t quantity);

    private:
        std::ostream& output_;
};
