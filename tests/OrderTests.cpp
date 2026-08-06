#include <gtest/gtest.h>

#include "Order.hpp"

namespace
{
    Order createOrder(
            uint64_t id = 1,
            const std::string& clientId = "CLIENT1",
            const std::string& symbol = "AAPL",
            Side side = Side::BUY,
            OrderType type = OrderType::LIMIT,
            double price = 100.0,
            uint64_t quantity = 10,
            uint64_t timestamp = 123)
    {
        return Order(
                id,
                clientId,
                symbol,
                side,
                type,
                price,
                quantity,
                timestamp);
    }
}

TEST(OrderTest, ConstructorInitializesAllFields)
{
    Order order = createOrder();

    EXPECT_EQ(order.getId(), 1);
    EXPECT_EQ(order.getClientId(), "CLIENT1");
    EXPECT_EQ(order.getSymbol(), "AAPL");
    EXPECT_EQ(order.getSide(), Side::BUY);
    EXPECT_EQ(order.getType(), OrderType::LIMIT);
    EXPECT_DOUBLE_EQ(order.getPrice(), 100.0);
    EXPECT_EQ(order.getQuantity(), 10);
    EXPECT_EQ(order.getRemainingQuantity(), 10);
    EXPECT_EQ(order.getTimestamp(), 123);
}

TEST(OrderTest, RemainingQuantityInitiallyEqualsQuantity)
{
    Order order = createOrder(
            1,
            "CLIENT1",
            "AAPL",
            Side::BUY,
            OrderType::LIMIT,
            100.0,
            25);

    EXPECT_EQ(order.getQuantity(), 25);
    EXPECT_EQ(order.getRemainingQuantity(), 25);
}

TEST(OrderTest, SetRemainingQuantity)
{
    Order order = createOrder();

    order.setRemainingQuantity(6);

    EXPECT_EQ(order.getQuantity(), 10);
    EXPECT_EQ(order.getRemainingQuantity(), 6);
}

TEST(OrderTest, SetQuantity)
{
    Order order = createOrder();

    order.setQuantity(20);

    EXPECT_EQ(order.getQuantity(), 20);
    EXPECT_EQ(order.getRemainingQuantity(), 10);
}

TEST(OrderTest, QuantityAndRemainingAreIndependent)
{
    Order order = createOrder();

    order.setQuantity(20);
    order.setRemainingQuantity(5);

    EXPECT_EQ(order.getQuantity(), 20);
    EXPECT_EQ(order.getRemainingQuantity(), 5);
}

TEST(OrderTest, BuyOrder)
{
    Order order = createOrder();

    EXPECT_EQ(order.getSide(), Side::BUY);
}

TEST(OrderTest, SellOrder)
{
    Order order = createOrder(
            1,
            "CLIENT1",
            "AAPL",
            Side::SELL);

    EXPECT_EQ(order.getSide(), Side::SELL);
}

TEST(OrderTest, OrderTypes)
{
    EXPECT_EQ(
            createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::BUY,
                OrderType::LIMIT).getType(),
            OrderType::LIMIT);

    EXPECT_EQ(
            createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::BUY,
                OrderType::MARKET).getType(),
            OrderType::MARKET);

    EXPECT_EQ(
            createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::BUY,
                OrderType::IOC).getType(),
            OrderType::IOC);
}
