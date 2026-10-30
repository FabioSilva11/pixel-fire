#pragma once
#include <cmath>
#include <cstddef>
#include "br_inventory.h"
namespace zz::br {
inline bool clientInputActive(float phase) { return phase == 1.f || phase == 2.f; }
inline bool validSnapshot(const float *data, size_t n) {
    if (!data || n < 72 || n > 24000 || data[0] != 1)
        return false;
    for (size_t i = 0; i < n; ++i)
        if (!std::isfinite(data[i]))
            return false;
    const int positions[] = {16, 17, 18, 19, 20};
    const int limits[] = {50, 1800, 128, 12, 100};
    int counts[5];
    for (int i = 0; i < 5; ++i) {
        float v = data[positions[i]];
        if (v < 0 || v > limits[i] || std::floor(v) != v)
            return false;
        counts[i] = int(v);
    }
    if (n !=
        size_t(24 + counts[0] * 32 + counts[1] * 7 + counts[2] * 10 + counts[3] * 7 + counts[4] * 5 + 48))
        return false;
    const auto integer = [](float v, int low, int high) {
        return v >= low && v <= high && std::floor(v) == v;
    };
    if (!integer(data[1], 0, 3) || !integer(data[3], -1, 49) || data[2] < 0 || data[6] < 0 ||
        data[6] > 1440 || data[9] < 0 || data[9] > 1440 || data[10] < -.11f ||
        !integer(data[11], 0, 100000) || !integer(data[12], 0, 1) || !integer(data[21], 0, 50))
        return false;
    bool seen[50] = {};
    size_t at = 24;
    for (int i = 0; i < counts[0]; ++i, at += 32) {
        const float *p = data + at;
        if (!integer(p[0], 0, 49) || seen[int(p[0])] || !integer(p[1], 0, 49) || !integer(p[2], 0, 65535) ||
            !integer(p[3], 0, 63) || p[4] < 0 || p[4] > 1440 || p[5] < -10 || p[5] > 300 || p[6] < 0 ||
            p[6] > 1440 || std::abs(p[8]) > 1.55f || p[9] < 0 || p[9] > 100 || !integer(p[10], 0, 100000) ||
            !integer(p[11], 0, 3) || !integer(p[12], 0, 6) || p[13] < 0 || p[13] > 60 || p[14] < 0 ||
            p[15] < 0 || p[15] > 100 || p[16] < 0 || p[16] > 80 || !integer(p[17], 0, 2) ||
            !integer(p[18], 0, 16777215) || !integer(p[23], 0, 50) || !integer(p[26], -1, 23) ||
            !integer(p[30], 0, 65535) || !integer(p[31], 0, 16777215))
            return false;
        seen[int(p[0])] = true;
    }
    for (int i = 0; i < counts[1]; ++i, at += 7) {
        const float *p = data + at;
        if (!integer(p[0], 1, 16777215) || !integer(p[1], 1, 28) || !validItem(int(p[1])) ||
            !integer(p[2], 1, 10000) || p[3] < 0 || p[3] > 1440 || p[4] < 0 || p[4] > 300 || p[5] < 0 ||
            p[5] > 1440)
            return false;
    }
    for (int i = 0; i < counts[2]; ++i, at += 10) {
        const float *p = data + at;
        if (!integer(p[0], 1, 16777215) || !integer(p[1], 0, 49) || p[6] < 0 || p[6] > 250 || p[7] < 0 ||
            p[7] > 45)
            return false;
    }
    for (int i = 0; i < counts[3]; ++i, at += 7) {
        const float *p = data + at;
        if (!integer(p[0], 1, 16777215) || !integer(p[4], 0, 1) || p[5] < 0)
            return false;
    }
    at += size_t(counts[4]) * 5;
    for (int i = 0; i < 24; ++i, at += 2) {
        const float item = data[at], count = data[at + 1];
        if (!integer(item, 0, 28) || !integer(count, 0, 60))
            return false;
        if (item == 0) {
            if (count != 0)
                return false;
        } else if (!validItem(int(item)) || count < 1 || count > maxStack(int(item)))
            return false;
    }
    return true;
}
} // namespace zz::br
