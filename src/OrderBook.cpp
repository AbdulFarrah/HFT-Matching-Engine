#include "OrderBook.hpp"

#include <iostream>
#include <algorithm>
#include <cmath>

OrderBook::OrderBook(
        const std::string& symbol,
        Logger& logger,
        OutputWriter& outputWriter,
        MarketDataPublisher& marketDataPublisher)
    : symbol_(symbol),
    logger_(logger),
    outputWriter_(outputWriter),
    marketDataPublisher_(marketDataPublisher)
{
    LOG_DEBUG(
            logger_,
            "OrderBook created.");
}

ProcessResult OrderBook::processOrder(Order order)
{
    LOG_INFO(
            logger_,
            "Processing order ID=" + std::to_string(order.getId()) +
            " SYMBOL=" + order.getSymbol());

    ProcessResult result;

    if (order.getSide() == Side::BUY)
    {
        LOG_DEBUG(
                logger_,
                "Routing BUY order ID=" + std::to_string(order.getId()) +
                " to matchBuyOrder().");

        matchBuyOrder(order, result);
    }
    else
    {
        LOG_DEBUG(
                logger_,
                "Routing SELL order ID=" + std::to_string(order.getId()) +
                " to matchSellOrder().");

        matchSellOrder(order, result);
    }

    result.orderRested = (findOrder(order.getId()) != nullptr);

    if (result.orderRested)
    {
        LOG_INFO(
                logger_,
                "Order ID=" + std::to_string(order.getId()) +
                " added to the order book.");
    }

    return result;
}

void OrderBook::restoreOrder(const Order& order)
{
    LOG_DEBUG(
            logger_,
            "Restoring order ID=" +
            std::to_string(order.getId()));
    if (order.getSide() == Side::BUY)
    {
        auto& orders = buyOrders_[order.getPrice()];

        orders.push_back(order);

        auto it = std::prev(orders.end());

        orderLookup_[order.getId()] =
        {
            Side::BUY,
            order.getPrice(),
            it
        };
    }
    else
    {
        auto& orders = sellOrders_[order.getPrice()];

        orders.push_back(order);

        auto it = std::prev(orders.end());

        orderLookup_[order.getId()] =
        {
            Side::SELL,
            order.getPrice(),
            it
        };
    }

    LOG_INFO(
            logger_,
            "Restored order ID=" +
            std::to_string(order.getId()) +
            " SYMBOL=" +
            order.getSymbol());
}

void OrderBook::cancelOrder(uint64_t orderId)
{
    LOG_INFO(
            logger_,
            "Cancelling order ID=" + std::to_string(orderId));

    auto lookup = orderLookup_.find(orderId);

    if (lookup == orderLookup_.end())
    {
        LOG_WARNING(
                logger_,
                "Cancel ignored. Unknown order ID=" +
                std::to_string(orderId));

        return;
    }

    const OrderLocation& location = lookup->second;

    if (location.side == Side::BUY)
    {
        auto level = buyOrders_.find(location.price);

        if (level != buyOrders_.end())
        {
            level->second.erase(location.orderIterator);

            LOG_DEBUG(
                    logger_,
                    "Removed BUY order ID=" +
                    std::to_string(orderId));

            if (level->second.empty())
            {
                buyOrders_.erase(level);

                LOG_DEBUG(
                        logger_,
                        "Removed empty BUY price level " +
                        std::to_string(location.price));
            }
        }
    }
    else
    {
        auto level = sellOrders_.find(location.price);

        if (level != sellOrders_.end())
        {
            level->second.erase(location.orderIterator);

            LOG_DEBUG(
                    logger_,
                    "Removed SELL order ID=" +
                    std::to_string(orderId));

            if (level->second.empty())
            {
                sellOrders_.erase(level);

                LOG_DEBUG(
                        logger_,
                        "Removed empty SELL price level " +
                        std::to_string(location.price));
            }
        }
    }

    orderLookup_.erase(lookup);
    publishMarketData();
    LOG_INFO(
            logger_,
            "Successfully cancelled order ID=" +
            std::to_string(orderId));
}

Order* OrderBook::findOrder(uint64_t orderId)
{
    auto lookup = orderLookup_.find(orderId);
    if (lookup == orderLookup_.end())
    {
        return nullptr;
    }


    return &(*lookup->second.orderIterator);
}

void OrderBook::matchBuyOrder(Order order, ProcessResult& result)
{
    bool bookChanged = false;
    LOG_DEBUG(
            logger_,
            "Attempting to match BUY order ID=" +
            std::to_string(order.getId()));

    for (auto sellLevel = sellOrders_.begin();
            sellLevel != sellOrders_.end() &&
            order.getRemainingQuantity() > 0; )
    {
        LOG_DEBUG(
                logger_,
                "Checking SELL price level " +
                std::to_string(sellLevel->first));

        if (order.getType() == OrderType::LIMIT &&
                order.getPrice() < sellLevel->first)
        {
            LOG_DEBUG(
                    logger_,
                    "Best SELL price exceeds BUY limit price. Stopping matching.");

            break;
        }

        auto& orders = sellLevel->second;

        auto resting = orders.begin();

        while (resting != orders.end() &&
                resting->getClientId() == order.getClientId())
        {
            LOG_DEBUG(
                    logger_,
                    "Skipping self-trade with order ID=" +
                    std::to_string(resting->getId()));

            ++resting;
        }

        // Entire price level belongs to the same client.
        if (resting == orders.end())
        {
            LOG_DEBUG(
                    logger_,
                    "Entire SELL price level skipped due to self-trade prevention.");

            ++sellLevel;
            continue;
        }

        uint64_t tradedQuantity = std::min(
                order.getRemainingQuantity(),
                resting->getRemainingQuantity());

        outputWriter_.writeTrade(
                order.getId(),
                resting->getId(),
                resting->getPrice(),
                tradedQuantity);

        LOG_TRADE(
                logger_,
                order.getId(),
                resting->getId(),
                resting->getSymbol(),
                resting->getPrice(),
                tradedQuantity);

        order.setRemainingQuantity(
                order.getRemainingQuantity() - tradedQuantity);
        resting->setRemainingQuantity(
                resting->getRemainingQuantity() - tradedQuantity);
        bookChanged = true;

        if (order.getRemainingQuantity() > 0)
        {
            LOG_INFO(
                    logger_,
                    "BUY order ID=" +
                    std::to_string(order.getId()) +
                    " partially filled. Remaining quantity=" +
                    std::to_string(order.getRemainingQuantity()));
        }

        if (resting->getRemainingQuantity() == 0)
        {
            LOG_INFO(
                    logger_,
                    "SELL order ID=" +
                    std::to_string(resting->getId()) +
                    " completely filled.");

            result.completedOrders.push_back(resting->getId());
            orderLookup_.erase(resting->getId());
            orders.erase(resting);
        }

        if (orders.empty())
        {
            LOG_DEBUG(
                    logger_,
                    "SELL price level exhausted and removed.");

            sellLevel = sellOrders_.erase(sellLevel);
        }
    }

    if (order.getRemainingQuantity() > 0 &&
            order.getType() == OrderType::LIMIT)
    {
        LOG_INFO(
                logger_,
                "BUY order ID=" +
                std::to_string(order.getId()) +
                " resting with quantity=" +
                std::to_string(order.getRemainingQuantity()));

        auto& orders = buyOrders_[order.getPrice()];
        orders.push_back(order);
        bookChanged = true;
        auto it = std::prev(orders.end());

        orderLookup_[order.getId()] =
        {
            Side::BUY,
            order.getPrice(),
            it
        };
    }
    else if (order.getRemainingQuantity() > 0)
    {
        LOG_INFO(
                logger_,
                "Unfilled quantity of BUY order ID=" +
                std::to_string(order.getId()) +
                " discarded (" +
                (order.getType() == OrderType::MARKET ? "MARKET" : "IOC") +
                " order).");
    }
    if (bookChanged)
    {
        publishMarketData();
    }
}

void OrderBook::matchSellOrder(Order order,ProcessResult& result)
{
    bool bookChanged = false;
    LOG_DEBUG(
            logger_,
            "Attempting to match SELL order ID=" +
            std::to_string(order.getId()));

    for (auto buyLevel = buyOrders_.begin();
            buyLevel != buyOrders_.end() &&
            order.getRemainingQuantity() > 0; )
    {
        LOG_DEBUG(
                logger_,
                "Checking BUY price level " +
                std::to_string(buyLevel->first));

        if (order.getType() == OrderType::LIMIT &&
                buyLevel->first < order.getPrice())
        {
            LOG_DEBUG(
                    logger_,
                    "Best BUY price below SELL limit price. Stopping matching.");

            break;
        }

        auto& orders = buyLevel->second;

        auto resting = orders.begin();

        while (resting != orders.end() &&
                resting->getClientId() == order.getClientId())
        {
            LOG_DEBUG(
                    logger_,
                    "Skipping self-trade with order ID=" +
                    std::to_string(resting->getId()));

            ++resting;
        }

        // Entire price level belongs to the same client.
        if (resting == orders.end())
        {
            LOG_DEBUG(
                    logger_,
                    "Entire BUY price level skipped due to self-trade prevention.");

            ++buyLevel;
            continue;
        }

        uint64_t tradedQuantity = std::min(
                order.getRemainingQuantity(),
                resting->getRemainingQuantity());

        outputWriter_.writeTrade(
                resting->getId(),
                order.getId(),
                resting->getPrice(),
                tradedQuantity);

        LOG_TRADE(
                logger_,
                resting->getId(),
                order.getId(),
                resting->getSymbol(),
                resting->getPrice(),
                tradedQuantity);

        order.setRemainingQuantity(
                order.getRemainingQuantity() - tradedQuantity);

        resting->setRemainingQuantity(
                resting->getRemainingQuantity() - tradedQuantity);
        bookChanged = true;
        if (order.getRemainingQuantity() > 0)
        {
            LOG_INFO(
                    logger_,
                    "SELL order ID=" +
                    std::to_string(order.getId()) +
                    " partially filled. Remaining quantity=" +
                    std::to_string(order.getRemainingQuantity()));
        }
        if (resting->getRemainingQuantity() == 0)
        {
            LOG_INFO(
                    logger_,
                    "BUY order ID=" +
                    std::to_string(resting->getId()) +
                    " completely filled.");

            result.completedOrders.push_back(resting->getId());
            orderLookup_.erase(resting->getId());
            orders.erase(resting);
        }

        if (orders.empty())
        {
            LOG_DEBUG(
                    logger_,
                    "BUY price level exhausted and removed.");

            buyLevel = buyOrders_.erase(buyLevel);
        }
    }

    if (order.getRemainingQuantity() > 0 &&
            order.getType() == OrderType::LIMIT)
    {
        LOG_INFO(
                logger_,
                "SELL order ID=" +
                std::to_string(order.getId()) +
                " resting with quantity=" +
                std::to_string(order.getRemainingQuantity()));

        auto& orders = sellOrders_[order.getPrice()];
        orders.push_back(order);
        bookChanged = true;
        auto it = std::prev(orders.end());

        orderLookup_[order.getId()] =
        {
            Side::SELL,
            order.getPrice(),
            it
        };
    }
    else if (order.getRemainingQuantity() > 0)
    {
        LOG_INFO(
                logger_,
                "Unfilled quantity of SELL order ID=" +
                std::to_string(order.getId()) +
                " discarded (" +
                (order.getType() == OrderType::MARKET ? "MARKET" : "IOC") +
                " order).");
    }
    if (bookChanged)
    {
        publishMarketData();
    }
}

void OrderBook::writeBook() const
{
    LOG_DEBUG(
            logger_,
            "Printing order book.");

    outputWriter_.beginBuyOrders();

    if (buyOrders_.empty())
    {
        LOG_DEBUG(logger_, "BUY book is empty.");
    }

    for (const auto& [price, orders] : buyOrders_)
    {
        LOG_DEBUG(
                logger_,
                "BUY level PRICE=" +
                std::to_string(price));

        for (const auto& order : orders)
        {
            LOG_DEBUG(
                    logger_,
                    "BUY ORDER ID=" +
                    std::to_string(order.getId()));

            outputWriter_.writeOrder(
                    order.getId(),
                    order.getPrice(),
                    order.getRemainingQuantity());
        }
    }

    outputWriter_.beginSellOrders();

    if (sellOrders_.empty())
    {
        LOG_DEBUG(logger_, "SELL book is empty.");
    }

    for (const auto& [price, orders] : sellOrders_)
    {
        LOG_DEBUG(
                logger_,
                "SELL level PRICE=" +
                std::to_string(price));

        for (const auto& order : orders)
        {
            LOG_DEBUG(
                    logger_,
                    "SELL ORDER ID=" +
                    std::to_string(order.getId()));

            outputWriter_.writeOrder(
                    order.getId(),
                    order.getPrice(),
                    order.getRemainingQuantity());
        }
    }

    LOG_DEBUG(
            logger_,
            "Finished printing order book.");
}

std::vector<Order> OrderBook::getBuyOrders() const
{
    std::vector<Order> orders;

    for (const auto& [price, level] : buyOrders_)
    {
        for (const auto& order : level)
        {
            orders.push_back(order);
        }
    }

    return orders;
}

std::vector<Order> OrderBook::getSellOrders() const
{
    std::vector<Order> orders;

    for (const auto& [price, level] : sellOrders_)
    {
        for (const auto& order : level)
        {
            orders.push_back(order);
        }
    }

    return orders;
}

const BuyBook& OrderBook::getBuyLevels() const
{
    return buyOrders_;
}

const SellBook& OrderBook::getSellLevels() const
{
    return sellOrders_;
}

const std::string& OrderBook::getSymbol() const
{
    return symbol_;
}

void OrderBook::publishMarketData()
{
    marketDataPublisher_.publish(*this);
}

void OrderBook::notifyMarketDataUpdate()
{
    publishMarketData();
}

void OrderBook::clear()
{
    buyOrders_.clear();
    sellOrders_.clear();
    orderLookup_.clear();
}
