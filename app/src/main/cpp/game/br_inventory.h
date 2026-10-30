#pragma once
#include <array>
#include <algorithm>

namespace zz::br {
enum Item : int {
    NONE = 0,
    PISTOL = 1,
    SMG = 2,
    RIFLE = 3,
    SHOTGUN = 4,
    SNIPER = 5,
    KNIFE = 6,
    AMMO_PISTOL = 10,
    AMMO_SMG = 11,
    AMMO_RIFLE = 12,
    AMMO_SHELL = 13,
    AMMO_SNIPER = 14,
    VEST = 20,
    HELMET = 21,
    BACKPACK = 22,
    MEDKIT = 23,
    BANDAGE = 24,
    GRENADE = 25,
    SMOKE = 26,
    ICE = 27,
    ADRENALINE = 28
};
inline bool weapon(int id) { return id >= PISTOL && id <= KNIFE; }
inline bool validItem(int id) { return weapon(id) || (id >= 10 && id <= 14) || (id >= 20 && id <= 28); }
inline int maxStack(int id) {
    return weapon(id) || id == VEST || id == HELMET || id == BACKPACK ? 1 : (id >= 10 && id <= 14 ? 60 : 5);
}
struct Stack {
    int id = 0, count = 0;
};
struct Inventory {
    std::array<Stack, 24> slots{};
    int backpack = 0, vest = 0, helmet = 0;
    int capacity() const { return 12 + std::max(0, std::min(2, backpack)) * 6; }
    int count(int item) const {
        int n = 0;
        for (const auto &s : slots)
            if (s.id == item)
                n += s.count;
        return n;
    }
    int add(int item, int n) {
        if (!validItem(item) || n <= 0 || n > 10000)
            return 0;
        if (weapon(item)) {
            if (count(item) > 0)
                return 0;
            n = 1;
        }
        int left = n;
        for (int i = 0; i < capacity(); ++i) {
            auto &s = slots[i];
            if (s.id == item && s.count < maxStack(item)) {
                int k = std::min(left, maxStack(item) - s.count);
                s.count += k;
                left -= k;
            }
        }
        for (int i = 0; i < capacity() && left > 0; ++i) {
            auto &s = slots[i];
            if (s.count == 0) {
                int k = std::min(left, maxStack(item));
                s = {item, k};
                left -= k;
            }
        }
        return n - left;
    }
    bool take(int item, int n) {
        if (!validItem(item) || n <= 0 || n > 10000 || count(item) < n)
            return false;
        for (auto &s : slots)
            if (s.id == item && n > 0) {
                int k = std::min(n, s.count);
                s.count -= k;
                n -= k;
                if (s.count == 0)
                    s.id = 0;
            }
        return true;
    }
};
struct WeaponSpec {
    int ammo, magazine;
    float damage, interval, range, reload;
    int pellets;
};
inline WeaponSpec weaponSpec(int id) {
    switch (id) {
    case PISTOL:
        return {AMMO_PISTOL, 12, 22, .30f, 85, 1.4f, 1};
    case SMG:
        return {AMMO_SMG, 30, 14, .075f, 100, 1.7f, 1};
    case SHOTGUN:
        return {AMMO_SHELL, 8, 11, .9f, 42, 2.5f, 8};
    case SNIPER:
        return {AMMO_SNIPER, 5, 78, 1.3f, 450, 2.8f, 1};
    case KNIFE:
        return {0, 0, 38, .55f, 2.6f, 0, 1};
    default:
        return {AMMO_RIFLE, 30, 21, .11f, 180, 2, 1};
    }
}
} // namespace zz::br
