#include "OutputWriter.hpp"

OutputWriter::OutputWriter(std::ostream& output)
    : output_(output)
{
}

void OutputWriter::writeTrade(uint64_t buyOrderId,
        uint64_t sellOrderId,
        double price,
        uint64_t quantity)
{
    output_
        << "TRADE BUY="
        << buyOrderId
        << " SELL="
        << sellOrderId
        << " PRICE="
        << price
        << " QTY="
        << quantity
        << '\n';
}

void OutputWriter::beginSymbol(const std::string& symbol)
{
    output_
        << "\n========== "
        << symbol
        << " ==========\n";
}

void OutputWriter::beginBuyOrders()
{
    output_ << "\nBUY ORDERS\n";
}

void OutputWriter::beginSellOrders()
{
    output_ << "\nSELL ORDERS\n";
}

void OutputWriter::writeOrder(uint64_t orderId,
        double price,
        uint64_t quantity)
{
    output_
        << orderId
        << ' '
        << price
        << ' '
        << quantity
        << '\n';
}
