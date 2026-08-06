#include "MarketDataPublisher.hpp"

#include <filesystem>

#include "OrderBook.hpp"

MarketDataPublisher::MarketDataPublisher(
        Logger& logger)
    : logger_(logger)
{
    const std::filesystem::path logDirectory = "../logs";

    std::filesystem::create_directories(logDirectory);

    marketDataLog_.open(logDirectory / "market_data.csv",std::ios::out | std::ios::trunc);

    marketDataLog_
        << "Symbol,"
        << "BestBid,"
        << "BidQuantity,"
        << "BestAsk,"
        << "AskQuantity\n";
}
void MarketDataPublisher::publish(const OrderBook& book)
{
    std::lock_guard lock(marketDataMutex_);
    double bestBid = 0.0;
    uint64_t bidQty = 0;

    if (!book.getBuyLevels().empty())
    {
        const auto& [price, orders] =
            *book.getBuyLevels().begin();

        bestBid = price;

        for (const auto& order : orders)
        {
            bidQty += order.getRemainingQuantity();
        }
    }

    double bestAsk = 0.0;
    uint64_t askQty = 0;

    if (!book.getSellLevels().empty())
    {
        const auto& [price, orders] =
            *book.getSellLevels().begin();

        bestAsk = price;

        for (const auto& order : orders)
        {
            askQty += order.getRemainingQuantity();
        }
    }

    marketDataLog_
        << book.getSymbol() << ','
        << bestBid << ','
        << bidQty << ','
        << bestAsk << ','
        << askQty
        << '\n';

    LOG_DEBUG(
            logger_,
            "Published market data for " +
            book.getSymbol() +
            " BID=" + std::to_string(bestBid) +
            " BID_QTY=" + std::to_string(bidQty) +
            " ASK=" + std::to_string(bestAsk) +
            " ASK_QTY=" + std::to_string(askQty));
}
