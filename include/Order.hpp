#pragma once

#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

enum class Side
{
    BUY,
    SELL
};

enum class OrderType
{
    LIMIT,
    MARKET,
    IOC
};

class Order
{
    public:
        Order(uint64_t id,
                std::string clientId,
                std::string symbol,
                Side side,
                OrderType type,
                double price,
                uint64_t quantity,
                uint64_t timestamp);

        uint64_t getId() const;
        const std::string& getClientId() const;
        const std::string& getSymbol() const;
        Side getSide() const;
        OrderType getType() const;
        double getPrice() const;
        uint64_t getQuantity() const;
        uint64_t getRemainingQuantity() const;
        uint64_t getTimestamp() const;
        void setRemainingQuantity(uint64_t quantity);
        void setQuantity(uint64_t quantity);

    private:
        uint64_t id_;
        std::string clientId_;
        std::string symbol_;
        Side side_;
        OrderType type_;
        double price_;
        uint64_t quantity_;
        uint64_t remainingQuantity_;
        uint64_t timestamp_;
};

inline void to_json(
        nlohmann::json& j,
        const Order& order)
{
    j =
    {
        {"id", order.getId()},
        {"clientId", order.getClientId()},
        {"symbol", order.getSymbol()},
        {"side",
            order.getSide() == Side::BUY ?
                "BUY" : "SELL"},
        {"type",
            order.getType() == OrderType::LIMIT ?
                "LIMIT" :
                order.getType() == OrderType::MARKET ?
                "MARKET" :
                "IOC"},
        {"price", order.getPrice()},
        {"quantity", order.getQuantity()},
        {"remainingQuantity",
            order.getRemainingQuantity()},
        {"timestamp", order.getTimestamp()}
    };
}

inline void from_json(
        const nlohmann::json& j,
        Order& order)
{
    Side side =
        j.at("side") == "BUY"
        ? Side::BUY
        : Side::SELL;

    OrderType type;

    std::string orderType = j.at("type");

    if (orderType == "LIMIT")
    {
        type = OrderType::LIMIT;
    }
    else if (orderType == "MARKET")
    {
        type = OrderType::MARKET;
    }
    else
    {
        type = OrderType::IOC;
    }

    order = Order(
            j.at("id"),
            j.at("clientId"),
            j.at("symbol"),
            side,
            type,
            j.at("price"),
            j.at("quantity"),
            j.at("timestamp"));

    order.setRemainingQuantity(
            j.at("remainingQuantity"));
}
