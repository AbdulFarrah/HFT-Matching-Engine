#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

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

TEST(MatchingEngineTest, NegativeQuantityIsRejected)
{
    EXPECT_TRUE(runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 -1
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
    std::string output = runEngine(R"(
CANCEL AAPL 999
)");

    EXPECT_NE(output.find("========== AAPL =========="), std::string::npos);
    EXPECT_EQ(output.find("TRADE"), std::string::npos);
}

TEST(MatchingEngineTest, ModifyUnknownOrder)
{
    std::string output = runEngine(R"(
MODIFY AAPL 999 PRICE 120.00
)");

    EXPECT_NE(output.find("========== AAPL =========="), std::string::npos);
    EXPECT_EQ(output.find("TRADE"), std::string::npos);
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

TEST(MatchingEngineTest, MultiSymbolTradeOutputFollowsInputSequence)
{
    const std::string input = R"(
LIMIT SELL 1 CLIENT1 MSFT 100.00 1
LIMIT SELL 2 CLIENT2 AAPL 100.00 1
LIMIT BUY 3 CLIENT3 MSFT 100.00 1
LIMIT BUY 4 CLIENT4 AAPL 100.00 1
)";

    const std::string firstRun = runEngine(input);
    const std::string secondRun = runEngine(input);

    EXPECT_EQ(firstRun, secondRun);
    const size_t msftTrade =
        firstRun.find("TRADE BUY=3 SELL=1 PRICE=100 QTY=1");
    const size_t aaplTrade =
        firstRun.find("TRADE BUY=4 SELL=2 PRICE=100 QTY=1");

    ASSERT_NE(msftTrade, std::string::npos);
    ASSERT_NE(aaplTrade, std::string::npos);
    EXPECT_LT(msftTrade, aaplTrade);

    const size_t aaplBook = firstRun.find("========== AAPL ==========");
    const size_t msftBook = firstRun.find("========== MSFT ==========");
    ASSERT_NE(aaplBook, std::string::npos);
    ASSERT_NE(msftBook, std::string::npos);
    EXPECT_LT(aaplBook, msftBook);
}

TEST(MatchingEngineTest, ProcessOrdersUsesPassedOutputStream)
{
    std::istringstream input(R"(
LIMIT SELL 1 CLIENT1 AAPL 100.00 1
LIMIT BUY 2 CLIENT2 AAPL 100.00 1
)");
    std::ostringstream configuredOutput;
    std::ostringstream passedOutput;

    Logger logger;
    OutputWriter writer(configuredOutput);
    SnapshotManager snapshot("snapshot_test.json");
    MarketDataPublisher publisher(logger);
    MatchingEngine engine(logger, writer, snapshot, publisher);

    engine.processOrders(input, passedOutput);

    EXPECT_TRUE(configuredOutput.str().empty());
    EXPECT_NE(passedOutput.str().find("TRADE BUY=2 SELL=1"), std::string::npos);
}

TEST(MatchingEngineTest, ModifyPriceResetsPriority)
{
    std::string output = runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10
LIMIT BUY 2 CLIENT2 AAPL 101.00 10

MODIFY AAPL 1 PRICE 101.00

MARKET SELL 3 CLIENT3 AAPL 999.00 15
)");

    EXPECT_NE(output.find("TRADE BUY=2"), std::string::npos);
    EXPECT_NE(output.find("TRADE BUY=1"), std::string::npos);

    EXPECT_LT(
            output.find("TRADE BUY=2"),
            output.find("TRADE BUY=1"));
}

TEST(MatchingEngineTest, PriceModificationPreservesPreviouslyFilledQuantity)
{
    std::string output = runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10
MARKET SELL 2 CLIENT2 AAPL 999.00 4
MODIFY AAPL 1 PRICE 101.00
MODIFY AAPL 1 QUANTITY 8
MARKET SELL 3 CLIENT3 AAPL 999.00 8
)");

    EXPECT_NE(
            output.find("TRADE BUY=1 SELL=3 PRICE=101 QTY=4"),
            std::string::npos);
    EXPECT_EQ(
            output.find("TRADE BUY=1 SELL=3 PRICE=101 QTY=8"),
            std::string::npos);
}

TEST(MatchingEngineTest, ModifyQuantityDecreaseKeepsPriority)
{
    std::string output = runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 10
LIMIT BUY 2 CLIENT2 AAPL 100.00 10

MODIFY AAPL 1 QUANTITY 5

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

TEST(MatchingEngineTest, QuantityModificationToAlreadyFilledAmountCancelsOrder)
{
    std::string output = runEngine(R"(
LIMIT BUY 1 CLIENT1 AAPL 100.00 5
MARKET SELL 2 CLIENT2 AAPL 999.00 3
MODIFY AAPL 1 QUANTITY 3
MARKET SELL 3 CLIENT3 AAPL 999.00 1
)");

    EXPECT_NE(output.find("TRADE BUY=1 SELL=2 PRICE=100 QTY=3"), std::string::npos);
    EXPECT_EQ(output.find("QTY=0"), std::string::npos);
    EXPECT_EQ(output.find("1 100 0"), std::string::npos);
}

TEST(MatchingEngineTest, InvalidPriceAndFractionalQuantityModificationsAreRejected)
{
    std::string output = runEngine(R"(
LIMIT SELL 1 CLIENT1 AAPL 100.00 5
MODIFY AAPL 1 PRICE -1
MODIFY AAPL 1 QUANTITY 4.9
MODIFY AAPL 1 QUANTITY -1
)");

    EXPECT_NE(output.find("1 100 5"), std::string::npos);
    EXPECT_EQ(output.find("1 -1"), std::string::npos);
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

TEST(MatchingEngineTest, SnapshotWriteFailureIsReportedAndPreservesTarget)
{
    const std::filesystem::path target =
        "../snapshots/fix4_snapshot_failure_target";
    const std::filesystem::path temporary =
        "../snapshots/fix4_snapshot_failure_target.tmp";
    std::filesystem::remove_all(target);
    std::filesystem::remove(temporary);
    std::filesystem::create_directories(target);

    Logger logger;
    std::ostringstream output;
    OutputWriter writer(output);
    SnapshotManager snapshot("fix4_snapshot_failure_target");
    MarketDataPublisher publisher(logger);
    MatchingEngine engine(logger, writer, snapshot, publisher);
    std::string error;

    EXPECT_FALSE(snapshot.save(engine, error));
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(std::filesystem::is_directory(target));
    EXPECT_FALSE(std::filesystem::exists(temporary));

    std::filesystem::remove_all(target);
}

TEST(MatchingEngineTest, SnapshotRecoveryPublishesRestoredMarketData)
{
    const std::string snapshotName = "fix4_quote_snapshot.json";
    {
        std::istringstream input(
                "LIMIT BUY 901 CLIENT1 AAPL 123 7\n");
        std::ostringstream output;
        Logger logger;
        OutputWriter writer(output);
        SnapshotManager snapshot(snapshotName);
        MarketDataPublisher publisher(logger);
        MatchingEngine engine(logger, writer, snapshot, publisher);
        engine.processOrders(input, output);
    }

    {
        std::ostringstream output;
        Logger logger;
        OutputWriter writer(output);
        SnapshotManager snapshot(snapshotName);
        MarketDataPublisher publisher(logger);
        MatchingEngine recovered(logger, writer, snapshot, publisher);
        snapshot.load(recovered);
    }

    std::ifstream marketData("../logs/market_data.csv");
    const std::string contents(
            (std::istreambuf_iterator<char>(marketData)),
            std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("AAPL,123,7,0,0"), std::string::npos);
    std::filesystem::remove("../snapshots/" + snapshotName);
}

TEST(MatchingEngineTest, LoggerCanBeToggledWhileWorkersLog)
{
    Logger logger;
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker)
    {
        workers.emplace_back([&logger, worker]
        {
            for (int i = 0; i < 500; ++i)
            {
                logger.logMessage(
                        LogType::DEBUG,
                        "worker=" + std::to_string(worker),
                        __FILE__,
                        __LINE__);
            }
        });
    }

    for (int i = 0; i < 500; ++i)
    {
        logger.setEnabled(i % 2 == 0);
    }
    logger.setEnabled(true);

    for (auto& worker : workers)
    {
        worker.join();
    }

    SUCCEED();
}
