#include "MatchingEngine.hpp"

#include <sstream>
#include <string>
#include <iostream>

MatchingEngine::MatchingEngine(
        Logger& logger,
        OutputWriter& outputWriter,
        SnapshotManager& snapshotManager,
        MarketDataPublisher& marketDataPublisher)
    :
        logger_(logger),
        outputWriter_(outputWriter),
        snapshotManager_(snapshotManager),
        marketDataPublisher_(marketDataPublisher)
{
    LOG_DEBUG(logger_, "MatchingEngine created.");
    for (const auto& symbol : validSymbols_)
    {
        orderBooks_.try_emplace(
                symbol,
                symbol,
                logger_,
                outputWriter_,
                marketDataPublisher_);

        workers_[symbol].thread =
            std::thread(
                    &MatchingEngine::workerLoop,
                    this,
                    symbol);
    }
    LOG_INFO(
            logger_,
            "OrderBooks created = " +
            std::to_string(orderBooks_.size()));
}

void MatchingEngine::processOrders(std::istream& input,std::ostream& output)
{
    LOG_INFO(logger_, "Started processing input stream.");
    uint64_t processedOrders = 0;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        LOG_DEBUG(logger_, "Processing line: " + line);

        std::istringstream cancelStream(line);

        std::string command;

        cancelStream >> command;

        if (command == "CANCEL")
        {
            std::string symbol;
            uint64_t orderId;

            if (!(cancelStream >> symbol >> orderId))
            {
                LOG_ERROR(
                        logger_,
                        "Malformed CANCEL command.");
                continue;
            }

            EngineRequest request;

            request.type = RequestType::CANCEL;
            request.orderId = orderId;
            request.symbol = symbol;
            enqueueRequest(request);

            continue;
        }
        std::istringstream modifyStream(line);

        modifyStream >> command;

        if (command == "MODIFY")
        {
            std::string symbol;
            uint64_t orderId;
            std::string field;
            double value;
            if (!(modifyStream >> symbol >> orderId >> field >> value))
            {
                LOG_ERROR(
                        logger_,
                        "Malformed MODIFY command.");
                continue;
            }

            EngineRequest request;

            request.type = RequestType::MODIFY;
            request.orderId = orderId;
            request.field = field;
            request.value = value;
            request.symbol = symbol;
            enqueueRequest(request);

            continue;
        }
        std::istringstream iss(line);

        std::string orderType;
        std::string side;
        uint64_t orderId;
        std::string clientId;
        std::string symbol;
        double price;
        uint64_t quantity;

        if (!(iss >> orderType
                    >> side
                    >> orderId
                    >> clientId
                    >> symbol
                    >> price
                    >> quantity))
        {
            LOG_ERROR(logger_, "Malformed order: " + line);
            continue;
        }

        Side orderSide;

        if (side == "BUY")
        {
            orderSide = Side::BUY;
        }
        else if (side == "SELL")
        {
            orderSide = Side::SELL;
        }
        else
        {
            LOG_WARNING(
                    logger_,
                    "Invalid order side \"" +
                    side +
                    "\" for order ID=" +
                    std::to_string(orderId));

            continue;
        }

        OrderType type;

        if (orderType == "LIMIT")
        {
            type = OrderType::LIMIT;
        }
        else if (orderType == "MARKET")
        {
            type = OrderType::MARKET;
        }
        else if (orderType == "IOC")
        {
            type = OrderType::IOC;
        }
        else
        {
            LOG_WARNING(
                    logger_,
                    "Invalid order type \"" +
                    orderType +
                    "\" for order ID=" +
                    std::to_string(orderId));

            continue;
        }

        if (quantity == 0)
        {
            LOG_WARNING(
                    logger_,
                    "Rejected order ID=" +
                    std::to_string(orderId) +
                    ". Quantity must be greater than zero.");

            continue;
        }

        if (price < 0)
        {
            LOG_WARNING(
                    logger_,
                    "Rejected order ID=" +
                    std::to_string(orderId) +
                    ". Negative price.");

            continue;
        }

        if (!validSymbols_.contains(symbol))
        {
            LOG_WARNING(
                    logger_,
                    "Rejected order ID=" +
                    std::to_string(orderId) +
                    ". Unknown symbol \"" +
                    symbol +
                    "\".");

            continue;
        }

        {
            std::lock_guard lock(engineMutex_);

            if (!orderIds_.insert(orderId).second)
            {
                LOG_WARNING(
                        logger_,
                        "Duplicate order ID=" +
                        std::to_string(orderId) +
                        " rejected.");

                continue;
            }
        }
        LOG_INFO(
                logger_,
                "Accepted " +
                orderType +
                " " +
                side +
                " order ID=" +
                std::to_string(orderId) +
                " SYMBOL=" +
                symbol +
                " PRICE=" +
                std::to_string(price) +
                " QTY=" +
                std::to_string(quantity));
        ++processedOrders;
        EngineRequest request;

        request.type = RequestType::NEW_ORDER;
        uint64_t timestamp;

        {
            std::lock_guard lock(engineMutex_);
            timestamp = timestamp_++;
        }

        request.order = Order(
                orderId,
                clientId,
                symbol,
                orderSide,
                type,
                price,
                quantity,
                timestamp);

        enqueueRequest(request);
    }
    {
        std::unique_lock lock(completionMutex_);

        completionCondition_.wait(
                lock,
                [&]
                {
                return allWorkersIdle();
                });
    }
    printBooks();
    LOG_INFO(
            logger_,
            "Processing summary: "
            + std::to_string(processedOrders)
            + " orders processed, "
            + std::to_string(orderBooks_.size())
            + " active symbol(s).");
    snapshotManager_.save(*this);

    LOG_INFO(
            logger_,
            "Snapshot written successfully.");
    LOG_INFO(logger_, "Finished processing input stream.");
    LOG_INFO(logger_, "processOrders returning");
}

void MatchingEngine::printBooks()
{
    for (const auto& symbol : touchedSymbols_)
    {
        outputWriter_.beginSymbol(symbol);

        LOG_INFO(
                logger_,
                "Printing order book for symbol " + symbol);

        orderBooks_.at(symbol).writeBook();
    }
}

const std::unordered_map<std::string, OrderBook>&
MatchingEngine::getOrderBooks() const
{
    return orderBooks_;
}

uint64_t MatchingEngine::getTimestamp() const
{
    return timestamp_;
}

void MatchingEngine::loadSnapshot(
        const nlohmann::json& snapshot)
{
    orderIds_.clear();
    orderIdToSymbol_.clear();
    touchedSymbols_.clear();

    timestamp_ = snapshot.at("engineTimestamp");

    const auto& symbols = snapshot.at("symbols");

    for (const auto& [symbol, books] : symbols.items())
    {
        touchedSymbols_.insert(symbol);

        OrderBook& book = orderBooks_.at(symbol);
        book.clear();
        for (const auto& orderJson : books.at("buy"))
        {
            Order order(
                    0,
                    "",
                    "",
                    Side::BUY,
                    OrderType::LIMIT,
                    0.0,
                    0,
                    0);

            from_json(orderJson, order);
            book.restoreOrder(order);

            orderIds_.insert(order.getId());

            orderIdToSymbol_[order.getId()] =
                symbol;
        }

        for (const auto& orderJson : books.at("sell"))
        {
            Order order(
                    0,
                    "",
                    "",
                    Side::BUY,
                    OrderType::LIMIT,
                    0.0,
                    0,
                    0);

            from_json(orderJson, order);
            book.restoreOrder(order);

            orderIds_.insert(order.getId());

            orderIdToSymbol_[order.getId()] =
                symbol;
        }
    }

    LOG_INFO(
            logger_,
            "Snapshot loaded successfully.");
}

void MatchingEngine::processRequest(
        const EngineRequest& request)
{
    std::string type;

    switch (request.type)
    {
        case RequestType::NEW_ORDER:
            type = "NEW_ORDER";
            break;

        case RequestType::CANCEL:
            type = "CANCEL";
            break;

        case RequestType::MODIFY:
            type = "MODIFY";
            break;
    }
    LOG_INFO(
            logger_,
            "BEGIN " + type +
            " thread=" +
            std::to_string(
                std::hash<std::thread::id>{}(std::this_thread::get_id())));
    if (request.type == RequestType::CANCEL)
    {
        LOG_INFO(logger_, "Received CANCEL");

        std::unique_lock lock(engineMutex_);
        auto symbolIt = orderIdToSymbol_.find(request.orderId);

        if (symbolIt == orderIdToSymbol_.end())
        {
            LOG_WARNING(
                    logger_,
                    "Cancel ignored. Unknown order ID=" +
                    std::to_string(request.orderId));
            lock.unlock();
            return;
        }

        if (symbolIt->second != request.symbol)
        {
            LOG_WARNING(
                    logger_,
                    "Cancel ignored. Symbol mismatch for order ID=" +
                    std::to_string(request.orderId));
            lock.unlock();
            return;
        }

        LOG_DEBUG(
                logger_,
                "Routing CANCEL for order ID=" +
                std::to_string(request.orderId) +
                " to symbol " +
                request.symbol);

        OrderBook& book = orderBooks_.at(request.symbol);
        book.cancelOrder(request.orderId);

        orderIdToSymbol_.erase(symbolIt);

        LOG_INFO(
                logger_,
                "Cancelled order ID=" +
                std::to_string(request.orderId));
        lock.unlock();
        return;
    }
    if (request.type == RequestType::MODIFY)
    {
        LOG_INFO(logger_, "Received MODIFY");
        std::unique_lock lock(engineMutex_);
        auto symbolIt = orderIdToSymbol_.find(request.orderId);

        if (symbolIt == orderIdToSymbol_.end())
        {
            LOG_WARNING(
                    logger_,
                    "Modify ignored. Unknown order ID=" +
                    std::to_string(request.orderId));
            lock.unlock();
            return;
        }
        if (symbolIt->second != request.symbol)
        {
            LOG_WARNING(
                    logger_,
                    "Modify ignored. Symbol mismatch for order ID=" +
                    std::to_string(request.orderId));
            lock.unlock();
            return;
        }
        LOG_DEBUG(
                logger_,
                "Routing MODIFY for order ID=" +
                std::to_string(request.orderId) +
                " to symbol " +
                symbolIt->second);

        OrderBook& book = orderBooks_.at(request.symbol);
        Order* existing = book.findOrder(request.orderId);

        if (existing == nullptr)
        {
            LOG_WARNING(
                    logger_,
                    "Modify ignored. Order ID=" +
                    std::to_string(request.orderId) +
                    " not found in order book.");
            lock.unlock();
            return;
        }

        Order updated = *existing;

        if (request.field == "PRICE")
        {
            LOG_INFO(
                    logger_,
                    "Modifying PRICE of order ID=" +
                    std::to_string(request.orderId) +
                    " from " +
                    std::to_string(existing->getPrice()) +
                    " to " +
                    std::to_string(request.value));

            updated = Order(
                    existing->getId(),
                    existing->getClientId(),
                    existing->getSymbol(),
                    existing->getSide(),
                    existing->getType(),
                    request.value,
                    existing->getRemainingQuantity(),
                    timestamp_++);

            OrderBook& book = orderBooks_.at(request.symbol);

            book.cancelOrder(request.orderId);
            orderIdToSymbol_.erase(symbolIt);

            ProcessResult result = book.processOrder(updated);

            if (result.orderRested)
            {
                LOG_INFO(
                        logger_,
                        "Modified order ID=" +
                        std::to_string(request.orderId) +
                        " re-entered the order book.");

                orderIdToSymbol_[request.orderId] = updated.getSymbol();
            }

            for (uint64_t completedId : result.completedOrders)
            {
                LOG_DEBUG(
                        logger_,
                        "Removing completed order ID=" +
                        std::to_string(completedId));

                orderIdToSymbol_.erase(completedId);
            }
        }
        else if (request.field == "QUANTITY")
        {
            uint64_t newQuantity =
                static_cast<uint64_t>(request.value);

            uint64_t alreadyTraded =
                existing->getQuantity() -
                existing->getRemainingQuantity();

            LOG_INFO(
                    logger_,
                    "Modifying QUANTITY of order ID=" +
                    std::to_string(request.orderId) +
                    " from " +
                    std::to_string(existing->getQuantity()) +
                    " to " +
                    std::to_string(newQuantity));

            // Invalid: cannot reduce below already executed quantity.
            if (newQuantity < alreadyTraded)
            {
                LOG_WARNING(
                        logger_,
                        "Rejected quantity modification for order ID=" +
                        std::to_string(request.orderId) +
                        ". Requested quantity is below executed quantity.");
                lock.unlock();
                return;
            }

            uint64_t newRemaining =
                newQuantity - alreadyTraded;

            // Quantity decrease -> retain priority.
            if (newQuantity <= existing->getQuantity())
            {
                LOG_INFO(
                        logger_,
                        "Quantity decrease. Time priority retained for order ID=" +
                        std::to_string(request.orderId));

                existing->setQuantity(newQuantity);
                existing->setRemainingQuantity(newRemaining);
                book.notifyMarketDataUpdate();
            }
            // Quantity increase -> reset priority.
            else
            {
                LOG_INFO(
                        logger_,
                        "Quantity increase. Time priority reset for order ID=" +
                        std::to_string(request.orderId));

                updated = Order(
                        existing->getId(),
                        existing->getClientId(),
                        existing->getSymbol(),
                        existing->getSide(),
                        existing->getType(),
                        existing->getPrice(),
                        newQuantity,
                        timestamp_++);

                updated.setRemainingQuantity(newRemaining);

                OrderBook& book = orderBooks_.at(request.symbol);

                book.cancelOrder(request.orderId);
                orderIdToSymbol_.erase(symbolIt);

                ProcessResult result = book.processOrder(updated);

                if (result.orderRested)
                {
                    LOG_INFO(
                            logger_,
                            "Modified order ID=" +
                            std::to_string(request.orderId) +
                            " re-entered the order book.");

                    orderIdToSymbol_[request.orderId] = updated.getSymbol();
                }

                for (uint64_t completedId : result.completedOrders)
                {
                    LOG_DEBUG(
                            logger_,
                            "Removing completed order ID=" +
                            std::to_string(completedId));

                    orderIdToSymbol_.erase(completedId);
                }
            }
        }
        else
        {
            LOG_WARNING(
                    logger_,
                    "Unknown MODIFY field \"" +
                    request.field +
                    "\" for order ID=" +
                    std::to_string(request.orderId));
            lock.unlock();
            return;
        }
        lock.unlock();
        return;
    }
    if (request.type == RequestType::NEW_ORDER)
    {
        const Order& order = request.order;


        OrderBook& book = orderBooks_.at(order.getSymbol());
        ProcessResult result =
            book.processOrder(order);

        if (result.orderRested)
        {
            LOG_DEBUG(
                    logger_,
                    "Order ID=" +
                    std::to_string(order.getId()) +
                    " added to active order map.");

            {
                std::lock_guard lock(engineMutex_);

                orderIdToSymbol_[order.getId()] =
                    order.getSymbol();
            }
        }

        for (uint64_t completedId : result.completedOrders)
        {
            LOG_DEBUG(
                    logger_,
                    "Removing completed order ID=" +
                    std::to_string(completedId));
            {
                std::lock_guard lock(engineMutex_);

                orderIdToSymbol_.erase(completedId);
            }
        }
        auto finishTime = std::chrono::steady_clock::now();
        {
            std::lock_guard lock(latencyMutex_);

            latencySamples_.push_back(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        finishTime - request.enqueueTime).count());
        }
        return;
    }

    LOG_INFO(
            logger_,
            "END " + type);
}

MatchingEngine::~MatchingEngine()
{
    LOG_INFO(logger_, "~MatchingEngine begin");

    for (auto& [symbol, worker] : workers_)
    {
        {
            std::lock_guard lock(worker.mutex);
            worker.stop = true;
        }

        worker.condition.notify_one();
    }

    for (auto& [symbol, worker] : workers_)
    {
        if (worker.thread.joinable())
        {
            worker.thread.join();
        }
    }
    LOG_INFO(logger_, "~MatchingEngine end");
}

void MatchingEngine::workerLoop(const std::string& symbol)
{
    LOG_INFO(
            logger_,
            "Worker started for symbol " +
            symbol +
            " thread=" +
            std::to_string(
                std::hash<std::thread::id>{}(std::this_thread::get_id())));

    SymbolWorker& worker = workers_[symbol];

    std::unique_lock lock(worker.mutex);

    while (true)
    {
        worker.condition.wait(
                lock,
                [&]
                {
                return worker.stop ||
                !worker.requests.empty();
                });

        if (worker.stop &&
                worker.requests.empty())
        {
            LOG_INFO(
                    logger_,
                    "Worker stopped for symbol " +
                    symbol);
            break;
        }


        EngineRequest request = worker.requests.front();
        worker.requests.pop();
        worker.busy = true;
        LOG_DEBUG(
                logger_,
                symbol +
                " queue size=" +
                std::to_string(worker.requests.size()));
        lock.unlock();
        LOG_DEBUG(
                logger_,
                "Worker " +
                symbol +
                " processing request on thread=" +
                std::to_string(
                    std::hash<std::thread::id>{}(std::this_thread::get_id())));
        processRequest(request);

        lock.lock();

        worker.busy = false;
        completionCondition_.notify_one();
    }
}

void MatchingEngine::enqueueRequest(EngineRequest& request)
{
    request.enqueueTime = std::chrono::steady_clock::now();
    std::string symbol;

    if (request.type == RequestType::NEW_ORDER)
    {
        symbol = request.order.getSymbol();
    }
    else
    {
        symbol = request.symbol;
    }

    touchedSymbols_.insert(symbol);
    SymbolWorker& worker = workers_[symbol];

    {
        std::lock_guard lock(worker.mutex);
        LOG_INFO(
                logger_,
                "Enqueue " +
                symbol +
                " size(before)=" +
                std::to_string(worker.requests.size()));
        worker.requests.push(request);
        LOG_INFO(
                logger_,
                "Enqueue done " +
                symbol +
                " size(after)=" +
                std::to_string(worker.requests.size()));
    }

    worker.condition.notify_one();
}

bool MatchingEngine::allWorkersIdle()
{
    for (auto& [symbol, worker] : workers_)
    {
        std::lock_guard lock(worker.mutex);

        if (worker.busy || !worker.requests.empty())
        {
            return false;
        }
    }

    return true;
}

const std::vector<uint64_t>& MatchingEngine::getLatencySamples() const
{
    return latencySamples_;
}
