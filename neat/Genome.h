#pragma once
#include "Innovation.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────────────────────
enum class NodeType { INPUT = 0, HIDDEN = 1, OUTPUT = 2 };

// ─────────────────────────────────────────────────────────────────────────────
//  Gene Structs
// ─────────────────────────────────────────────────────────────────────────────
struct NodeGene {
  int id;
  NodeType type;
  float bias = 0.0f;
};

struct ConnGene {
  int inNode;
  int outNode;
  float weight;
  bool enabled;
  int innovation;
};

// ─────────────────────────────────────────────────────────────────────────────
//  NEAT Hyperparameters (tweak freely without recompiling)
// ─────────────────────────────────────────────────────────────────────────────
namespace NeatCfg {
inline int POP_SIZE = 300;
inline float WEIGHT_MUTATE_RATE = 0.80f;
inline float WEIGHT_PERTURB_RATE = 0.90f;
inline float WEIGHT_PERTURB_STR = 0.35f;   // 0.15→0.35: Mạng 216 conns cần bước nhảy lớn hơn để đa dạng hóa
inline float ADD_CONN_RATE = 0.05f;
inline float ADD_NODE_RATE = 0.02f;         // 0.03→0.02: Tránh phình mạng quá sớm, ưu tiên tối ưu trọng số
inline float DISABLE_RATE = 0.03f;          // 0.05→0.03: Giảm rủi ro disable nhầm connection quan trọng
inline float CROSSOVER_RATE = 0.50f;        // 0.75→0.50: Tăng tỷ lệ Clone+Mutate, tránh tạo "frankenstein"
inline float COMPAT_C1 = 1.0f;   // excess gene coefficient
inline float COMPAT_C2 = 1.0f;   // disjoint gene coefficient
inline float COMPAT_C3 = 0.4f;   // weight-diff coefficient
inline float COMPAT_THRESHOLD =
    2.0f; // Ngưỡng 2.0 để chia được ~10-20 loài khỏe mạnh
inline int STAGNATION_LIMIT =
    30; // 20→30: Cho species thêm thời gian đào qua plateau
} // namespace NeatCfg

// ─────────────────────────────────────────────────────────────────────────────
//  Thread-local RNG for NEAT mutations & crossover (safe under OpenMP)
// ─────────────────────────────────────────────────────────────────────────────
inline std::mt19937 &NeatRng() {
  thread_local std::mt19937 rng(std::random_device{}());
  return rng;
}
inline float RandFloat01() {
  thread_local std::uniform_real_distribution<float> d(0.0f, 1.0f);
  return d(NeatRng());
}
inline float RandWeight() {
  thread_local std::uniform_real_distribution<float> d(-1.0f, 1.0f);
  return d(NeatRng());
}
/** Return a uniform random int in [0, n). */
inline int RandIdx(int n) {
  if (n <= 1)
    return 0;
  return (int)(NeatRng()() % (unsigned)n);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Genome
// ─────────────────────────────────────────────────────────────────────────────
class Genome {
public:
  std::vector<NodeGene> nodes;
  std::vector<ConnGene> conns;
  float fitness = 0.0f;
  float adjustedFitness = 0.0f;
  int speciesId = -1;
  int numInputs = 0;
  int numOutputs = 0;

  // ── Factory: fully-connected perceptron (all inputs → all outputs) ────────
  static Genome CreateInitial(int numIn, int numOut) {
    Genome g;
    g.numInputs = numIn;
    g.numOutputs = numOut;
    auto &inn = InnovationTracker::Get();

    for (int i = 0; i < numIn; i++) {
      NodeGene n;
      n.id = i + 1; // Khóa tĩnh ID cho Input
      n.type = NodeType::INPUT;
      n.bias = 0.0f;
      g.nodes.push_back(n);
    }
    for (int i = 0; i < numOut; i++) {
      NodeGene n;
      n.id = numIn + i + 1; // Khóa tĩnh ID cho Output
      n.type = NodeType::OUTPUT;
      n.bias = 0.0f;
      g.nodes.push_back(n);
    }

    // Cập nhật bộ đếm toàn cục để tránh trùng lặp khi Mutate mọc thêm Node
    if (inn.GetNodeCounter() < numIn + numOut) {
      inn.Reset(numIn + numOut);
    }

    for (int i = 0; i < numIn; i++) {
      for (int j = 0; j < numOut; j++) {
        ConnGene c;
        c.inNode = g.nodes[i].id;
        c.outNode = g.nodes[numIn + j].id;
        c.weight = RandWeight();
        c.enabled = true;
        c.innovation = inn.GetConnInnovation(c.inNode, c.outNode);
        g.conns.push_back(c);
      }
    }
    return g;
  }

  // ── Mutate (called on all non-elite offspring) ────────────────────────────
  void Mutate() {
    if (RandFloat01() < NeatCfg::WEIGHT_MUTATE_RATE)
      MutateWeights();
    if (RandFloat01() < NeatCfg::ADD_CONN_RATE)
      AddConnection();
    if (RandFloat01() < NeatCfg::ADD_NODE_RATE)
      AddNode();
    if (RandFloat01() < NeatCfg::DISABLE_RATE)
      ToggleConnection();
  }

  // ── Crossover: p1 must be the FITTER parent ───────────────────────────────
  static Genome Crossover(const Genome &p1, const Genome &p2) {
    Genome child;
    child.numInputs = p1.numInputs;
    child.numOutputs = p1.numOutputs;

    // Map p2 connection innovations
    std::unordered_map<int, const ConnGene *> p2Map;
    for (auto &c : p2.conns)
      p2Map[c.innovation] = &c;

    for (auto &c1 : p1.conns) {
      auto it = p2Map.find(c1.innovation);
      ConnGene nc;
      if (it != p2Map.end()) {
        // Matching gene: randomly choose from either parent
        const ConnGene &chosen = (RandFloat01() < 0.5f) ? c1 : *it->second;
        nc = chosen;
        // [VÁ LỖI ZOMBIE]: Chỉ 25% cơ hội hồi sinh nếu bố hoặc mẹ bị disable
        if (!c1.enabled || !it->second->enabled)
          nc.enabled = (RandFloat01() < 0.25f);
      } else {
        // Disjoint / excess from fitter parent → always inherit
        nc = c1;
      }

      // [VÁ LỖI DEADLOCK CHÍ MẠNG]:
      // Kiểm tra xem Gene mới được thêm vào có tạo Vòng lặp (Cycle) không.
      // Nếu có, LẬP TỨC VÔ HIỆU HÓA NÓ để cứu Thuật toán Kahn!
      if (nc.enabled && child.CreatesCycle(nc.inNode, nc.outNode)) {
        nc.enabled = false;
      }
      child.conns.push_back(nc);
    }

    // Collect required node IDs from p1 and from child connections
    std::unordered_set<int> needIds;
    for (auto &n : p1.nodes)
      needIds.insert(n.id);
    for (auto &c : child.conns) {
      needIds.insert(c.inNode);
      needIds.insert(c.outNode);
    }

    // Build a lookup from both parents
    std::unordered_map<int, NodeGene> nodeMap;
    for (auto &n : p1.nodes)
      nodeMap[n.id] = n;
    for (auto &n : p2.nodes)
      if (!nodeMap.count(n.id))
        nodeMap[n.id] = n;

    for (int id : needIds) {
      auto it = nodeMap.find(id);
      if (it != nodeMap.end())
        child.nodes.push_back(it->second);
    }
    return child;
  }

  // ── Compatibility distance δ = c1*E/N + c2*D/N + c3*W̄ ───────────────────
  float CompatibilityDistance(const Genome &other) const {
    std::unordered_map<int, float> myW, otW;
    for (auto &c : conns)
      myW[c.innovation] = c.weight;
    for (auto &c : other.conns)
      otW[c.innovation] = c.weight;

    int maxInn1 = 0, maxInn2 = 0;
    for (auto &[k, v] : myW)
      maxInn1 = std::max(maxInn1, k);
    for (auto &[k, v] : otW)
      maxInn2 = std::max(maxInn2, k);
    int threshold = std::min(maxInn1, maxInn2);

    int excess = 0, disjoint = 0, matching = 0;
    float wDiffSum = 0.0f;

    for (auto &[inn, w] : myW) {
      if (otW.count(inn)) {
        matching++;
        wDiffSum += std::abs(w - otW[inn]);
      } else {
        (inn > threshold ? excess : disjoint)++;
      }
    }
    for (auto &[inn, w] : otW) {
      if (!myW.count(inn))
        (inn > threshold ? excess : disjoint)++;
    }

    // Phân loài với mạng > 200 liên kết khởi điểm: ép N=1 để trân trọng từng
    // mutation nhỏ
    float N = 1.0f;
    float wBar = (matching > 0) ? wDiffSum / matching : 0.0f;

    return NeatCfg::COMPAT_C1 * excess / N + NeatCfg::COMPAT_C2 * disjoint / N +
           NeatCfg::COMPAT_C3 * wBar;
  }

  // ── Save / Load (binary) ──────────────────────────────────────────────────
  bool Save(const std::string &path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f)
      return false;
    f.write("NEAT", 4);
    int ver = 1;
    f.write(reinterpret_cast<const char *>(&ver), sizeof(ver));
    f.write(reinterpret_cast<const char *>(&numInputs), sizeof(numInputs));
    f.write(reinterpret_cast<const char *>(&numOutputs), sizeof(numOutputs));
    f.write(reinterpret_cast<const char *>(&fitness), sizeof(fitness));

    int nNodes = (int)nodes.size();
    f.write(reinterpret_cast<const char *>(&nNodes), sizeof(nNodes));
    for (auto &n : nodes) {
      f.write(reinterpret_cast<const char *>(&n.id), sizeof(n.id));
      int t = (int)n.type;
      f.write(reinterpret_cast<const char *>(&t), sizeof(t));
      f.write(reinterpret_cast<const char *>(&n.bias), sizeof(n.bias));
    }

    int nConns = (int)conns.size();
    f.write(reinterpret_cast<const char *>(&nConns), sizeof(nConns));
    for (auto &c : conns) {
      f.write(reinterpret_cast<const char *>(&c.inNode), sizeof(c.inNode));
      f.write(reinterpret_cast<const char *>(&c.outNode), sizeof(c.outNode));
      f.write(reinterpret_cast<const char *>(&c.weight), sizeof(c.weight));
      char en = c.enabled ? 1 : 0;
      f.write(&en, 1);
      f.write(reinterpret_cast<const char *>(&c.innovation),
              sizeof(c.innovation));
    }
    return true;
  }

  static Genome Load(const std::string &path) {
    Genome g;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
      printf("[NEAT] ERROR: Cannot open genome file: %s\n", path.c_str());
      return g;
    }
    char magic[4];
    f.read(magic, 4);
    if (magic[0] != 'N' || magic[1] != 'E' || magic[2] != 'A' ||
        magic[3] != 'T') {
      printf("[NEAT] ERROR: Invalid genome file format.\n");
      return g;
    }
    int ver;
    f.read(reinterpret_cast<char *>(&ver), sizeof(ver));
    f.read(reinterpret_cast<char *>(&g.numInputs), sizeof(g.numInputs));
    f.read(reinterpret_cast<char *>(&g.numOutputs), sizeof(g.numOutputs));
    f.read(reinterpret_cast<char *>(&g.fitness), sizeof(g.fitness));

    int nNodes;
    f.read(reinterpret_cast<char *>(&nNodes), sizeof(nNodes));
    g.nodes.resize(nNodes);
    for (auto &n : g.nodes) {
      f.read(reinterpret_cast<char *>(&n.id), sizeof(n.id));
      int t;
      f.read(reinterpret_cast<char *>(&t), sizeof(t));
      n.type = (NodeType)t;
      f.read(reinterpret_cast<char *>(&n.bias), sizeof(n.bias));
    }

    int nConns;
    f.read(reinterpret_cast<char *>(&nConns), sizeof(nConns));
    g.conns.resize(nConns);
    for (auto &c : g.conns) {
      f.read(reinterpret_cast<char *>(&c.inNode), sizeof(c.inNode));
      f.read(reinterpret_cast<char *>(&c.outNode), sizeof(c.outNode));
      f.read(reinterpret_cast<char *>(&c.weight), sizeof(c.weight));
      char en;
      f.read(&en, 1);
      c.enabled = (en == 1);
      f.read(reinterpret_cast<char *>(&c.innovation), sizeof(c.innovation));
    }
    return g;
  }

  // ── Convenience getters ───────────────────────────────────────────────────
  std::vector<int> GetInputIds() const {
    std::vector<int> ids;
    for (auto &n : nodes)
      if (n.type == NodeType::INPUT)
        ids.push_back(n.id);
    return ids;
  }
  std::vector<int> GetOutputIds() const {
    std::vector<int> ids;
    for (auto &n : nodes)
      if (n.type == NodeType::OUTPUT)
        ids.push_back(n.id);
    return ids;
  }

private:
  // ── Mutation helpers ──────────────────────────────────────────────────────
  void MutateWeights() {
    std::normal_distribution<float> perturb(0.0f, NeatCfg::WEIGHT_PERTURB_STR);
    for (auto &c : conns) {
      if (RandFloat01() < NeatCfg::WEIGHT_PERTURB_RATE) {
        c.weight += perturb(NeatRng());
        c.weight = std::clamp(c.weight, -5.0f, 5.0f);
      } else {
        c.weight = RandWeight();
      }
    }
  }

  void AddConnection() {
    // Build candidate lists
    std::vector<int> fromCands, toCands;
    for (auto &n : nodes) {
      if (n.type != NodeType::OUTPUT)
        fromCands.push_back(n.id);
      if (n.type != NodeType::INPUT)
        toCands.push_back(n.id);
    }
    if (fromCands.empty() || toCands.empty())
      return;

    auto &rng = NeatRng();
    for (int attempt = 0; attempt < 25; attempt++) {
      int from = fromCands[RandIdx((int)fromCands.size())];
      int to = toCands[RandIdx((int)toCands.size())];
      if (from == to)
        continue;

      // Already exists?
      bool exists = false;
      for (auto &c : conns)
        if (c.inNode == from && c.outNode == to) {
          exists = true;
          break;
        }
      if (exists)
        continue;

      // Would create a cycle?
      if (CreatesCycle(from, to))
        continue;

      ConnGene nc;
      nc.inNode = from;
      nc.outNode = to;
      nc.weight = RandWeight();
      nc.enabled = true;
      nc.innovation = InnovationTracker::Get().GetConnInnovation(from, to);
      conns.push_back(nc);
      return;
    }
  }

  void AddNode() {
    // Collect enabled connection indices
    std::vector<int> enabledIdx;
    for (int i = 0; i < (int)conns.size(); i++)
      if (conns[i].enabled)
        enabledIdx.push_back(i);
    if (enabledIdx.empty())
      return;

    auto &rng = NeatRng();
    int idx = enabledIdx[RandIdx((int)enabledIdx.size())];
    ConnGene old = conns[idx];
    conns[idx].enabled = false;

    auto &inn = InnovationTracker::Get();
    int newId = inn.NewNodeId();

    NodeGene newN;
    newN.id = newId;
    newN.type = NodeType::HIDDEN;
    newN.bias = 0.0f;
    nodes.push_back(newN);

    // in→new (weight 1.0 preserves old behaviour)
    ConnGene c1;
    c1.inNode = old.inNode;
    c1.outNode = newId;
    c1.weight = 1.0f;
    c1.enabled = true;
    c1.innovation = inn.GetConnInnovation(old.inNode, newId);
    conns.push_back(c1);

    // new→out (preserves old weight)
    ConnGene c2;
    c2.inNode = newId;
    c2.outNode = old.outNode;
    c2.weight = old.weight;
    c2.enabled = true;
    c2.innovation = inn.GetConnInnovation(newId, old.outNode);
    conns.push_back(c2);
  }

  void ToggleConnection() {
    if (conns.empty())
      return;
    auto &rng = NeatRng();
    conns[RandIdx((int)conns.size())].enabled ^= true;
  }

  // DFS from 'to': true if we can reach 'from' → adding from→to would create a
  // cycle
  bool CreatesCycle(int from, int to) const {
    std::unordered_set<int> visited;
    std::queue<int> q;
    q.push(to);
    while (!q.empty()) {
      int cur = q.front();
      q.pop();
      if (cur == from)
        return true;
      if (!visited.insert(cur).second)
        continue;
      for (auto &c : conns)
        if (c.enabled && c.inNode == cur && !visited.count(c.outNode))
          q.push(c.outNode);
    }
    return false;
  }
};
