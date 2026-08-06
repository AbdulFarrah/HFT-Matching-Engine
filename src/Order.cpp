#include "Order.hpp"

Order::Order(uint64_t id,
        std::string clientId,
        std::string symbol,
        Side side,
        OrderType type,
        double price,
        uint64_t quantity,
        uint64_t timestamp)
    : id_(id),
    clientId_(std::move(clientId)),
    symbol_(std::move(symbol)),
    side_(side),
    type_(type),
    price_(price),
    quantity_(quantity),
    remainingQuantity_(quantity),
    timestamp_(timestamp)
{
}

uint64_t Order::getId() const
{
    return id_;
}

const std::string& Order::getClientId() const
{
    return clientId_;
}

const std::string& Order::getSymbol() const
{
    return symbol_;
}

Side Order::getSide() const
{
    return side_;
}

OrderType Order::getType() const
{
    return type_;
}

double Order::getPrice() const
{
    return price_;
}

uint64_t Order::getQuantity() const
{
    return quantity_;
}

uint64_t Order::getRemainingQuantity() const
{
    return remainingQuantity_;
}

uint64_t Order::getTimestamp() const
{
    return timestamp_;
}

void Order::setRemainingQuantity(uint64_t quantity)
{
    remainingQuantity_ = quantity;
}

void Order::setQuantity(uint64_t quantity)
{
    quantity_ = quantity;
}
