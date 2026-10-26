#pragma once

#include <string>

class MatchingEngine;

class SnapshotManager
{
    public:
        explicit SnapshotManager(const std::string& fileName);

        bool save(const MatchingEngine& engine, std::string& error) const;
        void load(MatchingEngine& engine);

    private:
        std::string fileName_;
};
