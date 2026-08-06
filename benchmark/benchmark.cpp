#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <vector>

#include "MatchingEngine.hpp"

int main(int argc, char* argv[])
{
    int orderCount = 100000;

    if (argc > 1)
    {
        orderCount = std::stoi(argv[1]);
    }
    std::ostringstream input;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> symbolDist(0, 2);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> priceDist(95, 105);
    std::uniform_int_distribution<int> qtyDist(1, 100);

    const std::string symbols[] = {"AAPL", "GOOG", "MSFT"};

    for (int i = 1; i <= orderCount; ++i)
    {
        input
            << "LIMIT "
            << (sideDist(rng) ? "BUY " : "SELL ")
            << i
            << " CLIENT"
            << i
            << ' '
            << symbols[symbolDist(rng)]
            << ' '
            << priceDist(rng)
            << ' '
            << qtyDist(rng)
            << '\n';
    }

    std::istringstream orders(input.str());
    std::ostringstream output;

    Logger logger;
    logger.setEnabled(false);
    OutputWriter writer(output);
    SnapshotManager snapshot("benchmark_snapshot.json");
    MarketDataPublisher publisher(logger);

    MatchingEngine engine(
            logger,
            writer,
            snapshot,
            publisher);

    auto start = std::chrono::steady_clock::now();

    engine.processOrders(orders, output);

    auto end = std::chrono::steady_clock::now();

    auto totalNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - start).count();

    const auto& latencies = engine.getLatencySamples();

    if (latencies.empty())
    {
        std::cout << "No latency samples.\n";
        return 0;
    }

    std::vector<uint64_t> sorted = latencies;

    std::sort(sorted.begin(), sorted.end());

    auto percentile = [&](double p)
    {
        size_t index =
            static_cast<size_t>(
                    p * (sorted.size() - 1));

        return sorted[index];
    };

    double mean =
        static_cast<double>(
                std::accumulate(
                    latencies.begin(),
                    latencies.end(),
                    uint64_t(0)))
        / latencies.size();

    double throughput =
        static_cast<double>(orderCount)
        / (static_cast<double>(totalNs) / 1e9);

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "Orders processed : " << orderCount << '\n';
    std::cout << "Orders/sec       : " << throughput << '\n';
    std::cout << "Mean latency(ns) : " << mean << '\n';
    std::cout << "P50              : " << percentile(0.50) << '\n';
    std::cout << "P75              : " << percentile(0.75) << '\n';
    std::cout << "P90              : " << percentile(0.90) << '\n';
    std::cout << "P99              : " << percentile(0.99) << '\n';
    std::cout << "Max              : " << sorted.back() << '\n';

    return 0;
}
