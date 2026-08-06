#pragma once

#include <cstdint>
#include <istream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <ostream>
#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "OrderBook.hpp"
#include "Logger.hpp"
#include "OutputWriter.hpp"
#include "SnapshotManager.hpp"
#include "MarketDataPublisher.hpp"
#include "EngineRequest.hpp"

class MatchingEngine
{
    public:
        MatchingEngine(
                Logger& logger,
                OutputWriter& outputWriter,
                SnapshotManager& snapshotManager,
                MarketDataPublisher& marketDataPublisher);
        ~MatchingEngine();
        void processOrders(std::istream& input,
                std::ostream& output = std::cout);
        const std::unordered_map<std::string, OrderBook>& getOrderBooks() const;
        uint64_t getTimestamp() const;
        void loadSnapshot(const nlohmann::json& snapshot);
        void printBooks();
        const std::vector<uint64_t>& getLatencySamples() const;
    private:
        struct SymbolWorker
        {
            std::queue<EngineRequest> requests;
            std::mutex mutex;
            std::condition_variable condition;
            std::thread thread;
            bool stop = false;
            bool busy = false;
        };
        // One independent order book per symbol.
        std::unordered_map<std::string, OrderBook> orderBooks_;
        std::unordered_set<std::string> touchedSymbols_;
        std::unordered_map<std::string, SymbolWorker> workers_;
        void processRequest(const EngineRequest& request);
        // Supported symbols.
        const std::unordered_set<std::string> validSymbols_
        {
            "AAPL",
                "MSFT",
                "GOOG"
        };

        // Global order ID tracking.
        std::unordered_set<uint64_t> orderIds_;
        std::unordered_map<uint64_t, std::string> orderIdToSymbol_;
        // Global timestamp.
        uint64_t timestamp_ = 0;
        Logger& logger_;
        OutputWriter& outputWriter_;
        SnapshotManager& snapshotManager_;
        MarketDataPublisher& marketDataPublisher_;
        std::mutex completionMutex_;
        std::condition_variable completionCondition_;
        std::mutex engineMutex_;
        mutable std::mutex latencyMutex_;
        std::vector<uint64_t> latencySamples_;
        void workerLoop(const std::string& symbol);
        void enqueueRequest(EngineRequest& request);
        bool allWorkersIdle();
};
