#pragma once
#include "Genome.h"
#include "Network.h"
#include "Species.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif

/**
 * @brief NEAT Population — manages the entire evolution loop.
 *
 * Usage:
 *   Population pop(18, 6);           // 18 inputs, 6 outputs
 *   pop.EvaluateAll([](Genome& g){ return runEpisode(g); });
 *   pop.Evolve();
 *   pop.SaveBest("agents/phase1_best.bin");
 */
class Population {
public:
  std::vector<Genome> genomes;
  std::vector<Species> species;
  int generation = 0;
  float bestFitness = 0.0f;
  int numInputs, numOutputs;

private:
  int speciesCounter = 0;

public:
  // ── Constructor ───────────────────────────────────────────────────────────
  Population(int numIn, int numOut, int popSize = -1)
      : numInputs(numIn), numOutputs(numOut) {
    if (popSize < 0)
      popSize = NeatCfg::POP_SIZE;
    InnovationTracker::Get().Reset();

    Genome proto = Genome::CreateInitial(numIn, numOut);
    genomes.reserve(popSize);
    for (int i = 0; i < popSize; i++) {
      Genome g = proto;
      g.Mutate(); // give each initial genome a slightly different set of
                  // weights
      genomes.push_back(std::move(g));
    }
  }

  // ── Load from a saved genome (continue training from checkpoint) ──────────
  static Population FromCheckpoint(const std::string &path, int numIn,
                                   int numOut, int popSize = -1) {
    Population pop(numIn, numOut, popSize);
    Genome best = Genome::Load(path);
    if (!best.nodes.empty()) {
      // Sửa lỗi cực kỳ quan trọng: Đồng bộ lại mốc đếm ID của Tracker!
      int maxN = 0, maxC = 0;
      for (auto &n : best.nodes)
        maxN = std::max(maxN, n.id);
      for (auto &c : best.conns)
        maxC = std::max(maxC, c.innovation);
      InnovationTracker::Get().UpdateCounters(maxN, maxC);

      // Replace first genome with loaded best, mutate rest around it
      pop.genomes[0] = best;
      for (size_t i = 1; i < pop.genomes.size(); i++) {
        pop.genomes[i] = best;
        pop.genomes[i].Mutate();
      }
    }
    return pop;
  }

  // ── Evaluate all genomes ──────────────────────────────────────────────────
  /**
   * fitnessFunc receives a mutable Genome ref and must return its fitness.
   * THREAD-SAFE: Each thread evaluates a unique genome; use AZ::Rand() for RNG.
   */
  void EvaluateAll(const std::function<float(Genome &)> &fitnessFunc) {
    int n = (int)genomes.size();
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic) num_threads(omp_get_max_threads())
#endif
    for (int i = 0; i < n; i++)
      genomes[i].fitness = fitnessFunc(genomes[i]);
  }

  // ── One evolution step ────────────────────────────────────────────────────
  void Evolve() {
    Speciate();

    // ── Dynamic Compatibility Thresholding (Aggressive) ──
    // Target: duy trì 10-15 species (nhỏ hơn trước = selection pressure mạnh
    // hơn)
    const int TARGET_SPECIES_MIN = 10;
    const int TARGET_SPECIES_MAX = 15;
    const float ADJUST_RATE = 0.15f; // Smooth adjustment to prevent oscillation

    if ((int)species.size() < TARGET_SPECIES_MIN) {
      NeatCfg::COMPAT_THRESHOLD =
          std::max(0.3f, NeatCfg::COMPAT_THRESHOLD - ADJUST_RATE);
    } else if ((int)species.size() > TARGET_SPECIES_MAX) {
      NeatCfg::COMPAT_THRESHOLD += ADJUST_RATE;
    }
    for (auto &sp : species) {
      sp.ComputeAdjustedFitness(genomes);
      sp.UpdateStagnation(genomes);
    }
    RemoveStagnant();

    // Compute total adjusted fitness for proportional offspring allocation
    float totalAdj = 0.0f;
    for (auto &sp : species)
      totalAdj += sp.TotalAdjustedFitness(genomes);
    if (totalAdj <= 0.0f)
      totalAdj = 1.0f;

    // ── GLOBAL ELITISM: Save top 2 genomes across entire population ──────
    Genome elite1, elite2;
    float fit1 = -1e9f, fit2 = -1e9f;
    for (auto &g : genomes) {
      if (g.fitness > fit1) {
        fit2 = fit1;
        elite2 = elite1;
        fit1 = g.fitness;
        elite1 = g;
      } else if (g.fitness > fit2) {
        fit2 = g.fitness;
        elite2 = g;
      }
    }

    int targetSize = (int)genomes.size();
    std::vector<Genome> nextGen;
    nextGen.reserve(targetSize);

    for (auto &sp : species) {
      if (sp.memberIdxs.empty())
        continue;

      // Number of offspring proportional to share of total adjusted fitness
      int offspring = (int)std::round(sp.TotalAdjustedFitness(genomes) /
                                      totalAdj * targetSize);
      offspring = std::max(1, offspring);

      // Sort member indices by fitness descending
      std::sort(sp.memberIdxs.begin(), sp.memberIdxs.end(), [&](int a, int b) {
        return genomes[a].fitness > genomes[b].fitness;
      });

      // Elitism: carry the champion genome unchanged (if species is large
      // enough)
      if ((int)sp.memberIdxs.size() >= 5) {
        nextGen.push_back(genomes[sp.memberIdxs[0]]);
        offspring--;
      }

      // Cull bottom 50%
      int survivors = std::max(1, (int)(sp.memberIdxs.size() * 0.5f));
      sp.memberIdxs.resize(survivors);

      // Produce offspring
      for (int k = 0; k < offspring; k++) {
        Genome child;
        if ((int)sp.memberIdxs.size() >= 2 &&
            RandFloat01() < NeatCfg::CROSSOVER_RATE) {
          int i1 = TournamentIdx(sp, 3); // tournament k=3
          int i2 = TournamentIdx(sp, 3);
          if (i1 == i2)
            i2 = sp.memberIdxs[RandIdx((int)sp.memberIdxs.size())];

          Genome &p1 = genomes[i1];
          Genome &p2 = genomes[i2];
          if (p1.fitness >= p2.fitness)
            child = Genome::Crossover(p1, p2);
          else
            child = Genome::Crossover(p2, p1);
        } else {
          // Clone a random survivor
          child = genomes[sp.memberIdxs[RandIdx((int)sp.memberIdxs.size())]];
        }
        child.Mutate();
        // Reset fitness to worst so sort puts unevaluated children at end
        // (elite stays on top)
        child.fitness = -1e30f;
        nextGen.push_back(std::move(child));
      }
    }

    // Fill to target size if rounding left us short
    while ((int)nextGen.size() < targetSize && !species.empty()) {
      auto &rsp = species[RandIdx((int)species.size())];
      if (rsp.memberIdxs.empty())
        continue;
      Genome child = genomes[rsp.BestMemberIdx(genomes)];
      child.Mutate();
      // Reset fitness to worst so elite stays on top
      child.fitness = -1e30f;
      nextGen.push_back(std::move(child));
    }

    if ((int)nextGen.size() > targetSize)
      nextGen.resize(targetSize);

    // ── Restore top 2 elites (replace worst 2 if necessary) ────────────────
    if ((int)nextGen.size() >= 2) {
      // Place top 2 elites at BEST positions (0, 1) to ensure preservation
      // Sort descending so elites don't get pushed out next generation
      std::sort(nextGen.begin(), nextGen.end(),
                [](const Genome &a, const Genome &b) {
                  return a.fitness > b.fitness;
                });
      nextGen[0] = elite1; // Best position → elite1 (top genome)
      nextGen[1] = elite2; // 2nd best position → elite2 (2nd top)
    } else if ((int)nextGen.size() == 1) {
      nextGen[0] = elite1; // Only 1 slot, use best elite
    }

    genomes = std::move(nextGen);
    generation++;

    // Update all-time best
    for (auto &g : genomes)
      bestFitness = std::max(bestFitness, g.fitness);
  }

  // ── Statistics ────────────────────────────────────────────────────────────
  void PrintStats() const {
    // Calculate best/avg only from evaluated genomes (skip child genomes with
    // fitness=-1e30f)
    float best = -1e30f, avg = 0.0f;
    int countValid = 0;
    for (auto &g : genomes) {
      if (g.fitness > -1e20f) { // Valid genome (exclude -1e30f children)
        best = std::max(best, g.fitness);
        avg += g.fitness;
        countValid++;
      }
    }
    if (countValid > 0)
      avg /= (float)countValid;
    else {
      best = 0.0f;
      avg = 0.0f;
    }

    // Count hidden nodes in best genome
    int nodes = 0, conns = 0;
    for (auto &g : genomes) {
      if (g.fitness >= best - 1e-4f) {
        for (auto &n : g.nodes)
          if (n.type == NodeType::HIDDEN)
            nodes++;
        conns = (int)g.conns.size();
        break;
      }
    }

    printf("Gen %3d | Best: %8.2f | Avg: %7.2f | Sp: %2d | Hidden: %2d | "
           "Conns: %3d\n",
           generation, best, avg, (int)species.size(), nodes, conns);
  }

  // ── Accessors ─────────────────────────────────────────────────────────────
  Genome GetBest() const {
    return *std::max_element(
        genomes.begin(), genomes.end(),
        [](const Genome &a, const Genome &b) { return a.fitness < b.fitness; });
  }

  /** Save best genome to a binary file for later loading or watching. */
  bool SaveBest(const std::string &path) const { return GetBest().Save(path); }

private:
  // ── Speciation ────────────────────────────────────────────────────────────
  void Speciate() {
    // Clear member lists (representatives kept from last generation)
    for (auto &sp : species)
      sp.memberIdxs.clear();

    for (int i = 0; i < (int)genomes.size(); i++) {
      Genome &g = genomes[i];
      bool found = false;
      for (auto &sp : species) {
        if (sp.representative.CompatibilityDistance(g) <
            NeatCfg::COMPAT_THRESHOLD) {
          sp.memberIdxs.push_back(i);
          g.speciesId = sp.id;
          found = true;
          break;
        }
      }
      if (!found) {
        species.emplace_back(++speciesCounter, g);
        species.back().memberIdxs.push_back(i);
        g.speciesId = speciesCounter;
      }
    }

    // Remove empty species
    species.erase(
        std::remove_if(species.begin(), species.end(),
                       [](const Species &sp) { return sp.memberIdxs.empty(); }),
        species.end());

    // Update each species' representative to a random member from this
    // generation
    for (auto &sp : species) {
      int ridx = sp.memberIdxs[RandIdx((int)sp.memberIdxs.size())];
      sp.representative = genomes[ridx];
    }
  }

  // ── Remove stagnant species ───────────────────────────────────────────────
  void RemoveStagnant() {
    if (species.size() <= 1)
      return;

    // Find global best fitness
    float globalBest = 0.0f;
    for (auto &g : genomes)
      globalBest = std::max(globalBest, g.fitness);

    // Species that contains the global best genome is never eliminated
    int championSpeciesId = -1;
    for (auto &sp : species) {
      for (int idx : sp.memberIdxs) {
        if (genomes[idx].fitness >= globalBest - 1e-4f) {
          championSpeciesId = sp.id;
          break;
        }
      }
      if (championSpeciesId != -1)
        break;
    }

    species.erase(std::remove_if(species.begin(), species.end(),
                                 [&](const Species &sp) {
                                   return sp.IsStagnant() &&
                                          sp.id != championSpeciesId;
                                 }),
                  species.end());

    if (species.empty()) {
      // Recover: rebuild from genomes in case every species was stagnant
      speciesCounter++;
      Genome &rep = genomes[0];
      species.emplace_back(speciesCounter, rep);
      for (int i = 0; i < (int)genomes.size(); i++)
        species.back().memberIdxs.push_back(i);
    }
  }

  // ── Tournament selection — returns a genome index ─────────────────────────
  int TournamentIdx(const Species &sp, int k) {
    int best = sp.memberIdxs[RandIdx((int)sp.memberIdxs.size())];
    for (int i = 1; i < k; i++) {
      int cand = sp.memberIdxs[RandIdx((int)sp.memberIdxs.size())];
      if (genomes[cand].fitness > genomes[best].fitness)
        best = cand;
    }
    return best;
  }
};
