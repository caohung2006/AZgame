#pragma once
#include <random>

namespace AZ {
    /**
     * @brief Thread-local Random Number Generator.
     * Prevents race conditions when using OpenMP parallel loops.
     */
    inline std::mt19937& GetTLS_RNG() {
        thread_local std::mt19937 rng(std::random_device{}());
        return rng;
    }

    /**
     * @brief Equivalent to srand() but sets the seed for the current thread's RNG.
     */
    inline void SRand(unsigned int seed) {
        GetTLS_RNG().seed(seed);
    }

    /**
     * @brief Equivalent to rand(). Returns a pseudo-random integer between 0 and 32767.
     */
    inline int Rand() {
        return GetTLS_RNG()() & 0x7FFF; // 0x7FFF is 32767 (standard RAND_MAX on Windows)
    }
}
