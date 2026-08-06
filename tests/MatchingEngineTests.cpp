#include <gtest/gtest.h>

#include <sstream>

#include "MatchingEngine.hpp"

namespace
{
    std::string runEngine(const std::string& inputText)
    {
        std::istringstream input(inputText);
        std::ostringstream output;

        {
            Logger logger;
            OutputWriter outputWriter(output);
            SnapshotManager snapshotManager("snapshot.json");
            MarketDataPublisher marketDataPublisher(logger);

            MatchingEngine engine(
                    logger,
                    outputWriter,
                    snapshotManager,
                    marketDataPublisher);

            engine.processOrders(input, output);
        }   // <-- engine is destroyed here

        return output.str();
    }
}

TEST(MatchingEngineTest, InvalidOrderType)
{
    EXPECT_TRUE(runEngine(R"(
HELLO BUY 1 CLIENT1 AAPL 100.00 10
)").empty());
}

TEST(MatchingEngineTest, InvalidSide)
{
    EXPECT_TRUE(runEngine(R"(
LIMIT HOLD 1 CLIENT1 AAPL 100.00 10
)").empty());
}

TEST(MatchingEngineTest, InvalidQuantity)
{
    EXPECT_TRUE(runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 0
)").empty());

}

TEST(MatchingEngineTest, InvalidPrice)
{
    EXPECT_TRUE(runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL -10.00 5
)").empty());

}

TEST(MatchingEngineTest, UnknownSymbol)
{
    EXPECT_TRUE(runEngine(R"(
LIMIT BUY 1 CLIENT1 TSLA 100.00 5
)").empty());

}

TEST(MatchingEngineTest, MalformedInput)
{
    EXPECT_TRUE(runEngine(R"(
LIMIT BUY
THIS IS NOT AN ORDER
LIMIT BUY ABC CLIENT1 AAPL 100.00 10
)").empty());
}

TEST(MatchingEngineTest, DuplicateOrderId)
{
    std::string output = runEngine(R"(
LIMIT BUY 100 CLIENT1 AAPL 100.00 10
LIMIT BUY 100 CLIENT2 MSFT 100.00 10
)");

    EXPECT_NE(output.find("========== AAPL =========="), std::string::npos);
    EXPECT_EQ(output.find("========== MSFT =========="), std::string::npos);
}

TEST(MatchingEngineTest, CancelUnknownOrder)
{
    EXPECT_TRUE(runEngine(R"(
CANCEL 999
)").empty());
}

TEST(MatchingEngineTest, ModifyUnknownOrder)
{
    EXPECT_TRUE(runEngine(R"(
MODIFY 999 PRICE 120.00
)").empty());
}

TEST(MatchingEngineTest, CancelRouting)
{
    std::string output = runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10
LIMIT BUY 2 CLIENT2 MSFT 200.00 20

CANCEL MSFT 2
)");


    EXPECT_NE(output.find("========== AAPL =========="), std::string::npos);
    EXPECT_NE(output.find("1 100 10"), std::string::npos);

    EXPECT_NE(output.find("========== MSFT =========="), std::string::npos);
    EXPECT_EQ(output.find("2 200 20"), std::string::npos);
}

TEST(MatchingEngineTest, ModifyRouting)
{
    std::string output = runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10
LIMIT BUY 2 CLIENT2 MSFT 200.00 20

MODIFY MSFT 2 PRICE 205.00
)");
    EXPECT_NE(output.find("2 205 20"), std::string::npos);
    EXPECT_NE(output.find("1 100 10"), std::string::npos);
}

TEST(MatchingEngineTest, ModifyPriceResetsPriority)
{
    std::string output = runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10
LIMIT BUY 2 CLIENT2 AAPL 101.00 10

MODIFY 1 PRICE 101.00

MARKET SELL 3 CLIENT3 AAPL 999.00 15
)");

    EXPECT_NE(output.find("TRADE BUY=2"), std::string::npos);
    EXPECT_NE(output.find("TRADE BUY=1"), std::string::npos);

    EXPECT_LT(
            output.find("TRADE BUY=2"),
            output.find("TRADE BUY=1"));
}

TEST(MatchingEngineTest, ModifyQuantityDecreaseKeepsPriority)
{
    std::string output = runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10
LIMIT BUY 2 CLIENT2 AAPL 100.00 10

MODIFY 1 QUANTITY 5

MARKET SELL 3 CLIENT3 AAPL 999.00 8
)");

    EXPECT_NE(output.find("TRADE BUY=1"), std::string::npos);
    EXPECT_LT(
            output.find("TRADE BUY=1"),
            output.find("TRADE BUY=2"));
}

TEST(MatchingEngineTest, ModifyQuantityIncreaseResetsPriority)
{
    std::string output = runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10
LIMIT BUY 2 CLIENT2 AAPL 100.00 10

MODIFY AAPL 1 QUANTITY 20

MARKET SELL 3 CLIENT3 AAPL 999.00 15
)");
    EXPECT_EQ(output.find("2 100 10"), std::string::npos);

    EXPECT_NE(output.find("1 100 15"), std::string::npos);
}

TEST(MatchingEngineTest, InvalidQuantityModification)
{
    std::string output = runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10

MARKET SELL 2 CLIENT2 AAPL 999.00 6

MODIFY AAPL 1 QUANTITY 5
)");

    EXPECT_NE(output.find("1 100 4"), std::string::npos);
}

TEST(MatchingEngineTest, DuplicateOrderIdAcrossSymbols)
{
    std::string output = runEngine(R"(
LIMIT BUY 100 CLIENT1 AAPL 100.00 10
LIMIT BUY 100 CLIENT2 MSFT 200.00 20
)");

    EXPECT_NE(output.find("========== AAPL =========="), std::string::npos);
    EXPECT_EQ(output.find("========== MSFT =========="), std::string::npos);
}

TEST(MatchingEngineTest, SelfTradePrevention)
{
    std::string output = runEngine(R"(
LIMIT SELL 1 CLIENT1 GOOG 100.00 10
LIMIT BUY 2 CLIENT1 GOOG 100.00 10
)");

    // No trade should occur.
    EXPECT_EQ(output.find("TRADE"), std::string::npos);

    // Both orders should still remain.
    EXPECT_NE(output.find("2 100 10"), std::string::npos);
    EXPECT_NE(output.find("1 100 10"), std::string::npos);
}

TEST(MatchingEngineTest, SnapshotRecoveryRestoresOrderBook)
{
    std::istringstream input(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10
LIMIT SELL 2 CLIENT2 MSFT 200.00 20
)");

    std::ostringstream output;

    Logger logger;
    OutputWriter writer(output);
    SnapshotManager snapshot("snapshot_test.json");
    MarketDataPublisher publisher(logger);

    MatchingEngine engine(
            logger,
            writer,
            snapshot,
            publisher);

    engine.processOrders(input, output);

    std::ostringstream recoveredOutput;

    Logger logger2;
    OutputWriter writer2(recoveredOutput);
    SnapshotManager snapshot2("snapshot_test.json");
    MarketDataPublisher publisher2(logger2);

    MatchingEngine recovered(
            logger2,
            writer2,
            snapshot2,
            publisher2);

    snapshot2.load(recovered);

    recovered.printBooks();

    EXPECT_NE(recoveredOutput.str().find("1 100 10"), std::string::npos);
    EXPECT_NE(recoveredOutput.str().find("2 200 20"), std::string::npos);

    EXPECT_EQ(recovered.getTimestamp(), 2);
}

TEST(MatchingEngineTest, SnapshotRecoveryAllowsFurtherTrading)
{
    {
        std::istringstream input(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10
)");

        std::ostringstream output;

        Logger logger;
        OutputWriter writer(output);
        SnapshotManager snapshot("snapshot_test.json");
        MarketDataPublisher publisher(logger);

        MatchingEngine engine(
                logger,
                writer,
                snapshot,
                publisher);

        engine.processOrders(input, output);
    }

    std::istringstream input2(R"(
LIMIT SELL 2 CLIENT2 AAPL 100.00 10
)");

    std::ostringstream output2;

    Logger logger2;
    OutputWriter writer2(output2);
    SnapshotManager snapshot2("snapshot_test.json");
    MarketDataPublisher publisher2(logger2);

    MatchingEngine recovered(
            logger2,
            writer2,
            snapshot2,
            publisher2);

    snapshot2.load(recovered);

    recovered.processOrders(input2, output2);

    EXPECT_EQ(output2.str().find("1 100 10"), std::string::npos);
    EXPECT_EQ(output2.str().find("2 100 10"), std::string::npos);
}
