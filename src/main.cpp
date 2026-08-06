#include <fstream>
#include <iostream>

#include "MatchingEngine.hpp"
#include "Logger.hpp"

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr<<"arguments must be 3"<<std::endl;
        return 1;
    }

    std::ifstream input(argv[1]);

    if (!input)
    {
        std::cerr<<"failed to open input file"<<std::endl;
        return 1;
    }

    std::ofstream output(argv[2], std::ios::out | std::ios::trunc);

    if (!output)
    {
        std::cerr<<"failed to open output file"<<std::endl;
        return 1;
    }

    Logger logger;
    LOG_INFO(logger,"Matching Engine started.");
    OutputWriter outputWriter(output);
    SnapshotManager snapshotManager("snapshot.json");
    MarketDataPublisher marketDataPublisher(logger);
    MatchingEngine engine(logger, outputWriter,snapshotManager,marketDataPublisher);
    engine.processOrders(input);
    LOG_INFO(logger,"Matching Engine finished successfully.");
    /*
       LOG_INFO(logger,"Recover Engine started.");
       MatchingEngine recoveredEngine(
       logger,
       outputWriter,
       snapshotManager,marketDataPublisher);
       snapshotManager.load(recoveredEngine);
       outputWriter.beginSymbol("RECOVERED ENGINE");
       recoveredEngine.printBooks();
       LOG_INFO(logger,"Recover Engine finished successfully.");
     */
    return 0;
}
