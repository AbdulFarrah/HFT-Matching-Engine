#pragma once

#include <list>
#include <map>
#include <functional>
#include <unordered_map>
#include <vector>

#include "Order.hpp"
#include "Logger.hpp"
#include "OutputWriter.hpp"
#include "MarketDataPublisher.hpp"

struct ProcessResult
{
    bool orderRested = false;
    std::vector<uint64_t> completedOrders;
};

using BuyBook =
std::map<double, std::list<Order>, std::greater<double>>;

using SellBook =
std::map<double, std::list<Order>>;

class OrderBook
{
    public:
        explicit OrderBook(
                const std::string& symbol,
                Logger& logger,
                OutputWriter& outputWriter,
                MarketDataPublisher& marketDataPublisher);
        ProcessResult processOrder(Order order);
        void cancelOrder(uint64_t orderId);
        Order* findOrder(uint64_t orderId);
        void writeBook() const;
        std::vector<Order> getBuyOrders() const;
        std::vector<Order> getSellOrders() const;
        void restoreOrder(const Order& order);
        const BuyBook& getBuyLevels() const;
        const SellBook& getSellLevels() const;
        const std::string& getSymbol() const;
        void notifyMarketDataUpdate();
        void clear();
    private:
        struct OrderLocation
        {
            Side side;
            double price;
            std::list<Order>::iterator orderIterator;
        };
        void matchBuyOrder(Order order,
                ProcessResult& result);

        void matchSellOrder(Order order,
                ProcessResult& result);
        void publishMarketData();
        BuyBook buyOrders_;
        SellBook sellOrders_;
        std::unordered_map<uint64_t, OrderLocation> orderLookup_;
        std::string symbol_;
        Logger& logger_;
        OutputWriter& outputWriter_;
        MarketDataPublisher& marketDataPublisher_;
};
