#include "SnapshotManager.hpp"

#include <filesystem>
#include <fstream>
#include <exception>
#include <system_error>

#include <nlohmann/json.hpp>

#include "MatchingEngine.hpp"

using nlohmann::json;


SnapshotManager::SnapshotManager(const std::string& fileName)
    : fileName_(fileName)
{
}

bool SnapshotManager::save(
        const MatchingEngine& engine,
        std::string& error) const
{
    const std::filesystem::path snapshotDirectory = "../snapshots";
    const std::filesystem::path snapshotPath =
        snapshotDirectory / fileName_;
    std::filesystem::path temporaryPath = snapshotPath;
    temporaryPath += ".tmp";

    std::error_code filesystemError;
    std::filesystem::create_directories(
            snapshotDirectory,
            filesystemError);
    if (filesystemError)
    {
        error = "cannot create snapshot directory: " +
            filesystemError.message();
        return false;
    }

    json snapshot;

    snapshot["engineTimestamp"] = engine.getTimestamp();

    json symbols = json::object();

    for (const auto& [symbol, book] : engine.getOrderBooks())
    {
        symbols[symbol]["buy"] = book.getBuyOrders();
        symbols[symbol]["sell"] = book.getSellOrders();
    }

    snapshot["symbols"] = symbols;

    std::string serializedSnapshot;
    try
    {
        serializedSnapshot = snapshot.dump(4);
    }
    catch (const std::exception& exception)
    {
        error = "cannot serialize snapshot: ";
        error += exception.what();
        return false;
    }

    std::ofstream file(
            temporaryPath,
            std::ios::out | std::ios::trunc);
    if (!file)
    {
        error = "cannot open temporary snapshot file " +
            temporaryPath.string();
        return false;
    }

    file << serializedSnapshot;
    file.flush();
    if (!file)
    {
        error = "failed writing temporary snapshot file " +
            temporaryPath.string();
        file.close();
        std::filesystem::remove(temporaryPath, filesystemError);
        return false;
    }
    file.close();
    if (!file)
    {
        error = "failed closing temporary snapshot file " +
            temporaryPath.string();
        std::filesystem::remove(temporaryPath, filesystemError);
        return false;
    }

    std::filesystem::rename(
            temporaryPath,
            snapshotPath,
            filesystemError);
    if (filesystemError)
    {
        error = "cannot replace snapshot file " + snapshotPath.string() +
            ": " + filesystemError.message();
        std::filesystem::remove(temporaryPath, filesystemError);
        return false;
    }

    error.clear();
    return true;
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
