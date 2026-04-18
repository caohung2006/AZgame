#pragma once
#include <vector>
#include <unordered_map>
#include <queue>
#include <cmath>
#include <algorithm>
#include "Genome.h"

/**
 * @brief Decoded feed-forward neural network from a Genome.
 *
 * Build once per genome evaluation with Network::FromGenome(), then call
 * Activate() as many times as needed during the episode.
 *
 * Activation functions:
 *   INPUT  nodes → identity (value set directly from observation)
 *   HIDDEN nodes → tanh
 *   OUTPUT nodes → sigmoid  (output ∈ [0,1]; threshold 0.5 for boolean actions)
 */
class Network {
public:
    // ── Factory ───────────────────────────────────────────────────────────────
    static Network FromGenome(const Genome& g) {
        Network net;
        net.inputIds  = g.GetInputIds();
        net.outputIds = g.GetOutputIds();

        // Store node metadata
        int maxNid = 0;
        for (auto& n : g.nodes) {
            net.nodeType[n.id] = n.type;
            net.nodeBias[n.id] = n.bias;
            maxNid = std::max(maxNid, n.id);
        }
        net.maxNodeId = maxNid;
        net.nodeValues.assign(maxNid + 1, 0.0f);  // Pre-allocate to avoid repeated allocations

        // Build in-degree map and adjacency list for enabled connections
        std::unordered_map<int, int>              inDeg;
        std::unordered_map<int, std::vector<int>> adj;
        for (auto& n : g.nodes) inDeg[n.id] = 0;

        for (auto& c : g.conns) {
            if (!c.enabled) continue;
            adj[c.inNode].push_back(c.outNode);
            inDeg[c.outNode]++;
            // Store weighted incoming edges per node
            net.incoming[c.outNode].emplace_back(c.inNode, c.weight);
        }

        // Kahn's topological sort (skips any nodes forming a cycle)
        std::queue<int> q;
        for (auto& [id, deg] : inDeg) if (deg == 0) q.push(id);
        while (!q.empty()) {
            int cur = q.front(); q.pop();
            net.topoOrder.push_back(cur);
            auto it = adj.find(cur);
            if (it != adj.end()) {
                for (int nxt : it->second)
                    if (--inDeg[nxt] == 0) q.push(nxt);
            }
        }

        return net;
    }

    // ── Forward pass ─────────────────────────────────────────────────────────
    void Activate(const std::vector<float>& inputs, std::vector<float>& outputs) {
        // Clear reusable node values (faster than allocating new unordered_map)
        std::fill(nodeValues.begin(), nodeValues.end(), 0.0f);

        // Set input node values directly from observation
        for (size_t i = 0; i < inputIds.size(); i++)
            if ((size_t)inputIds[i] < nodeValues.size())
                nodeValues[inputIds[i]] = (i < inputs.size()) ? inputs[i] : 0.0f;

        // Process remaining nodes in topological order
        for (int nodeId : topoOrder) {
            if (nodeType[nodeId] == NodeType::INPUT) continue; // already set

            float sum = nodeBias[nodeId];
            auto  it  = incoming.find(nodeId);
            if (it != incoming.end()) {
                for (auto& [inId, w] : it->second) {
                    if ((size_t)inId < nodeValues.size())
                        sum += nodeValues[inId] * w;
                }
            }

            if ((size_t)nodeId < nodeValues.size()) {
                if (nodeType[nodeId] == NodeType::HIDDEN)
                    nodeValues[nodeId] = std::tanh(sum);
                else // OUTPUT
                    nodeValues[nodeId] = 1.0f / (1.0f + std::exp(-sum));  // sigmoid
            }
        }

        // Collect outputs in the same order as genome output nodes
        outputs.clear();
        outputs.reserve(outputIds.size());
        for (int id : outputIds) {
            if ((size_t)id < nodeValues.size())
                outputs.push_back(nodeValues[id]);
            else
                outputs.push_back(0.0f);
        }
    }

private:
    std::vector<int> inputIds, outputIds, topoOrder;
    std::vector<float> nodeValues;  // Pre-allocated for O(1) Activate() calls
    int maxNodeId = 0;
    std::unordered_map<int, NodeType>  nodeType;
    std::unordered_map<int, float>     nodeBias;
    // outNode → list of (inNode, weight)
    std::unordered_map<int, std::vector<std::pair<int, float>>> incoming;
};
