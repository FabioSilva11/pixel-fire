#include "structures.h"

#include <cmath>
#include <cstdlib>

#include "blocks.h"

namespace zz {

namespace {

void leafBlock(BlockSink& w, int x, int y, int z, uint8_t leaf) {
    if (w.get(x, y, z) == B_AIR) w.set(x, y, z, leaf);
}

// --------------------------------------------------------------------------- árvores (troncos altos)

void placeOak(BlockSink& w, int x, int y, int z, Rng& rng) {
    if (rng.chance(0.18f)) {
        // Carvalho gigante: copa esférica bem acima da cabeça.
        int h = rng.rangeInt(9, 11);
        for (int i = 0; i < h; ++i) w.set(x, y + i, z, B_LOG_OAK);
        int cy = h - 1;
        for (int dy = -3; dy <= 2; ++dy)
            for (int dz = -3; dz <= 3; ++dz)
                for (int dx = -3; dx <= 3; ++dx) {
                    float dist = static_cast<float>(dx * dx + dz * dz) + dy * dy * 1.7f;
                    if (dist > 10.5f || (dist > 8.0f && rng.chance(0.4f))) continue;
                    if (dx == 0 && dz == 0 && cy + dy < h) continue;
                    leafBlock(w, x + dx, y + cy + dy, z + dz, B_LEAVES_OAK);
                }
        return;
    }
    int h = rng.rangeInt(7, 9);
    for (int i = 0; i < h; ++i) w.set(x, y + i, z, B_LOG_OAK);
    for (int dy = h - 3; dy <= h + 1; ++dy) {
        int r = dy <= h - 1 ? 2 : 1;
        for (int dz = -r; dz <= r; ++dz) {
            for (int dx = -r; dx <= r; ++dx) {
                bool corner = std::abs(dx) == r && std::abs(dz) == r;
                if (corner && (dy == h + 1 || dy == h - 1 || rng.chance(0.5f))) continue;
                if (dx == 0 && dz == 0 && dy < h) continue;
                leafBlock(w, x + dx, y + dy, z + dz, B_LEAVES_OAK);
            }
        }
    }
}

void placeSpruce(BlockSink& w, int x, int y, int z, Rng& rng) {
    int h = rng.rangeInt(9, 12);
    for (int i = 0; i < h; ++i) w.set(x, y + i, z, B_LOG_SPRUCE);
    for (int dy = 4; dy <= h; ++dy) {
        int fromTop = h - dy;
        int layerR = fromTop == 0 ? 0 : (fromTop < 3 ? 1 : (dy % 2 == 0 ? 2 : 1));
        for (int dz = -layerR; dz <= layerR; ++dz)
            for (int dx = -layerR; dx <= layerR; ++dx) {
                if (std::abs(dx) + std::abs(dz) > layerR + 1) continue;
                if (dx == 0 && dz == 0 && dy < h) continue;
                leafBlock(w, x + dx, y + dy, z + dz, B_LEAVES_SPRUCE);
            }
    }
    w.set(x, y + h, z, B_LEAVES_SPRUCE);
}

void placeCactus(BlockSink& w, int x, int y, int z, Rng& rng) {
    int h = rng.chance(0.4f) ? 2 : 1;  // nunca mais alto que o jogador
    for (int i = 0; i < h; ++i) w.set(x, y + i, z, B_CACTUS);
}

void placeDeadTree(BlockSink& w, int x, int y, int z, Rng& rng) {
    int h = rng.rangeInt(5, 7);
    for (int i = 0; i < h; ++i) w.set(x, y + i, z, B_LOG_DEAD);
    int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int branches = rng.rangeInt(1, 3);
    for (int b = 0; b < branches; ++b) {
        int* d = dirs[rng.rangeInt(0, 3)];
        int by = y + h - 1 - b;
        w.set(x + d[0], by, z + d[1], B_LOG_DEAD);
        w.set(x + d[0] * 2, by + 1, z + d[1] * 2, B_LOG_DEAD);
    }
}

// Bétula: tronco branco alto e fino, copa estreita.
void placeBirch(BlockSink& w, int x, int y, int z, Rng& rng) {
    int h = rng.rangeInt(7, 10);
    for (int i = 0; i < h; ++i) w.set(x, y + i, z, B_LOG_BIRCH);
    for (int dy = h - 4; dy <= h + 1; ++dy) {
        int r = (dy >= h) ? 1 : (dy <= h - 4 ? 1 : 2);
        for (int dz = -r; dz <= r; ++dz)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) == r && std::abs(dz) == r && (r > 1 || rng.chance(0.6f))) continue;
                if (dx == 0 && dz == 0 && dy < h) continue;
                leafBlock(w, x + dx, y + dy, z + dz, B_LEAVES_BIRCH);
            }
    }
}

// Copa arredondada (cerejeira, bordo): bolha larga e um pouco achatada.
void roundCanopy(BlockSink& w, int x, int cy, int z, int rx, int ry, uint8_t leaf, Rng& rng) {
    for (int dy = -ry; dy <= ry; ++dy)
        for (int dz = -rx; dz <= rx; ++dz)
            for (int dx = -rx; dx <= rx; ++dx) {
                float d = static_cast<float>(dx * dx + dz * dz) / static_cast<float>(rx * rx) + static_cast<float>(dy * dy) / static_cast<float>(std::max(1, ry * ry));
                if (d > 1.05f || (d > 0.75f && rng.chance(0.35f))) continue;
                leafBlock(w, x + dx, cy + dy, z + dz, leaf);
            }
}

// Cerejeira: tronco curto que se abre em galhos e copa rosa larga.
void placeCherry(BlockSink& w, int x, int y, int z, Rng& rng) {
    int h = rng.rangeInt(4, 6);
    for (int i = 0; i < h; ++i) w.set(x, y + i, z, B_LOG_CHERRY);
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int a = rng.rangeInt(0, 3), b = (a + 1 + rng.rangeInt(0, 2)) % 4;
    for (int k : {a, b}) {
        w.set(x + dirs[k][0], y + h, z + dirs[k][1], B_LOG_CHERRY);
        w.set(x + dirs[k][0] * 2, y + h + 1, z + dirs[k][1] * 2, B_LOG_CHERRY);
        roundCanopy(w, x + dirs[k][0] * 2, y + h + 2, z + dirs[k][1] * 2, 2, 1, B_LEAVES_CHERRY, rng);
    }
    roundCanopy(w, x, y + h + 1, z, 3, 2, B_LEAVES_CHERRY, rng);
}

// Bordo de outono: copa redonda vermelho-alaranjada.
void placeMaple(BlockSink& w, int x, int y, int z, Rng& rng) {
    int h = rng.rangeInt(6, 8);
    for (int i = 0; i < h; ++i) w.set(x, y + i, z, B_LOG_OAK);
    roundCanopy(w, x, y + h - 1, z, 3, 3, B_LEAVES_MAPLE, rng);
}

// Ipê amarelo: tronco alto e copa em guarda-chuva, plana e larga.
void placeIpe(BlockSink& w, int x, int y, int z, Rng& rng) {
    int h = rng.rangeInt(6, 8);
    for (int i = 0; i < h; ++i) w.set(x, y + i, z, B_LOG_DEAD);
    for (int dz = -3; dz <= 3; ++dz)
        for (int dx = -3; dx <= 3; ++dx) {
            int d2 = dx * dx + dz * dz;
            if (d2 > 10) continue;
            leafBlock(w, x + dx, y + h, z + dz, B_LEAVES_GOLD);
            if (d2 <= 5) leafBlock(w, x + dx, y + h + 1, z + dz, B_LEAVES_GOLD);
            if (d2 > 4 && d2 <= 9 && rng.chance(0.5f)) leafBlock(w, x + dx, y + h - 1, z + dz, B_LEAVES_GOLD);
        }
}

// Jacarandá: tronco que se divide em dois braços com tufos roxos.
void placeJacaranda(BlockSink& w, int x, int y, int z, Rng& rng) {
    int h = rng.rangeInt(4, 5);
    for (int i = 0; i < h; ++i) w.set(x, y + i, z, B_LOG_CHERRY);
    bool alongX = rng.chance(0.5f);
    for (int side = -1; side <= 1; side += 2) {
        int bx = alongX ? side : 0, bz = alongX ? 0 : side;
        w.set(x + bx, y + h, z + bz, B_LOG_CHERRY);
        w.set(x + bx * 2, y + h + 1, z + bz * 2, B_LOG_CHERRY);
        w.set(x + bx * 2, y + h + 2, z + bz * 2, B_LOG_CHERRY);
        roundCanopy(w, x + bx * 2, y + h + 3, z + bz * 2, 2, 1, B_LEAVES_PURPLE, rng);
    }
    roundCanopy(w, x, y + h + 2, z, 2, 1, B_LEAVES_PURPLE, rng);
}

// Coqueiro: tronco levemente curvo e folhas compridas que caem nas pontas.
void placePalm(BlockSink& w, int x, int y, int z, Rng& rng) {
    int h = rng.rangeInt(6, 9);
    int lean = rng.rangeInt(0, 3);
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int tx = x, tz = z;
    for (int i = 0; i < h; ++i) {
        if (i == h / 2 || i == h - 2) { tx += dirs[lean][0]; tz += dirs[lean][1]; }
        w.set(tx, y + i, tz, B_LOG_PALM);
    }
    int top = y + h;
    leafBlock(w, tx, top, tz, B_LEAVES_PALM);
    const int fronds[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (const auto& f : fronds) {
        int len = (f[0] && f[1]) ? 2 : 3;
        for (int k = 1; k <= len; ++k) leafBlock(w, tx + f[0] * k, top - (k == len ? 1 : 0), tz + f[1] * k, B_LEAVES_PALM);
    }
}

void tent(BlockSink& w, int x, int y, int z, bool alongZ, uint8_t cloth) {
    // Barraca em "A" aberta nas pontas (cobertura, nunca sala fechada).
    for (int k = 0; k < 3; ++k) {
        int ax = alongZ ? 0 : k, az = alongZ ? k : 0;
        int sx = alongZ ? 1 : 0, sz = alongZ ? 0 : 1;
        w.set(x + ax - sx, y, z + az - sz, cloth);
        w.set(x + ax + sx, y, z + az + sz, cloth);
        w.set(x + ax, y + 1, z + az, cloth);
    }
}

}  // namespace

void placeTree(BlockSink& w, TreeKind kind, int x, int y, int z, Rng& rng) {
    switch (kind) {
        case TreeKind::Oak: placeOak(w, x, y, z, rng); break;
        case TreeKind::Spruce: placeSpruce(w, x, y, z, rng); break;
        case TreeKind::Cactus: placeCactus(w, x, y, z, rng); break;
        case TreeKind::Dead: placeDeadTree(w, x, y, z, rng); break;
        case TreeKind::Birch: placeBirch(w, x, y, z, rng); break;
        case TreeKind::Cherry: placeCherry(w, x, y, z, rng); break;
        case TreeKind::Maple: placeMaple(w, x, y, z, rng); break;
        case TreeKind::Ipe: placeIpe(w, x, y, z, rng); break;
        case TreeKind::Jacaranda: placeJacaranda(w, x, y, z, rng); break;
        case TreeKind::Palm: placePalm(w, x, y, z, rng); break;
    }
}

TreeKind pickTreeKind(const Theme& theme, int x, int z, uint32_t seed, bool shore) {
    // Bosques: cada região tem uma espécie dominante (cerejeiras juntas, ipês juntos...).
    float grove = fbm(x / 150.0f, z / 150.0f, seed + 4242, 2);
    float species = fbm(x / 260.0f, z / 260.0f, seed + 4245, 2);
    float pick = hashFloat(x, z, seed + 4243);
    auto flowering = [&]() {
        if (species < 0.42f) return TreeKind::Cherry;
        if (species < 0.5f) return TreeKind::Ipe;
        if (species < 0.58f) return TreeKind::Jacaranda;
        return TreeKind::Maple;
    };
    switch (theme.id) {
        case 0:  // floresta
            if (shore && pick < 0.65f) return TreeKind::Palm;
            if (grove > 0.58f) return pick < 0.82f ? flowering() : TreeKind::Oak;
            if (pick < 0.03f) return flowering();
            return pick < (species > 0.55f ? 0.45f : 0.25f) ? TreeKind::Birch : TreeKind::Oak;
        case 1:  // deserto: cactos, coqueiros nos oásis e ipês raros
            if (shore) return pick < 0.8f ? TreeKind::Palm : TreeKind::Cactus;
            if (grove > 0.64f && pick < 0.4f) return TreeKind::Ipe;
            return pick < 0.12f ? TreeKind::Palm : TreeKind::Cactus;
        case 2:  // neve: pinheiros, bétulas e bordos de outono
            if (grove > 0.6f && pick < 0.6f) return TreeKind::Maple;
            return pick < 0.22f ? TreeKind::Birch : TreeKind::Spruce;
        default:  // terra devastada: árvores mortas e jacarandás "corrompidos"
            if (grove > 0.6f && pick < 0.55f) return pick < 0.35f ? TreeKind::Jacaranda : TreeKind::Maple;
            return TreeKind::Dead;
    }
}

void lanternPost(BlockSink& w, int x, int y, int z, DecorOut& out) {
    w.set(x, y, z, B_LOG_OAK);
    w.set(x, y + 1, z, B_LOG_OAK);
    w.set(x, y + 2, z, B_LANTERN);
    out.lights.push_back({x + 0.5f, y + 2.6f, z + 0.5f});
}

void decorateCamp(BlockSink& w, const Camp& camp, const Theme& theme, Rng& rng, DecorOut& out) {
    if (isBuildingKind(camp.kind) && !camp.start && !camp.portal) {
        buildBuilding(w, camp, theme, rng, out);
        return;
    }
    int cx = static_cast<int>(std::floor(camp.center.x)), cz = static_cast<int>(std::floor(camp.center.z));
    int y = static_cast<int>(camp.center.y);
    int r = static_cast<int>(camp.radius);
    uint8_t floorA = theme.id == 1 ? B_SANDSTONE : B_COBBLE;
    uint8_t floorB = theme.id == 3 ? B_GRAVEL : B_PLANKS;
    if (camp.kind == K_GRAVEYARD || camp.kind == K_FARM) floorA = B_PATH;
    for (int dz = -r; dz <= r; ++dz)
        for (int dx = -r; dx <= r; ++dx) {
            float d = std::sqrt(static_cast<float>(dx * dx + dz * dz));
            if (d > r - 0.5f) continue;
            float n = hashFloat(cx + dx, cz + dz, 77);
            if (d < 3.2f || n < 0.55f) w.set(cx + dx, y - 1, cz + dz, n < 0.3f ? floorB : floorA);
        }
    float base = rng.range(0, kTwoPi);
    auto ringPos = [&](float ang, float dist, int& x, int& z) {
        x = cx + static_cast<int>(std::lround(std::cos(ang) * dist));
        z = cz + static_cast<int>(std::lround(std::sin(ang) * dist));
    };
    bool fire = !camp.portal && (camp.kind == K_FIRE || camp.kind == K_FARM || camp.kind == K_OUTPOST || camp.start);
    if (fire) {
        w.set(cx, y - 1, cz, B_CAMPFIRE);
        out.campfires.push_back({cx + 0.5f, static_cast<float>(y), cz + 0.5f});
        out.lights.push_back({cx + 0.5f, y + 0.8f, cz + 0.5f});
    }
    switch (camp.start ? K_FIRE : camp.kind) {
        case K_RUINS: {
            // Pilares quebrados e muretas baixas (cobertura, nunca paredes fechadas).
            int pillars = rng.rangeInt(5, 8);
            for (int i = 0; i < pillars; ++i) {
                int x, z;
                ringPos(base + i * kTwoPi / pillars, camp.radius - rng.range(0.8f, 2.0f), x, z);
                int h = rng.rangeInt(1, 2);
                for (int k = 0; k < h; ++k) w.set(x, y + k, z, rng.chance(0.3f) ? B_STONE : B_COBBLE);
            }
            for (int i = 0; i < 2; ++i) {
                int x, z;
                ringPos(base + kPi * 0.25f + i * kPi, camp.radius * 0.45f, x, z);
                for (int k = -1; k <= 1; ++k) w.set(x + (i ? k : 0), y, z + (i ? 0 : k), B_COBBLE);
            }
            if (!camp.portal) {
                lanternPost(w, cx + 1, y, cz - 1, out);
                out.chests.push_back({cx - 1.5f, static_cast<float>(y), cz + 1.5f});
            }
            break;
        }
        case K_GRAVEYARD: {
            for (int row = -2; row <= 2; row += 2)
                for (int col = -3; col <= 3; col += 2) {
                    if (rng.chance(0.25f)) continue;
                    int x = cx + col, z = cz + row;
                    w.set(x, y, z, B_COBBLE);
                    if (rng.chance(0.3f)) w.set(x, y + 1, z, B_STONE);
                    if (rng.chance(0.4f)) w.set(x, y, z + 1, theme.plantB);
                }
            lanternPost(w, cx + r - 1, y, cz, out);
            lanternPost(w, cx - r + 1, y, cz, out);
            break;
        }
        case K_DEPOT: {
            int stacks = rng.rangeInt(5, 8);
            for (int i = 0; i < stacks; ++i) {
                int x, z;
                ringPos(base + i * kTwoPi / stacks + rng.range(-0.2f, 0.2f), rng.range(2.5f, camp.radius - 1.0f), x, z);
                int h = rng.rangeInt(1, 2);
                for (int k = 0; k < h; ++k) w.set(x, y + k, z, rng.chance(0.25f) ? B_HAY : B_CRATE);
                if (rng.chance(0.5f)) w.set(x + 1, y, z, B_CRATE);
            }
            lanternPost(w, cx, y, cz, out);
            out.chests.push_back({cx + 1.5f, static_cast<float>(y), cz + 0.5f});
            break;
        }
        case K_FARM: {
            for (int dz = -2; dz <= 2; ++dz)
                for (int dx = 2; dx <= 5; ++dx) {
                    w.set(cx + dx, y - 1, cz + dz, B_DIRT);
                    if (rng.chance(0.8f)) w.set(cx + dx, y, cz + dz, B_TALL_GRASS);
                }
            for (int i = 0; i < 4; ++i) {
                int x, z;
                ringPos(base + kPi + i * 0.5f, camp.radius - 1.5f, x, z);
                w.set(x, y, z, B_HAY);
                if (i % 2 == 0) w.set(x, y + 1, z, B_HAY);
            }
            lanternPost(w, cx - 2, y, cz + 3, out);
            out.chests.push_back({cx - 2.5f, static_cast<float>(y), cz - 1.5f});
            break;
        }
        case K_TOWER: {
            // Torre de vigia: pilares, plataforma alta e escada lateral — um mirante para explorar.
            for (int k = 0; k < 5; ++k) {
                w.set(cx - 1, y + k, cz - 1, B_LOG_OAK);
                w.set(cx + 1, y + k, cz - 1, B_LOG_OAK);
                w.set(cx - 1, y + k, cz + 1, B_LOG_OAK);
                w.set(cx + 1, y + k, cz + 1, B_LOG_OAK);
            }
            for (int dz = -2; dz <= 2; ++dz)
                for (int dx = -2; dx <= 2; ++dx) w.set(cx + dx, y + 5, cz + dz, B_PLANKS);
            // Degraus maciços que sobem rente à face -Z até a plataforma.
            for (int k = 0; k < 5; ++k)
                for (int h = 0; h <= k; ++h) w.set(cx - 5 + k, y + h, cz - 3, B_COBBLE);
            w.set(cx + 2, y + 6, cz + 2, B_LOG_OAK);
            w.set(cx - 2, y + 6, cz + 2, B_LOG_OAK);
            w.set(cx + 2, y + 6, cz - 2, B_LOG_OAK);
            w.set(cx + 2, y + 7, cz + 2, B_LANTERN);
            out.lights.push_back({cx + 2.5f, y + 7.6f, cz + 2.5f});
            w.set(cx - 2, y + 7, cz + 2, B_LANTERN);
            out.lights.push_back({cx - 1.5f, y + 7.6f, cz + 2.5f});
            out.chests.push_back({cx + 0.5f, static_cast<float>(y + 6), cz + 0.5f});
            for (int i = 0; i < 3; ++i) {
                int x, z;
                ringPos(base + i * 2.1f, camp.radius - 1.5f, x, z);
                w.set(x, y, z, rng.chance(0.5f) ? B_HAY : B_CRATE);
            }
            break;
        }
        case K_OUTPOST: {
            uint8_t cloth = theme.id == 1 ? B_SANDSTONE : (theme.id == 2 ? B_SNOW : B_HAY);
            for (int i = 0; i < 3; ++i) {
                int x, z;
                ringPos(base + i * kTwoPi / 3, camp.radius - 3.0f, x, z);
                tent(w, x, y, z, i % 2 == 0, cloth);
            }
            int x, z;
            ringPos(base + kPi / 3, camp.radius - 2.0f, x, z);
            w.set(x, y, z, B_CRATE);
            w.set(x, y + 1, z, B_CRATE);
            lanternPost(w, cx + 2, y, cz + 2, out);
            out.chests.push_back({cx - 1.5f, static_cast<float>(y), cz - 1.5f});
            break;
        }
        default: {
            int covers = camp.start ? 3 : rng.rangeInt(4, 6);
            for (int i = 0; i < covers; ++i) {
                float ang = base + i * kTwoPi / covers + rng.range(-0.3f, 0.3f);
                int x, z;
                ringPos(ang, camp.radius - rng.range(1.5f, 2.5f), x, z);
                bool hay = rng.chance(0.35f);
                int height = hay ? 1 : rng.rangeInt(1, 2);
                int width = rng.chance(0.5f) ? 2 : 1;
                bool alongZ = std::fabs(std::cos(ang)) > 0.7f;
                for (int wv = 0; wv < width; ++wv)
                    for (int hy = 0; hy < height; ++hy)
                        w.set(x + (alongZ ? 0 : wv), y + hy, z + (alongZ ? wv : 0), hay ? B_HAY : B_CRATE);
            }
            for (int i = 0; i < 2; ++i) {
                int x, z;
                ringPos(base + kPi * 0.5f + i * kPi, camp.radius - 0.5f, x, z);
                lanternPost(w, x, y, z, out);
            }
            break;
        }
    }
}

void buildPortal(BlockSink& w, const Camp& camp, const Vec3& towards, Vec3& base, float& yaw, DecorOut& out) {
    int cx = static_cast<int>(std::floor(camp.center.x)), cz = static_cast<int>(std::floor(camp.center.z));
    int y = static_cast<int>(camp.center.y);
    Vec3 dir = normalize(Vec3(towards.x - camp.center.x, 0, towards.z - camp.center.z));
    bool alongZ = std::fabs(dir.x) > std::fabs(dir.z);  // portal voltado para ±X: moldura ao longo de Z
    for (int a = -2; a <= 2; ++a) {
        for (int dy = -1; dy <= 4; ++dy) {
            int x = alongZ ? cx : cx + a;
            int z = alongZ ? cz + a : cz;
            bool frame = std::abs(a) == 2 || dy == -1 || dy == 4;
            w.set(x, y + dy, z, frame ? B_OBSIDIAN : B_AIR);
        }
    }
    base = {cx + 0.5f, static_cast<float>(y), cz + 0.5f};
    yaw = alongZ ? (dir.x > 0 ? yawFromDirection(1, 0) : yawFromDirection(-1, 0))
                 : (dir.z > 0 ? yawFromDirection(0, 1) : yawFromDirection(0, -1));
    out.lights.push_back({cx + 0.5f, y + 2.0f, cz + 0.5f});
}

void buildNest(BlockSink& w, const Camp& camp, Rng& rng, DecorOut& out, Vec3& core) {
    int cx = static_cast<int>(std::floor(camp.center.x)), cz = static_cast<int>(std::floor(camp.center.z));
    int y = static_cast<int>(camp.center.y);
    int r = static_cast<int>(camp.radius);
    // Chão infectado: rocha avermelhada com veios de magma e cogumelos.
    for (int dz = -r; dz <= r; ++dz)
        for (int dx = -r; dx <= r; ++dx) {
            float d = std::sqrt(static_cast<float>(dx * dx + dz * dz));
            if (d > r - 0.5f) continue;
            float n = hashFloat(cx + dx, cz + dz, 913);
            if (d > r - 2.5f && n < 0.45f) continue;
            w.set(cx + dx, y - 1, cz + dz, n < 0.12f ? B_MAGMA : B_NETHERRACK);
            if (n > 0.9f && d > 3.0f) w.set(cx + dx, y, cz + dz, B_MUSHROOM_RED);
        }
    // Espinhos de obsidiana ao redor.
    int spikes = rng.rangeInt(6, 9);
    float base = rng.range(0, kTwoPi);
    for (int i = 0; i < spikes; ++i) {
        float ang = base + i * kTwoPi / spikes + rng.range(-0.2f, 0.2f);
        float dist = camp.radius - rng.range(1.5f, 3.0f);
        int x = cx + static_cast<int>(std::lround(std::cos(ang) * dist));
        int z = cz + static_cast<int>(std::lround(std::sin(ang) * dist));
        int h = rng.rangeInt(1, 2);
        for (int k = 0; k < h; ++k) w.set(x, y + k, z, k == h - 1 && rng.chance(0.5f) ? B_MAGMA : B_OBSIDIAN);
    }
    // Pedestal central.
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) w.set(cx + dx, y, cz + dz, (dx == 0 && dz == 0) ? B_MAGMA : B_OBSIDIAN);
    core = {cx + 0.5f, y + 2.6f, cz + 0.5f};
    out.lights.push_back({cx + 0.5f, y + 2.0f, cz + 0.5f});
}

}  // namespace zz
