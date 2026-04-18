#pragma once
#include <vector>
#include <algorithm>
#include "Genome.h"

/**
 * @brief A NEAT species: a group of genomes that are structurally similar.
 *
 * Speciation protects structural innovations from being eliminated before
 * they have time to optimise.  Each species maintains a representative genome
 * (randomly chosen from the previous generation) and tracks stagnation so
 * that consistently poor species can be culled.
 */
struct Species {
    int    id;
    Genome representative;          ///< Used to test new genomes for membership
    std::vector<int> memberIdxs;    ///< Indices into Population::genomes
    float  bestFitness       = 0.0f;
    int    stagnationCounter = 0;
    int    age               = 0;

    Species() = default;
    Species(int _id, const Genome& rep) : id(_id), representative(rep) {}

    // ── Adjusted fitness (sharing within species) ─────────────────────────────
    /** Divide each member's raw fitness by species size to discourage dominance. */
    void ComputeAdjustedFitness(std::vector<Genome>& genomes) {
        float n = (float)std::max((int)memberIdxs.size(), 1);
        for (int idx : memberIdxs)
            genomes[idx].adjustedFitness = genomes[idx].fitness / n;
    }

    float TotalAdjustedFitness(const std::vector<Genome>& genomes) const {
        float sum = 0.0f;
        for (int idx : memberIdxs) sum += genomes[idx].adjustedFitness;
        return sum;
    }

    // ── Stagnation tracking ───────────────────────────────────────────────────
    void UpdateStagnation(const std::vector<Genome>& genomes) {
        float best = 0.0f;
        for (int idx : memberIdxs) best = std::max(best, genomes[idx].fitness);

        if (best > bestFitness + 1e-4f) {
            bestFitness       = best;
            stagnationCounter = 0;
        } else {
            stagnationCounter++;
        }
        age++;
    }

    // ── Best member index ─────────────────────────────────────────────────────
    int BestMemberIdx(const std::vector<Genome>& genomes) const {
        int best = memberIdxs[0];
        for (int idx : memberIdxs)
            if (genomes[idx].fitness > genomes[best].fitness) best = idx;
        return best;
    }

    bool IsStagnant() const {
        return stagnationCounter >= NeatCfg::STAGNATION_LIMIT;
    }
};
