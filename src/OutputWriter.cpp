#include "OutputWriter.hpp"

#include <algorithm>
#include <sstream>

OutputWriter::OutputWriter(std::ostream& output)
    : output_(&output)
{
}

void OutputWriter::setOutput(std::ostream& output)
{
    std::lock_guard lock(outputMutex_);
    output_ = &output;
}

void OutputWriter::writeTrade(uint64_t buyOrderId,
        uint64_t sellOrderId,
        double price,
        uint64_t quantity,
        uint64_t outputSequence)
{
    std::ostringstream line;
    line
        << "TRADE BUY="
        << buyOrderId
        << " SELL="
        << sellOrderId
        << " PRICE="
        << price
        << " QTY="
        << quantity
        << '\n';

    std::lock_guard lock(outputMutex_);
    if (outputSequence == 0)
    {
        *output_ << line.str();
        return;
    }

    pendingTrades_.push_back({outputSequence, line.str()});
}

void OutputWriter::flushTrades()
{
    std::lock_guard lock(outputMutex_);
    std::stable_sort(
            pendingTrades_.begin(),
            pendingTrades_.end(),
            [](const PendingTrade& left, const PendingTrade& right)
            {
                return left.sequence < right.sequence;
            });

    for (const auto& trade : pendingTrades_)
    {
        *output_ << trade.line;
    }
    pendingTrades_.clear();
}

void OutputWriter::beginSymbol(const std::string& symbol)
{
    std::lock_guard lock(outputMutex_);
    *output_
        << "\n========== "
        << symbol
        << " ==========\n";
}

void OutputWriter::beginBuyOrders()
{
    std::lock_guard lock(outputMutex_);
    *output_ << "\nBUY ORDERS\n";
}

void OutputWriter::beginSellOrders()
{
    std::lock_guard lock(outputMutex_);
    *output_ << "\nSELL ORDERS\n";
}

void OutputWriter::writeOrder(uint64_t orderId,
        double price,
        uint64_t quantity)
{
    std::lock_guard lock(outputMutex_);
    *output_
        << orderId
        << ' '
        << price
        << ' '
        << quantity
        << '\n';
}
