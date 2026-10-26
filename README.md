# High Frequency Trading (HFT) Matching Engine

A production-oriented High Frequency Trading (HFT) Matching Engine implemented in **C++20**. The engine supports multiple symbols, deterministic order matching using **Price-Time Priority**, concurrent processing through **one worker thread per symbol**, execution logging, market data publication, snapshot recovery, benchmarking, and comprehensive unit testing.

The project emphasizes clear component boundaries, price-time matching, concurrent per-symbol processing, and reproducible output.

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
- **SnapshotManager** – Saves active orders and the engine timestamp, and restores them on startup.
- **OutputWriter** – Serializes trade records and prints the final state of each order book.

Each symbol owns its own `OrderBook`, request queue, and worker thread, allowing different symbols to be processed concurrently while ensuring deterministic processing within each symbol.

## Order Processing Flow

1. The input is parsed and validated.
2. Valid requests are placed into the queue of the corresponding symbol.
3. Each symbol's worker thread processes requests sequentially.
4. The order is matched against the opposite side of the order book using Price-Time Priority.
5. Any remaining quantity is either added to the book or cancelled, depending on the order type.
6. After each book change, market data is published and executions are logged.
7. After all workers become idle, trade records are written in input sequence, the final books are printed, and a snapshot is saved.

## Threading Model

The engine follows a **multiple producer, single consumer per symbol** design.

- The main thread parses and validates incoming requests.
- Each symbol owns a dedicated worker thread and request queue.
- Requests for the same symbol are processed sequentially, preserving deterministic matching.
- Requests for different symbols can be processed concurrently without blocking each other.
- Shared logging, market-data, and output components synchronize access across worker threads. Trade records are buffered until workers finish so cross-symbol output follows input sequence.

## Data Structures

The engine uses data structures that provide efficient order matching while maintaining Price-Time Priority.

- `std::map` stores price levels, allowing the best bid and ask to be accessed efficiently.
- `std::list` stores orders at each price level, preserving FIFO order and enabling constant-time insertion and removal.
- `std::unordered_map` provides fast lookup of active orders, order books, and order-to-symbol mappings.
- Per-price-level cached quantities let market-data publication report best-level sizes without scanning every order at that level.
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

After each input batch, the engine saves a JSON snapshot. The executable loads `../snapshots/snapshot.json` at startup when that file exists; otherwise it starts with empty books.

The snapshot contains:
- Engine timestamp
- All active buy orders
- All active sell orders

Recovery rebuilds order books, active-order mappings, cached quote quantities, and the timestamp counter. Snapshot writes go to a temporary file and are renamed into place after a successful write; failures are logged, leaving the previous snapshot intact when replacement fails.

## Logging

The engine maintains separate logs for different purposes:

- **debug.log** – Records engine activity, request processing, worker thread events, and error messages.
- **executions.csv** – Records every successful trade execution with timestamp, order IDs, symbol, price, and quantity.
- **market_data.csv** – Records the latest best bid and ask after every order book update.

Logging is thread-safe to support concurrent processing across multiple symbol worker threads.
When run from `build/`, these files are written under `../logs/`. Generated log and snapshot files are ignored by Git; the tracked `.gitkeep` files retain the directories.

## Market Data

The engine publishes market data after every change to an order book.

Each update contains:
- Best Bid
- Bid Quantity
- Best Ask
- Ask Quantity

The latest market data for each symbol is written to `market_data.csv`. Updates are synchronized, and cached per-price totals make best-level quantity lookup expected O(1), independent of the number of orders at that level.

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

Example output from a local run of the default 100,000-order benchmark:

```text
Orders processed : 100000
Orders/sec       : 49449.30
Mean latency(ns) : 199494972.74
P50              : 189104580
P75              : 293489622
P90              : 393129700
P99              : 456828008
Max              : 460325923
```

> **Note:** Benchmark results depend on hardware, operating system, compiler optimizations, and system load. These values are a reference run, not a performance guarantee.

## Unit Tests

The project includes **53 GoogleTest unit tests** covering the core functionality of the matching engine.

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
- Snapshot write failure handling and restored market-data publication
- Cached best-level quantity maintenance across book mutations
- Deterministic multi-symbol trade output and concurrent logger toggling
- Order routing across multiple symbols

## Time & Space Complexity

| Operation                         | Time Complexity |
|-----------------------------------|-----------------|
| Rest order at a price level       | O(log P)        |
| Cancel order                      | O(log P)        |
| Price modification                | O(log P)        |
| Quantity-only modification        | Expected O(1)   |
| Active order lookup               | Expected O(1)   |
| Best price and cached level size  | Expected O(1)   |
| Match order                       | O(log P + R + K + S) |

Where:
- **P** = Number of price levels.
- **R** = Number of price levels visited by the match.
- **K** = Number of resting orders filled.
- **S** = Total self-trade-prevention list checks; checks repeated while matching at a level are included.

The per-batch trade-output sort takes O(T log T), where **T** is the number of trades in that batch.

### Space Complexity

- **Order Book and per-level totals:** O(N + P)
- **Order Lookup Table:** O(N)
- **Worker Queues:** O(Q)

Where:
- **N** = Number of active orders.
- **Q** = Number of pending requests waiting to be processed.

## Build & Run

### Prerequisites

- C++20 compatible compiler
- CMake 3.20 or later
- nlohmann/json

### Build

```bash
cmake -S . -B build
cmake --build build --target matching_engine unit_tests benchmark -j2
```

### Run the Matching Engine

```bash
./matching_engine <input_file> <output_file>
```

Run from `build/` so the relative `../logs/` and `../snapshots/` paths point into the repository:

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

Snapshot loading is enabled in the main executable. To reproduce the checked-in cold-start and recovery outputs, run from `build/`:

```bash
rm -f ../snapshots/snapshot.json ../snapshots/snapshot.json.tmp
./matching_engine ../sample/input.txt ../sample/output.txt
./matching_engine ../sample/input.txt ../sample/recovery_output.txt
```

The master input includes a recovery probe: the cold run leaves order 900 resting in the snapshot; the warm run cancels that restored order before the repeated order ID is rejected. The first run should match `sample/output.txt`; the second should match `sample/recovery_output.txt`.

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
- **logs/** - Runtime debug, execution, and market-data files (generated and Git-ignored).
- **sample/** - Master regression input plus cold-start and recovery outputs.
- **snapshots/** - Runtime JSON snapshots and temporary snapshot files (generated and Git-ignored).
- **tests/** - GoogleTest unit tests.

## Design Decisions & Trade-offs

- A dedicated worker thread is assigned to each supported symbol, allowing different symbols to be processed concurrently while preserving deterministic order matching within a symbol.

- Separate order books are maintained for each symbol to eliminate contention between unrelated instruments and simplify synchronization.

- Price levels are stored using `std::map`, while orders at each price level are stored in `std::list`. This combination provides efficient price lookup while preserving FIFO ordering.
- Cached per-price quantity totals are updated on restore, resting, fills, cancels, quantity modifications, and book clearing.

- Active orders are indexed using hash maps to support fast order lookup during cancel and modify operations.

- Order books are created during engine initialization instead of on demand, avoiding concurrent modifications to the order book container and simplifying the threading model.

- Logger enablement is atomic; log writes, market-data writes, and output writes are synchronized across workers.

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
