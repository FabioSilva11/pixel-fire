// Gerador determinístico e ruído de valor para o terreno procedural.
#pragma once

#include <cmath>
#include <cstdint>

#include "math.h"

namespace zz {

class Rng {
public:
    explicit Rng(uint32_t seed = 0x9E3779B9u) { reseed(seed); }
    void reseed(uint32_t seed) {
        state_ = seed ? seed : 0x6D2B79F5u;
        for (int i = 0; i < 4; ++i) next();
    }
    uint32_t next() {
        // xorshift32
        uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x;
        return x;
    }
    float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }
    float range(float lo, float hi) { return lo + (hi - lo) * uniform(); }
    int rangeInt(int lo, int hiInclusive) {
        if (hiInclusive <= lo) return lo;
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hiInclusive - lo + 1));
    }
    bool chance(float p) { return uniform() < p; }

private:
    uint32_t state_ = 1;
};

inline uint32_t hash2(int x, int z, uint32_t seed) {
    uint32_t h = seed;
    h ^= static_cast<uint32_t>(x) * 0x27d4eb2du;
    h = (h ^ (h >> 15)) * 0x85ebca6bu;
    h ^= static_cast<uint32_t>(z) * 0x165667b1u;
    h = (h ^ (h >> 13)) * 0xc2b2ae35u;
    return h ^ (h >> 16);
}

inline uint32_t hash3(int x, int y, int z, uint32_t seed) {
    return hash2(x, z ^ (y * 0x3C6EF372), seed);
}

inline float hashFloat(int x, int z, uint32_t seed) {
    return (hash2(x, z, seed) >> 8) * (1.0f / 16777216.0f);
}

// Ruído de valor 2D suavizado.
inline float valueNoise(float x, float z, uint32_t seed) {
    int x0 = static_cast<int>(std::floor(x)), z0 = static_cast<int>(std::floor(z));
    float fx = x - x0, fz = z - z0;
    float sx = fx * fx * (3 - 2 * fx), sz = fz * fz * (3 - 2 * fz);
    float a = hashFloat(x0, z0, seed), b = hashFloat(x0 + 1, z0, seed);
    float c = hashFloat(x0, z0 + 1, seed), d = hashFloat(x0 + 1, z0 + 1, seed);
    return lerpf(lerpf(a, b, sx), lerpf(c, d, sx), sz);
}

// Ruído fractal (fBm) em [0, 1].
inline float fbm(float x, float z, uint32_t seed, int octaves = 4) {
    float sum = 0, amp = 0.5f, norm = 0, freq = 1;
    for (int i = 0; i < octaves; ++i) {
        sum += valueNoise(x * freq, z * freq, seed + i * 101u) * amp;
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return sum / norm;
}

}  // namespace zz
