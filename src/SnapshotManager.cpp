#include "SnapshotManager.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "MatchingEngine.hpp"

using nlohmann::json;


SnapshotManager::SnapshotManager(const std::string& fileName)
    : fileName_(fileName)
{
}

void SnapshotManager::save(const MatchingEngine& engine) const
{
    const std::filesystem::path snapshotDirectory = "../snapshots";

    std::filesystem::create_directories(snapshotDirectory);

    json snapshot;

    snapshot["engineTimestamp"] = engine.getTimestamp();

    json symbols = json::object();

    for (const auto& [symbol, book] : engine.getOrderBooks())
    {
        symbols[symbol]["buy"] = book.getBuyOrders();
        symbols[symbol]["sell"] = book.getSellOrders();
    }

    snapshot["symbols"] = symbols;

    std::ofstream file(snapshotDirectory / fileName_);

    file << snapshot.dump(4);
}

void SnapshotManager::load(MatchingEngine& engine)
{
    const std::filesystem::path snapshotDirectory = "../snapshots";
    std::ifstream input(snapshotDirectory / fileName_);
    if (!input)
    {
        return;
    }

    nlohmann::json snapshot;

    input >> snapshot;

    engine.loadSnapshot(snapshot);
}
