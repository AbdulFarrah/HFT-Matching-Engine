# High Frequency Trading (HFT) Matching Engine

A production-oriented High Frequency Trading (HFT) Matching Engine implemented in **C++20**. The engine supports multiple symbols, deterministic order matching using **Price-Time Priority**, concurrent processing through **one worker thread per symbol**, execution logging, market data publication, snapshot recovery, benchmarking, and comprehensive unit testing.

The project is designed to emphasize clean architecture, efficient data structures, thread safety, and low-latency order processing while remaining modular and easy to extend.

## Features

- Supports multiple independent order books (AAPL, GOOG and MSFT)
- Price-Time Priority matching
- Limit Orders
- Market Orders
- Immediate-Or-Cancel (IOC) Orders
- Cancel Orders
- Modify Orders
  - Price modification
  - Quantity modification
- Partial fills
- Multiple fills
- Complete fills
- Self-trade prevention
- Thread-safe execution logging
- Thread-safe market data publication
- Order book snapshots
- Snapshot recovery
- Per-symbol worker threads
- Duplicate order detection
- Input validation and error handling
- GoogleTest unit tests
- Benchmark reporting:
  - Orders per second
  - Mean latency
  - P50
  - P75
  - P90
  - P99
  - Maximum latency


## Architecture

The engine is divided into independent components, each with a single responsibility.

- **MatchingEngine** – Validates requests, routes them to the appropriate symbol, manages worker threads, and coordinates the overall workflow.
- **OrderBook** – Maintains the buy and sell books for a single symbol and performs order matching.
- **Order** – Represents an individual order and stores its attributes and execution state.
- **Logger** – Records debug messages and trade executions in a thread-safe manner.
- **MarketDataPublisher** – Publishes the latest best bid and ask whenever the order book changes.
- **SnapshotManager** – Saves and restores the complete engine state using JSON snapshots.
- **OutputWriter** – Prints the final state of each order book.

Each symbol owns its own `OrderBook`, request queue, and worker thread, allowing different symbols to be processed concurrently while ensuring deterministic processing within each symbol.

## Order Processing Flow

1. The input is parsed and validated.
2. Valid requests are placed into the queue of the corresponding symbol.
3. Each symbol's worker thread processes requests sequentially.
4. The order is matched against the opposite side of the order book using Price-Time Priority.
5. Any remaining quantity is either added to the book or cancelled, depending on the order type.
6. After every change, market data is published and trade executions are logged.
7. Once all input has been processed, the final order books are printed and a snapshot of the engine state is saved.

## Threading Model

The engine follows a **multiple producer, single consumer per symbol** design.

- The main thread parses and validates incoming requests.
- Each symbol owns a dedicated worker thread and request queue.
- Requests for the same symbol are processed sequentially, preserving deterministic matching.
- Requests for different symbols can be processed concurrently without blocking each other.
- Shared components such as the logger and market data publisher are protected using mutexes to ensure thread safety.

## Data Structures

The engine uses data structures that provide efficient order matching while maintaining Price-Time Priority.

- `std::map` stores price levels, allowing the best bid and ask to be accessed efficiently.
- `std::list` stores orders at each price level, preserving FIFO order and enabling constant-time insertion and removal.
- `std::unordered_map` provides fast lookup of active orders, order books, and order-to-symbol mappings.
- `std::queue` stores pending requests for each symbol and preserves arrival order.
- `std::unordered_set` is used for duplicate order detection and tracking symbols that received requests.

## Matching Algorithm

The engine follows the **Price-Time Priority** matching policy.

- Buy orders are matched against the lowest available sell price.
- Sell orders are matched against the highest available buy price.
- Orders at better prices always receive priority.
- Orders at the same price are matched in FIFO order.
- The execution price is always the price of the resting order.
- The engine supports partial fills, multiple fills, and complete fills within a single incoming order.
- Any remaining quantity is handled according to the order type (Limit, Market, or IOC).

## Supported Order Types

### Limit Order
- Matches immediately whenever possible.
- Any remaining quantity rests in the order book.

### Market Order
- Consumes available liquidity from the opposite side of the book.
- Any unfilled quantity is cancelled.

### Immediate-Or-Cancel (IOC) Order
- Executes immediately against available liquidity.
- Any remaining quantity is cancelled instead of resting in the order book.

### Cancel Order
- Removes an active order from the order book.
- Cancelling an unknown or already completed order is ignored gracefully.

### Modify Order
The engine supports modification of both price and quantity.

- Price modification resets time priority.
- Quantity decrease retains the original priority.
- Quantity increase resets time priority.

## Snapshot & Recovery

The engine periodically saves its state as a JSON snapshot.

The snapshot contains:
- Engine timestamp
- All active buy orders
- All active sell orders

On startup, the engine can restore its state from a previously saved snapshot, rebuilding the order books, active order mappings, and timestamp counter. Trading can continue normally after recovery without losing price-time priority among the restored orders.

## Logging

The engine maintains separate logs for different purposes:

- **debug.log** – Records engine activity, request processing, worker thread events, and error messages.
- **executions.csv** – Records every successful trade execution with timestamp, order IDs, symbol, price, and quantity.
- **market_data.csv** – Records the latest best bid and ask after every order book update.

Logging is thread-safe to support concurrent processing across multiple symbol worker threads.

## Market Data

The engine publishes market data after every change to an order book.

Each update contains:
- Best Bid
- Bid Quantity
- Best Ask
- Ask Quantity

The latest market data for each symbol is written to `market_data.csv`. Updates are synchronized to ensure thread-safe publishing when multiple symbol workers are active.

## Benchmark

A benchmark program is provided to measure the performance of the matching engine.

The benchmark reports:
- Orders processed per second
- Mean latency
- P50 latency
- P75 latency
- P90 latency
- P99 latency
- Maximum latency

The number of generated orders can be specified as a command-line argument. If no argument is provided, the benchmark processes 100,000 orders by default.

Example output:

```text
Orders processed : 100000
Orders/sec       : 56978.04
Mean latency(ns) : 215881697.49
P50              : 222012700
P75              : 332632786
P90              : 373988516
P99              : 418579816
Max              : 423499018
```

> **Note:** Benchmark results depend on the hardware, operating system, compiler optimizations, and system load. The values above are from a sample run and may vary on different machines.

## Unit Tests

The project includes **43 GoogleTest unit tests** covering the core functionality of the matching engine.

The tests verify:
- Order creation and state updates
- Price-Time Priority
- FIFO matching
- Partial, multiple, and complete fills
- Market and IOC order behavior
- Cancel and modify operations
- Self-trade prevention
- Invalid input handling
- Snapshot save and recovery
- Order routing across multiple symbols

## Time & Space Complexity

| Operation           | Time Complexity |
|---------------------|-----------------|
| Add Order           | O(log P)        |
| Cancel Order        | O(log P)        |
| Modify Order        | O(log P)        |
| Find Order          | O(1)            |
| Best Bid / Best Ask | O(1)            |
| Match Order         | O(K × log P)    |

Where:
- **P** = Number of price levels.
- **K** = Number of resting orders matched by the incoming order.

### Space Complexity

- **Order Book:** O(N)
- **Order Lookup Table:** O(N)
- **Worker Queues:** O(R)

Where:
- **N** = Number of active orders.
- **R** = Number of pending requests waiting to be processed.

## Build & Run

### Prerequisites

- C++20 compatible compiler
- CMake 3.20 or later
- nlohmann/json

### Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Run the Matching Engine

```bash
./matching_engine <input_file> <output_file>
```

Example:

```bash
./matching_engine ../sample/input.txt ../sample/output.txt
```

### Run Unit Tests

```bash
./unit_tests
```

### Run Benchmark

```bash
./benchmark
```

To benchmark a different number of orders:

```bash
./benchmark <number_of_orders>
```

### Snapshot Recovery

The engine supports restoring its previous state from a saved snapshot. This can be enabled by uncommenting the snapshot loading code in `main.cpp` before processing new orders.

## Project Structure

```
HFT/
├── benchmark/
├── build/
├── include/
├── logs/
├── sample/
├── snapshots/
├── src/
├── tests/
├── CMakeLists.txt
└── README.md
```

- **benchmark/** - Benchmark program for measuring engine performance.
- **include/** - Header files.
- **src/** - Source code implementation.
- **logs/** - Debug, execution, and market data logs.
- **sample/** - Sample input and output files.
- **snapshots/** - JSON snapshots used for state persistence and recovery.
- **tests/** - GoogleTest unit tests.

## Design Decisions & Trade-offs

- A dedicated worker thread is assigned to each supported symbol, allowing different symbols to be processed concurrently while preserving deterministic order matching within a symbol.

- Separate order books are maintained for each symbol to eliminate contention between unrelated instruments and simplify synchronization.

- Price levels are stored using `std::map`, while orders at each price level are stored in `std::list`. This combination provides efficient price lookup while preserving FIFO ordering.

- Active orders are indexed using hash maps to support fast order lookup during cancel and modify operations.

- Order books are created during engine initialization instead of on demand, avoiding concurrent modifications to the order book container and simplifying the threading model.

- Logger and MarketDataPublisher are protected with mutexes to ensure thread-safe access from multiple worker threads.

- Engine state is persisted as JSON snapshots, allowing recovery after restart without rebuilding the order book manually.

- The implementation prioritizes correctness, modularity, and deterministic behavior while keeping the design simple enough to extend with additional order types or symbols.

## Future Improvements

Possible enhancements for future versions include:

- Support for dynamically adding and removing trading symbols.
- Additional order types such as Stop and Stop-Limit orders.
- Lock-free queues to further reduce synchronization overhead.
- Network interfaces (FIX, TCP, or WebSocket) for real-time order submission.
- More advanced market data dissemination.
- Multi-threaded input processing for higher throughput.
- Additional benchmarking and stress-testing under larger workloads.
