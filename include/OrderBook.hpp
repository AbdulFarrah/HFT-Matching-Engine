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
        ProcessResult processOrder(Order order, uint64_t outputSequence = 0);
        void cancelOrder(uint64_t orderId);
        const Order* findOrder(uint64_t orderId) const;
        void updateOrderQuantities(
                uint64_t orderId,
                uint64_t quantity,
                uint64_t remainingQuantity);
        void writeBook() const;
        std::vector<Order> getBuyOrders() const;
        std::vector<Order> getSellOrders() const;
        void restoreOrder(const Order& order);
        const BuyBook& getBuyLevels() const;
        const SellBook& getSellLevels() const;
        const std::string& getSymbol() const;
        uint64_t getBestBidQuantity() const;
        uint64_t getBestAskQuantity() const;
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
                ProcessResult& result,
                uint64_t outputSequence);

        void matchSellOrder(Order order,
                ProcessResult& result,
                uint64_t outputSequence);
        void addLevelQuantity(Side side, double price, uint64_t quantity);
        void removeLevelQuantity(Side side, double price, uint64_t quantity);
        void publishMarketData();
        BuyBook buyOrders_;
        SellBook sellOrders_;
        std::unordered_map<double, uint64_t> buyLevelQuantities_;
        std::unordered_map<double, uint64_t> sellLevelQuantities_;
        std::unordered_map<uint64_t, OrderLocation> orderLookup_;
        std::string symbol_;
        Logger& logger_;
        OutputWriter& outputWriter_;
        MarketDataPublisher& marketDataPublisher_;
};
