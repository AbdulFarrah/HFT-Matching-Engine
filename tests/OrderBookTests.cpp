#include <gtest/gtest.h>
#include <sstream>

#include "OrderBook.hpp"
#include "Logger.hpp"

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
            uint64_t timestamp = 1)
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
    std::ostringstream output;
}

TEST(OrderBookTest, LimitOrderRests)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    Order order = createOrder();

    ProcessResult result = book.processOrder(order);

    EXPECT_TRUE(result.orderRested);
    EXPECT_TRUE(result.completedOrders.empty());

    Order* resting = book.findOrder(1);

    ASSERT_NE(resting, nullptr);

    EXPECT_EQ(resting->getId(), 1);
    EXPECT_EQ(resting->getRemainingQuantity(), 10);
    EXPECT_EQ(resting->getPrice(), 100.0);
}

TEST(OrderBookTest, LimitOrderMatches)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::SELL));

    ProcessResult result = book.processOrder(createOrder(
                2,
                "CLIENT2",
                "AAPL",
                Side::BUY));

    EXPECT_FALSE(result.orderRested);
    ASSERT_EQ(result.completedOrders.size(), 1);
    EXPECT_EQ(result.completedOrders[0], 1);
    EXPECT_EQ(book.findOrder(1), nullptr);
    EXPECT_EQ(book.findOrder(2), nullptr);
}

TEST(OrderBookTest, PricePriority)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::SELL,
                OrderType::LIMIT,
                101.0));

    book.processOrder(createOrder(
                2,
                "CLIENT2",
                "AAPL",
                Side::SELL,
                OrderType::LIMIT,
                100.0));

    book.processOrder(createOrder(
                3,
                "CLIENT3",
                "AAPL",
                Side::BUY,
                OrderType::LIMIT,
                101.0));

    EXPECT_EQ(book.findOrder(2), nullptr);
    EXPECT_NE(book.findOrder(1), nullptr);
}

TEST(OrderBookTest, FIFO)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::SELL));

    book.processOrder(createOrder(
                2,
                "CLIENT2",
                "AAPL",
                Side::SELL));

    book.processOrder(createOrder(
                3,
                "CLIENT3",
                "AAPL",
                Side::BUY));

    EXPECT_EQ(book.findOrder(1), nullptr);
    EXPECT_NE(book.findOrder(2), nullptr);
}

TEST(OrderBookTest, PartialFill)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::SELL,
                OrderType::LIMIT,
                100.0,
                10));

    ProcessResult result = book.processOrder(createOrder(
                2,
                "CLIENT2",
                "AAPL",
                Side::BUY,
                OrderType::LIMIT,
                100.0,
                4));

    EXPECT_FALSE(result.orderRested);

    Order* remaining = book.findOrder(1);

    ASSERT_NE(remaining, nullptr);

    EXPECT_EQ(remaining->getRemainingQuantity(), 6);

    EXPECT_EQ(book.findOrder(2), nullptr);
}

TEST(OrderBookTest, MultipleFills)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::SELL,
                OrderType::LIMIT,
                100.0,
                5));

    book.processOrder(createOrder(
                2,
                "CLIENT2",
                "AAPL",
                Side::SELL,
                OrderType::LIMIT,
                100.0,
                5));

    ProcessResult result = book.processOrder(createOrder(
                3,
                "CLIENT3",
                "AAPL",
                Side::BUY,
                OrderType::LIMIT,
                100.0,
                10));

    EXPECT_FALSE(result.orderRested);

    EXPECT_EQ(book.findOrder(1), nullptr);
    EXPECT_EQ(book.findOrder(2), nullptr);
    EXPECT_EQ(book.findOrder(3), nullptr);
    ASSERT_EQ(result.completedOrders.size(), 2);

    EXPECT_EQ(result.completedOrders[0], 1);
    EXPECT_EQ(result.completedOrders[1], 2);
}

TEST(OrderBookTest, CompleteFill)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::SELL,
                OrderType::LIMIT,
                100.0,
                10));

    ProcessResult result = book.processOrder(createOrder(
                2,
                "CLIENT2",
                "AAPL",
                Side::BUY,
                OrderType::LIMIT,
                100.0,
                10));

    EXPECT_FALSE(result.orderRested);

    EXPECT_EQ(book.findOrder(1), nullptr);
    EXPECT_EQ(book.findOrder(2), nullptr);

    ASSERT_EQ(result.completedOrders.size(), 1);
}

TEST(OrderBookTest, MarketOrderConsumesLiquidity)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::SELL,
                OrderType::LIMIT,
                100.0,
                5));

    ProcessResult result = book.processOrder(createOrder(
                2,
                "CLIENT2",
                "AAPL",
                Side::BUY,
                OrderType::MARKET,
                0.0,
                5));

    EXPECT_FALSE(result.orderRested);

    EXPECT_EQ(book.findOrder(1), nullptr);
    EXPECT_EQ(book.findOrder(2), nullptr);

    ASSERT_EQ(result.completedOrders.size(), 1);
}

TEST(OrderBookTest, MarketOrderCancelsRemainingQuantity)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::SELL,
                OrderType::LIMIT,
                100.0,
                5));

    ProcessResult result = book.processOrder(createOrder(
                2,
                "CLIENT2",
                "AAPL",
                Side::BUY,
                OrderType::MARKET,
                0.0,
                10));

    EXPECT_FALSE(result.orderRested);

    EXPECT_EQ(book.findOrder(1), nullptr);
    EXPECT_EQ(book.findOrder(2), nullptr);

    ASSERT_EQ(result.completedOrders.size(), 1);
}

TEST(OrderBookTest, IOCOrderCancelsRemainingQuantity)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::SELL,
                OrderType::LIMIT,
                100.0,
                4));

    ProcessResult result = book.processOrder(createOrder(
                2,
                "CLIENT2",
                "AAPL",
                Side::BUY,
                OrderType::IOC,
                100.0,
                10));

    EXPECT_FALSE(result.orderRested);

    EXPECT_EQ(book.findOrder(1), nullptr);
    EXPECT_EQ(book.findOrder(2), nullptr);

    ASSERT_EQ(result.completedOrders.size(), 1);
}

TEST(OrderBookTest, CancelExistingOrder)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder());

    ASSERT_NE(book.findOrder(1), nullptr);

    book.cancelOrder(1);

    EXPECT_EQ(book.findOrder(1), nullptr);
}

TEST(OrderBookTest, CancelUnknownOrder)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    EXPECT_NO_THROW(book.cancelOrder(999));
}

TEST(OrderBookTest, FindExistingOrder)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder());

    Order* order = book.findOrder(1);

    ASSERT_NE(order, nullptr);

    EXPECT_EQ(order->getId(), 1);
    EXPECT_EQ(order->getClientId(), "CLIENT1");
    EXPECT_EQ(order->getSymbol(), "AAPL");
}

TEST(OrderBookTest, FindUnknownOrder)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    EXPECT_EQ(book.findOrder(999), nullptr);
}

TEST(OrderBookTest, ProcessResultForRestingOrder)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    ProcessResult result =
        book.processOrder(createOrder());

    EXPECT_TRUE(result.orderRested);
    EXPECT_TRUE(result.completedOrders.empty());
}

TEST(OrderBookTest, ProcessResultCompletedOrders)
{
    Logger logger;
    OutputWriter outputwriter(output);
    MarketDataPublisher marketDataPublisher(logger);
    OrderBook book(
            "AAPL",
            logger,
            outputwriter,
            marketDataPublisher);
    book.processOrder(createOrder(
                1,
                "CLIENT1",
                "AAPL",
                Side::SELL));

    ProcessResult result =
        book.processOrder(createOrder(
                    2,
                    "CLIENT2",
                    "AAPL",
                    Side::BUY));

    EXPECT_FALSE(result.orderRested);

    ASSERT_EQ(result.completedOrders.size(), 1);

    EXPECT_EQ(result.completedOrders[0], 1);
}
