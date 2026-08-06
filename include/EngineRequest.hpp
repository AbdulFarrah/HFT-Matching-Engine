#pragma once

#include <string>
#include <chrono>

#include "Order.hpp"

enum class RequestType
{
    NEW_ORDER,
    CANCEL,
    MODIFY
};

struct EngineRequest
{
    RequestType type;

    Order order{
        0,
            "",
            "",
            Side::BUY,
            OrderType::LIMIT,
            0.0,
            0,
            0
    };

    uint64_t orderId = 0;

    std::string symbol;

    std::string field;

    double value = 0.0;
    std::chrono::steady_clock::time_point enqueueTime;
};
