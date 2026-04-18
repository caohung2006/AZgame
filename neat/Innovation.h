#pragma once
#include <map>
#include <utility>

/**
 * @brief Global Innovation Number Tracker (Singleton).
 *
 * Ensures that the same structural mutation (same from→to nodes)
 * always gets the same innovation number within a single run.
 * This is the key mechanic that allows NEAT crossover to work correctly.
 */
class InnovationTracker {
public:
    static InnovationTracker& Get() {
        static InnovationTracker inst;
        return inst;
    }

    /** Reset all state. Call once at the beginning of a new training run. */
    void Reset(int startNode = 0) {
        innovations.clear();
        connCounter  = 0;
        nodeCounter  = startNode;
    }

    /** Get (or create) a unique innovation number for a directed connection. */
    int GetConnInnovation(int from, int to) {
        auto key = std::make_pair(from, to);
        auto it  = innovations.find(key);
        if (it != innovations.end()) return it->second;
        innovations[key] = ++connCounter;
        return connCounter;
    }

    /** Allocate a brand-new unique node ID. */
    int NewNodeId() { return ++nodeCounter; }

    int GetNodeCounter() const { return nodeCounter; }
    int GetConnCounter() const { return connCounter; }

    /** Cập nhật bộ đếm nếu khôi phục từ tệp checkpoint */
    void UpdateCounters(int maxNodeId, int maxConnInn) {
        if (maxNodeId > nodeCounter)   nodeCounter = maxNodeId;
        if (maxConnInn > connCounter) connCounter = maxConnInn;
    }

private:
    InnovationTracker() : connCounter(0), nodeCounter(0) {}
    std::map<std::pair<int,int>, int> innovations;
    int connCounter;
    int nodeCounter;
};
