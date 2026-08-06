#pragma once

#include <string>

class MatchingEngine;

class SnapshotManager
{
    public:
        explicit SnapshotManager(const std::string& fileName);

        void save(const MatchingEngine& engine) const;
        void load(MatchingEngine& engine);

    private:
        std::string fileName_;
};
