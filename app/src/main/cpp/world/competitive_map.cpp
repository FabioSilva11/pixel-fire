#include "competitive_map.h"
#include "island.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <unordered_map>

namespace zz::br {
namespace {
constexpr int kChunks = kMapSize / 16, kChunkCount = kChunks * kChunks;
const char *names[] = {"Vila Rubra",      "Templo da Areia", "Mirante Escarlate", "Pedras Antigas",
                       "Pátio Central",   "Fortaleza Seca",  "Beco de Poeira",    "Usina de Ferro",
                       "Cais do Sol",     "Base Eclipse",    "Vale dos Ossos",    "Casco Perdido",
                       "Rocha do Tesouro"};
const int labels[13][2] = {{220, 220},  {690, 190},  {1160, 220}, {430, 410},  {720, 720},
                           {1190, 650}, {220, 700},  {960, 440},  {180, 1190}, {1160, 1160},
                           {470, 1020}, {740, 1240}, {950, 940}};
void write32(std::ostream &out, uint32_t v) {
    uint8_t b[] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
    out.write(reinterpret_cast<char *>(b), 4);
}
uint32_t read32(std::istream &in) {
    uint8_t b[4]{};
    in.read(reinterpret_cast<char *>(b), 4);
    return uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
}
} // namespace
struct CompetitiveMap::Data {
    uint32_t hash = 0;
    bool ready = false;
    std::vector<uint8_t> heights, compressed, surface; // baked 2m sample: top height, voxel material
    std::vector<uint32_t> offsets, colors;
    std::vector<MapRegion> regions;
    std::vector<MapBuilding> buildings;
    std::vector<std::vector<uint8_t>> collision;
};
void configureCompetitiveMission(IslandGenerator &mission) { mission.suppressProceduralPois(); }
std::shared_ptr<CompetitiveMap::Data> &CompetitiveMap::sharedData() {
    static std::shared_ptr<Data> data;
    return data;
}
CompetitiveMap::CompetitiveMap(bool loadDefault) {
    if (loadDefault && sharedData() && sharedData()->ready) {
        data_ = sharedData();
        return;
    }
    data_ = std::make_shared<Data>();
#if !defined(__ANDROID__)
    if (loadDefault) {
        const char *path = std::getenv("ZZ_MAP_FILE");
        std::ifstream in(path ? path : "/var/lib/zombie-zone/terra-escarlate.zzmap", std::ios::binary);
        if (in)
            load(in);
    }
#endif
}
uint32_t CompetitiveMap::hash() const { return data_->hash; }
bool CompetitiveMap::missionTerrain() const { return data_->ready; }
const std::vector<MapRegion> &CompetitiveMap::regions() const { return data_->regions; }
const std::vector<MapBuilding> &CompetitiveMap::buildings() const { return data_->buildings; }
int CompetitiveMap::heightAt(int x, int z) const {
    return data_->ready && inBounds(x, z) ? data_->heights[z * kMapSize + x] : 127;
}
void CompetitiveMap::releaseExpandedForClient() {
    std::vector<std::vector<uint8_t>>{}.swap(data_->collision);
}
bool CompetitiveMap::collisionResident() const {
    return data_->ready && data_->collision.size() == kChunkCount;
}
size_t CompetitiveMap::residentChunkCount() const { return data_->collision.size(); }
const std::vector<uint8_t> &CompetitiveMap::decodedChunk(int index) const {
    if (!data_->ready || index < 0 || index >= kChunkCount)
        throw std::runtime_error("Competitive map is not loaded");
    if (data_->collision.size() != kChunkCount)
        throw std::runtime_error("Client collider ownership was transferred to World");
    return data_->collision[size_t(index)];
}
void CompetitiveMap::copyChunk(int cx, int cz, std::vector<uint8_t> &out) const {
    if (cx < 0 || cz < 0 || cx >= kChunks || cz >= kChunks)
        throw std::runtime_error("Map chunk out of bounds");
    const auto &bytes = decodedChunk(cz * kChunks + cx);
    out.assign(bytes.begin(), bytes.end());
}
void CompetitiveMap::transferChunkToClient(int cx, int cz, std::vector<uint8_t> &out) {
    if (cx < 0 || cz < 0 || cx >= kChunks || cz >= kChunks || data_->collision.size() != kChunkCount)
        throw std::runtime_error("Client chunk transfer out of bounds");
    auto &bytes = data_->collision[size_t(cz * kChunks + cx)];
    if (bytes.size() != 32768)
        throw std::runtime_error("Client chunk already transferred");
    out = std::move(bytes);
}
uint8_t CompetitiveMap::blockAt(int x, int y, int z) const {
    if (y < 0 || !inBounds(x, z))
        return B_BEDROCK;
    if (y >= 128)
        return B_AIR;
    if (!data_->ready)
        return B_BEDROCK;
    const auto &b = decodedChunk((z / 16) * kChunks + x / 16);
    return b[(y << 8) | ((z & 15) << 4) | (x & 15)];
}
bool CompetitiveMap::solidAt(float x, float y, float z) const {
    return isSolid(blockAt(int(std::floor(x)), int(std::floor(y)), int(std::floor(z))));
}
uint32_t CompetitiveMap::mapColor(int x, int z) const {
    if (!data_->ready || !inBounds(x, z))
        return 0xff253248;
    return data_->colors[(z / 8) * 180 + x / 8];
}
uint32_t CompetitiveMap::calculateHash(const Data &d) {
    uint32_t h = 2166136261u ^ kMapVersion;
    auto byte = [&](uint8_t v) { h = (h ^ v) * 16777619u; };
    auto number = [&](uint32_t n) {
        for (int i = 0; i < 4; ++i) {
            byte(uint8_t(n));
            n >>= 8;
        }
    };
    for (auto v : d.surface)
        byte(v);
    for (auto v : d.heights)
        byte(v);
    for (auto v : d.colors)
        number(v);
    for (auto v : d.offsets)
        number(v);
    for (auto v : d.compressed)
        byte(v);
    for (const auto &b : d.buildings) {
        const int fields[] = {b.x,     b.z,     b.width, b.depth, b.height, b.rotation,
                              b.floor, b.style, b.lootX, b.lootY, b.lootZ};
        for (int v : fields)
            number(uint32_t(v));
    }
    for (const auto &r : d.regions) {
        number(r.x);
        number(r.z);
        number(r.elevation);
        number(r.style);
    }
    return h;
}
void CompetitiveMap::save(std::ostream &out) const {
    const auto &d = *data_;
    if (!d.ready) {
        out.setstate(std::ios::failbit);
        return;
    }
    const uint32_t header[] = {0x504D5A5A,
                               kMapVersion,
                               kMapSize,
                               d.hash,
                               kMapHeight,
                               kMapSeed,
                               kMissionOrigin,
                               kMissionOrigin,
                               kChunkCount,
                               uint32_t(d.heights.size()),
                               uint32_t(d.colors.size()),
                               uint32_t(d.compressed.size()),
                               uint32_t(d.buildings.size()),
                               uint32_t(d.regions.size()),
                               uint32_t(d.surface.size()),
                               0};
    for (uint32_t v : header)
        write32(out, v);
    for (uint32_t v : d.offsets)
        write32(out, v);
    out.write(reinterpret_cast<const char *>(d.heights.data()), d.heights.size());
    for (uint32_t v : d.colors)
        write32(out, v);
    for (const auto &b : d.buildings) {
        const int f[] = {b.x,     b.z,     b.width, b.depth, b.height, b.rotation,
                         b.floor, b.style, b.lootX, b.lootY, b.lootZ};
        for (int v : f)
            write32(out, uint32_t(v));
    }
    for (const auto &r : d.regions) {
        write32(out, r.x);
        write32(out, r.z);
        write32(out, r.elevation);
        write32(out, r.style);
    }
    out.write(reinterpret_cast<const char *>(d.compressed.data()), d.compressed.size());
    out.write(reinterpret_cast<const char *>(d.surface.data()), d.surface.size());
}
bool CompetitiveMap::load(std::istream &in) {
    std::array<uint32_t, 16> h{};
    for (auto &v : h)
        v = read32(in);
    if (!in || h[0] != 0x504D5A5A || h[1] != kMapVersion || h[2] != kMapSize || h[4] != 128 ||
        h[5] != kMapSeed || h[6] != kMissionOrigin || h[7] != kMissionOrigin || h[8] != kChunkCount ||
        h[9] != kMapSize * kMapSize || h[10] != 180 * 180 || h[11] > 128 * 1024 * 1024 || h[12] != 52 ||
        h[13] != 13 || h[14] != 720 * 720 * 2 || h[15] != 0)
        return false;
    auto candidate = std::make_shared<Data>();
    auto &d = *candidate;
    d.offsets.resize(kChunkCount + 1);
    for (auto &n : d.offsets)
        n = read32(in);
    if (d.offsets.front() != 0 || d.offsets.back() != h[11])
        return false;
    for (size_t i = 1; i < d.offsets.size(); ++i)
        if (d.offsets[i] < d.offsets[i - 1] || (d.offsets[i] - d.offsets[i - 1]) % 3 != 0)
            return false;
    d.heights.resize(h[9]);
    in.read(reinterpret_cast<char *>(d.heights.data()), d.heights.size());
    d.colors.resize(h[10]);
    for (auto &n : d.colors)
        n = read32(in);
    d.buildings.resize(h[12]);
    for (auto &b : d.buildings) {
        int *f[] = {&b.x,     &b.z,     &b.width, &b.depth, &b.height, &b.rotation,
                    &b.floor, &b.style, &b.lootX, &b.lootY, &b.lootZ};
        for (auto n : f)
            *n = int(read32(in));
        if (b.x < 0 || b.z < 0 || b.x >= 1440 || b.z >= 1440 || b.lootX < 0 || b.lootZ < 0 ||
            b.lootX >= 1440 || b.lootZ >= 1440 || b.lootY < 1 || b.lootY >= 126 || b.width < 4 ||
            b.depth < 4 || b.width > 64 || b.depth > 64 || b.height < 1 || b.height > 60 || b.floor < 1 ||
            b.floor + b.height >= 127 || b.style < 0 || b.style >= 47 || b.rotation < 0 ||
            b.rotation >= 360 || b.rotation % 90)
            return false;
    }
    for (int i = 0; i < 13; ++i) {
        MapRegion r;
        r.name = names[i];
        r.x = int(read32(in));
        r.z = int(read32(in));
        r.elevation = int(read32(in));
        r.style = int(read32(in));
        d.regions.push_back(r);
    }
    d.compressed.resize(h[11]);
    in.read(reinterpret_cast<char *>(d.compressed.data()), d.compressed.size());
    if (!in)
        return false;
    for (int i = 0; i < kChunkCount; ++i) {
        uint32_t total = 0;
        for (uint32_t at = d.offsets[i]; at < d.offsets[i + 1]; at += 3) {
            uint32_t n = uint32_t(d.compressed[at]) | (uint32_t(d.compressed[at + 1]) << 8);
            if (n == 0 || d.compressed[at + 2] >= B_COUNT)
                return false;
            total += n;
            if (total > 32768)
                return false;
        }
        if (total != 32768)
            return false;
    }
    d.surface.resize(h[14]);
    in.read(reinterpret_cast<char *>(d.surface.data()), d.surface.size());
    if (!in)
        return false;
    for (size_t i = 0; i < d.surface.size(); i += 2)
        if (d.surface[i] > 128 || d.surface[i + 1] >= B_COUNT)
            return false;
    if (calculateHash(d) != h[3])
        return false;
    // Explicit round trips/loaders may validate an identical immutable package. Reuse its complete
    // resident collider instead of briefly doubling253MiB on a small dedicated machine.
    const auto resident = sharedData();
    bool sameMetadata = resident && resident->buildings.size() == d.buildings.size() &&
                        resident->regions.size() == d.regions.size();
    if (sameMetadata)
        for (size_t i = 0; i < d.buildings.size(); ++i) {
            const auto &a = resident->buildings[i];
            const auto &b = d.buildings[i];
            sameMetadata = sameMetadata && a.x == b.x && a.z == b.z && a.width == b.width &&
                           a.depth == b.depth && a.height == b.height && a.rotation == b.rotation &&
                           a.floor == b.floor && a.style == b.style && a.lootX == b.lootX &&
                           a.lootY == b.lootY && a.lootZ == b.lootZ;
        }
    if (sameMetadata)
        for (size_t i = 0; i < d.regions.size(); ++i) {
            const auto &a = resident->regions[i];
            const auto &b = d.regions[i];
            sameMetadata =
                sameMetadata && a.x == b.x && a.z == b.z && a.elevation == b.elevation && a.style == b.style;
        }
    if (sameMetadata && resident->ready && resident->hash == h[3] &&
        resident->collision.size() == kChunkCount && resident->compressed == d.compressed &&
        resident->offsets == d.offsets && resident->heights == d.heights && resident->colors == d.colors &&
        resident->surface == d.surface &&
        std::all_of(resident->collision.begin(), resident->collision.end(),
                    [](const auto &chunk) { return chunk.size() == 32768; })) {
        data_ = resident;
        return true;
    }
    // No eviction or lazy reads: the authoritative collider is fully resident at load completion.
    d.collision.resize(kChunkCount);
    for (int i = 0; i < kChunkCount; ++i) {
        auto &bytes = d.collision[i];
        bytes.reserve(32768);
        for (uint32_t at = d.offsets[i]; at < d.offsets[i + 1]; at += 3) {
            const uint32_t n = uint32_t(d.compressed[at]) | (uint32_t(d.compressed[at + 1]) << 8);
            bytes.insert(bytes.end(), n, d.compressed[at + 2]);
        }
    }
    d.hash = h[3];
    d.ready = true;
    data_ = candidate;
    sharedData() = candidate;
    return true;
}
namespace {
struct Anchor {
    int x, z, yaw, style;
};
// Generated from docs/battle-royale/mapa-proposto.json. Coordinates and rotations are canonical.
constexpr Anchor anchors[] = {
    {182, 196, 0, 0},      // r01-b01 casa
    {251, 190, 90, 1},     // r01-b02 comercio
    {196, 254, 180, 2},    // r01-b03 sobrado
    {262, 259, 270, 3},    // r01-b04 oficina
    {652, 166, 0, 4},      // r02-b01 templo
    {721, 160, 90, 5},     // r02-b02 obelisco
    {666, 224, 180, 6},    // r02-b03 patio-monumental
    {732, 229, 270, 7},    // r02-b04 galeria
    {1122, 196, 0, 8},     // r03-b01 torre
    {1191, 190, 90, 9},    // r03-b02 plataforma
    {1136, 254, 180, 10},  // r03-b03 posto-observacao
    {1202, 259, 270, 11},  // r03-b04 passarela
    {392, 386, 0, 12},     // r04-b01 ruina-arcos
    {461, 380, 90, 13},    // r04-b02 ruina-sala
    {406, 444, 180, 14},   // r04-b03 colunata
    {472, 449, 270, 15},   // r04-b04 patio-pedra
    {682, 696, 0, 16},     // r05-b01 predio
    {751, 690, 90, 3},     // r05-b02 oficina
    {696, 754, 180, 17},   // r05-b03 galpao
    {762, 759, 270, 18},   // r05-b04 escritorio
    {1152, 626, 0, 19},    // r06-b01 portao-forte
    {1221, 620, 90, 20},   // r06-b02 quartel
    {1166, 684, 180, 21},  // r06-b03 bastiao
    {1232, 689, 270, 22},  // r06-b04 deposito
    {182, 676, 0, 23},     // r07-b01 casa-estreita
    {251, 670, 90, 24},    // r07-b02 comercio-becco
    {196, 734, 180, 2},    // r07-b03 sobrado
    {262, 739, 270, 25},   // r07-b04 patio-murado
    {922, 416, 0, 26},     // r08-b01 nave-industrial
    {991, 410, 90, 27},    // r08-b02 torre-industrial
    {936, 474, 180, 28},   // r08-b03 oficina-metal
    {1002, 479, 270, 29},  // r08-b04 tanques
    {142, 1166, 0, 30},    // r09-b01 armazem
    {211, 1160, 90, 31},   // r09-b02 doca
    {156, 1224, 180, 32},  // r09-b03 deposito-cais
    {222, 1229, 270, 33},  // r09-b04 posto-portuario
    {1122, 1136, 0, 20},   // r10-b01 quartel
    {1191, 1130, 90, 34},  // r10-b02 hangar
    {1136, 1194, 180, 35}, // r10-b03 posto-comando
    {1202, 1199, 270, 36}, // r10-b04 deposito-militar
    {432, 996, 0, 37},     // r11-b01 fosseis
    {501, 990, 90, 38},    // r11-b02 entrada-caverna
    {446, 1054, 180, 39},  // r11-b03 abrigo-rocha
    {512, 1059, 270, 40},  // r11-b04 galeria-subterranea
    {702, 1216, 0, 41},    // r12-b01 casco-abandonado
    {771, 1210, 90, 42},   // r12-b02 cabine
    {716, 1274, 180, 43},  // r12-b03 compartimento
    {782, 1279, 270, 44},  // r12-b04 passarela-casco
    {912, 916, 0, 38},     // r13-b01 entrada-caverna
    {981, 910, 90, 45},    // r13-b02 deposito-rocha
    {926, 974, 180, 46},   // r13-b03 galeria-mineral
    {992, 979, 270, 15},   // r13-b04 patio-pedra
};
struct Recipe {
    int width, depth, height, family;
};
constexpr Recipe recipes[] = {
    {14, 12, 7, 0},  // 0 casa
    {14, 12, 7, 0},  // 1 comercio
    {18, 16, 13, 8}, // 2 sobrado
    {14, 12, 7, 0},  // 3 oficina
    {22, 18, 11, 3}, // 4 templo
    {10, 10, 13, 1}, // 5 obelisco
    {22, 14, 4, 2},  // 6 patio-monumental
    {14, 12, 7, 0},  // 7 galeria
    {10, 10, 20, 1}, // 8 torre
    {22, 14, 4, 2},  // 9 plataforma
    {14, 12, 7, 0},  // 10 posto-observacao
    {22, 14, 4, 2},  // 11 passarela
    {22, 18, 11, 3}, // 12 ruina-arcos
    {22, 18, 11, 3}, // 13 ruina-sala
    {22, 14, 4, 2},  // 14 colunata
    {22, 14, 4, 2},  // 15 patio-pedra
    {18, 16, 13, 8}, // 16 predio
    {28, 20, 12, 7}, // 17 galpao
    {18, 16, 13, 8}, // 18 escritorio
    {22, 18, 11, 3}, // 19 portao-forte
    {18, 16, 13, 8}, // 20 quartel
    {10, 10, 13, 1}, // 21 bastiao
    {14, 12, 7, 0},  // 22 deposito
    {10, 18, 7, 0},  // 23 casa-estreita
    {14, 12, 7, 0},  // 24 comercio-becco
    {22, 14, 4, 2},  // 25 patio-murado
    {28, 20, 12, 7}, // 26 nave-industrial
    {10, 10, 20, 1}, // 27 torre-industrial
    {14, 12, 7, 0},  // 28 oficina-metal
    {24, 14, 11, 6}, // 29 tanques
    {28, 20, 12, 7}, // 30 armazem
    {22, 14, 4, 2},  // 31 doca
    {14, 12, 7, 0},  // 32 deposito-cais
    {14, 12, 7, 0},  // 33 posto-portuario
    {28, 20, 12, 7}, // 34 hangar
    {18, 16, 13, 8}, // 35 posto-comando
    {14, 12, 7, 0},  // 36 deposito-militar
    {22, 14, 4, 2},  // 37 fosseis
    {22, 18, 9, 4},  // 38 entrada-caverna
    {22, 18, 9, 4},  // 39 abrigo-rocha
    {22, 18, 9, 4},  // 40 galeria-subterranea
    {24, 12, 8, 5},  // 41 casco-abandonado
    {24, 12, 8, 5},  // 42 cabine
    {24, 12, 8, 5},  // 43 compartimento
    {24, 12, 8, 5},  // 44 passarela-casco
    {22, 18, 9, 4},  // 45 deposito-rocha
    {22, 18, 9, 4},  // 46 galeria-mineral
};
std::pair<int, int> localPoint(const MapBuilding &b, int wx, int wz) {
    int dx = wx - b.x, dz = wz - b.z;
    switch (b.rotation) {
    case 90:
        return {dz, -dx};
    case 180:
        return {-dx, -dz};
    case 270:
        return {-dz, dx};
    default:
        return {dx, dz};
    }
}
std::pair<int, int> worldPoint(const MapBuilding &b, int u, int v) {
    switch (b.rotation) {
    case 90:
        return {b.x - v, b.z + u};
    case 180:
        return {b.x - u, b.z - v};
    case 270:
        return {b.x + v, b.z - u};
    default:
        return {b.x + u, b.z + v};
    }
}
void buildArchitecture(Chunk &c, const std::vector<MapBuilding> &buildings) {
    const int x0 = (c.cx - kMissionOrigin / 16) * 16, z0 = (c.cz - kMissionOrigin / 16) * 16;
    for (const auto &b : buildings) {
        const auto &r = recipes[b.style];
        const int hw = b.width / 2, hd = b.depth / 2;
        if (std::abs(x0 + 8 - b.x) > b.width + b.depth || std::abs(z0 + 8 - b.z) > b.width + b.depth)
            continue;
        const uint8_t wall = r.family == 4   ? B_STONE
                             : r.family == 5 ? B_PLANKS
                             : r.family >= 6 ? B_CONCRETE
                             : r.family == 3 ? B_SANDSTONE
                                             : B_PLASTER;
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                auto uv = localPoint(b, x0 + x, z0 + z);
                int u = uv.first, v = uv.second;
                if (std::abs(u) > hw + 2 || std::abs(v) > hd + 2)
                    continue;
                const bool inside = std::abs(u) <= hw && std::abs(v) <= hd;
                // Level a foundation and remove trees/old village fragments throughout the footprint.
                for (int y = std::max(1, b.floor - 12); y < std::min(127, b.floor + b.height + 5); ++y) {
                    uint8_t block = y < b.floor - 1 ? B_STONE : y == b.floor - 1 ? B_STONE_BRICK : B_AIR;
                    const int h = y - b.floor;
                    if (inside && h >= 0 && h <= b.height) {
                        bool edge = std::abs(u) == hw || std::abs(v) == hd;
                        bool door = (v == -hd && std::abs(u) <= 1 && h < 3);
                        bool window = (h >= 2 && h <= 3 &&
                                       ((std::abs(u) % 6 <= 1 && std::abs(v) == hd) ||
                                        (std::abs(v) % 6 <= 1 && std::abs(u) == hw)));
                        if (r.family ==
                            2) { // Docks, walkways and courtyards: open pillars and waist-high cover.
                            if (h == 0)
                                block = B_PLANKS;
                            if (h > 0 && h < 4 && std::abs(u) % 6 == 0 && std::abs(v) == hd && !door)
                                block = B_LOG_OAK;
                            if (h == 1 && edge && !door && (b.style % 2))
                                block = B_STONE_BRICK;
                        } else if (r.family == 6) { // Three separate hollow industrial tanks, connected by a
                                                    // service deck.
                            int du = std::min({std::abs(u + 8), std::abs(u), std::abs(u - 8)}),
                                d = du * du + v * v;
                            if (d >= 10 && d <= 17 && h < b.height)
                                block = B_CONCRETE;
                            if (d <= 17 && h == b.height)
                                block = B_ROOF_SLATE;
                            if (v < 0 && std::abs(u) <= 1 && h < 3)
                                block = B_AIR;
                        } else {
                            if (edge && h < b.height && !door && !window)
                                block = wall;
                            if (window && r.family != 3 && r.family != 4)
                                block = B_GLASS;
                            if (h == b.height)
                                block = r.family == 4   ? B_STONE
                                        : r.family == 5 ? B_PLANKS
                                        : r.family >= 6 ? B_ROOF_SLATE
                                                        : B_ROOF_RED;
                            if (r.family == 3) { // Monumental arches/ruins preserve broad openings and
                                                 // intermittent broken crowns.
                                if (edge && std::abs(u) % 7 < 3 && h < 6)
                                    block = B_AIR;
                                if (h == b.height && (u + v + b.style) % 4 == 0)
                                    block = B_AIR;
                                if (std::abs(u) == hw && std::abs(v) % 6 == 0 && h < b.height)
                                    block = B_SANDSTONE;
                            }
                            if (r.family == 5 && std::abs(u) > hw - std::abs(v) / 3 && h < 3)
                                block = B_AIR;
                            // Accessible upper floors with an open stairwell and a physical voxel staircase.
                            if ((r.family == 8 || r.family == 1) && h == 6 && u < hw - 3)
                                block = B_PLANKS;
                            if ((r.family == 8 || r.family == 1) && u >= hw - 2 && u < hw && v >= -hd + 2 &&
                                v < -hd + 8 && h == (v + hd - 2))
                                block = B_PLANKS;
                            if (r.family == 7 && v == -hd && std::abs(u) <= 3 && h < 6)
                                block = B_AIR;
                            if (r.family == 4 && v == -hd && std::abs(u) <= 2 && h < 4)
                                block = B_AIR;
                        }
                        // Original interior cover layout differs by archetype, with a clear central
                        // circulation lane.
                        if (r.family != 2 && r.family != 6 && h == 0 && std::abs(u) > 2 &&
                            std::abs(u) < hw - 1 && v == hd - 3 && (u + b.style) % 4 == 0)
                            block = B_CRATE;
                    }
                    c.blocks[(y << 8) | (z << 4) | x] = block;
                }
                c.ground[z * 16 + x] = int16_t(b.floor + (r.family == 2 && inside ? 1 : 0));
            }
    }
}
// Proxy skirts close terrain cracks at LOD boundaries; all coordinates remain in chunk-local eighths.
void proxyQuad(ChunkMesh &mesh, int x0, int z0, int x1, int z1, int y0, int y1, uint8_t material, int face) {
    const auto &info = blockInfo(material);
    auto &verts = isWaterBlock(material) ? mesh.water : mesh.opaque;
    const int corners[3][4][3] = {{{x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}, {x0, y0, z0}},
                                  {{x0, y0, z0}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z0}},
                                  {{x1, y0, z1}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z1}}};
    const uint8_t uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    for (int i = 0; i < 4; ++i)
        verts.push_back({int16_t(corners[face][i][0] * 8), int16_t(corners[face][i][1] * 8),
                         int16_t(corners[face][i][2] * 8), uint8_t(uv[i][0] * std::max(1, x1 - x0 + z1 - z0)),
                         uint8_t(uv[i][1] * std::max(1, y1 - y0)), face == 0 ? info.top : info.side,
                         uint8_t(face == 0 ? 255 : 190), uint8_t(isWaterBlock(material) ? VF_WATER : 0), 0});
}
} // namespace

void CompetitiveMap::copyVisuals(int cx, int cz, Chunk &c) const {
    c.hasLods = true;
    c.detailedLandmark = false;
    for (const auto &b : data_->buildings)
        if (std::abs(cx * 16 + 8 - b.x) <= b.width + b.depth &&
            std::abs(cz * 16 + 8 - b.z) <= b.width + b.depth)
            c.detailedLandmark = true;
    const int gx = (cx / 4) * 4, gz = (cz / 4) * 4;
    c.hlodBounds = {{float(gx * 16), 0, float(gz * 16)},
                    {float(std::min(gx + 4, kChunks) * 16), 128, float(std::min(gz + 4, kChunks) * 16)}};
    auto sample = [&](int x, int z) {
        x = std::max(0, std::min(kMapSize - 1, x));
        z = std::max(0, std::min(kMapSize - 1, z));
        size_t i = ((z / 2) * 720 + x / 2) * 2;
        return std::pair<int, uint8_t>{data_->surface[i], data_->surface[i + 1]};
    };
    auto build = [&](ChunkMesh &mesh, int spanX, int spanZ, int step) {
        mesh = ChunkMesh{};
        const int ox = cx * 16, oz = cz * 16;
        auto cell = [&](int x, int z) {
            std::pair<int, uint8_t> result{0, B_AIR};
            if (ox + x < 0 || oz + z < 0 || ox + x >= kMapSize || oz + z >= kMapSize)
                return result;
            for (int dz = 0; dz < step; dz += 2)
                for (int dx = 0; dx < step; dx += 2) {
                    auto t = sample(ox + x + dx, oz + z + dz);
                    if (t.first > result.first)
                        result = t;
                }
            return result;
        };
        for (int z = 0; z < spanZ; z += step)
            for (int x = 0; x < spanX; x += step) {
                auto s = cell(x, z);
                int top = s.first;
                uint8_t block = s.second;
                if (!top || block == B_AIR)
                    continue;
                proxyQuad(mesh, x, z, x + step, z + step, top, top, block, 0);
                int lower = cell(x - step, z).first;
                if (lower < top)
                    proxyQuad(mesh, x, z, x, z + step, lower, top, block, 1);
                lower = cell(x + step, z).first;
                if (lower < top)
                    proxyQuad(mesh, x + step, z, x + step, z + step, lower, top, block, 2);
                lower = cell(x, z - step).first;
                if (lower < top)
                    proxyQuad(mesh, x, z, x + step, z, lower, top, block, 2);
                lower = cell(x, z + step).first;
                if (lower < top)
                    proxyQuad(mesh, x, z + step, x + step, z + step, lower, top, block, 1);
            }
    };
    if (!c.detailedLandmark)
        build(c.lodMesh[0], 16, 16, 2);
    if (cx % 4 == 0 && cz % 4 == 0)
        build(c.lodMesh[1], std::min(4, kChunks - cx) * 16, std::min(4, kChunks - cz) * 16, 8);
}

CompetitiveMap CompetitiveMap::bakeMission(const std::function<void(int, int)> &progress) {
    CompetitiveMap map(false);
    auto &d = *map.data_;
    d.heights.resize(kMapSize * kMapSize);
    d.colors.resize(180 * 180);
    d.surface.resize(720 * 720 * 2);
    d.offsets.reserve(kChunkCount + 1);
    d.compressed.reserve(16 * 1024 * 1024);
    IslandGenerator mission(themeForStage(1), 1, kMapSeed);
    configureCompetitiveMission(mission);
    for (const auto &a : anchors) {
        const auto &r = recipes[a.style];
        int floor = std::max(mission.seaLevel() + 2,
                             std::min(90, int(mission.naturalHeight(float(a.x + kMissionOrigin),
                                                                    float(a.z + kMissionOrigin)))));
        MapBuilding b{a.x, a.z, r.width, r.depth, r.height, a.yaw, floor, a.style, 0, floor, 0};
        auto door = worldPoint(b, 0, -r.depth / 2);
        b.lootX = door.first;
        b.lootZ = door.second;
        b.lootY = b.floor + (r.family == 2 ? 1 : 0);
        d.buildings.push_back(b);
    }
    for (int cz = 0; cz < kChunks; ++cz)
        for (int cx = 0; cx < kChunks; ++cx) {
            Chunk chunk;
            chunk.cx = cx + kMissionOrigin / 16;
            chunk.cz = cz + kMissionOrigin / 16;
            mission.generate(chunk, 128);
            buildArchitecture(chunk, d.buildings);
            d.offsets.push_back(uint32_t(d.compressed.size()));
            for (size_t at = 0; at < chunk.blocks.size();) {
                const uint8_t value = chunk.blocks[at];
                size_t end = at + 1;
                while (end < chunk.blocks.size() && chunk.blocks[end] == value && end - at < 65535)
                    ++end;
                const uint32_t count = uint32_t(end - at);
                d.compressed.push_back(uint8_t(count));
                d.compressed.push_back(uint8_t(count >> 8));
                d.compressed.push_back(value);
                at = end;
            }
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x) {
                    const int wx = cx * 16 + x, wz = cz * 16 + z;
                    d.heights[wz * 1440 + wx] =
                        uint8_t(std::max(1, std::min(127, int(chunk.ground[z * 16 + x]))));
                    if (wx % 2 == 0 && wz % 2 == 0) {
                        int top = 127;
                        while (top > 0 &&
                               (chunk.blocks[(top << 8) | (z << 4) | x] == B_AIR ||
                                blockInfo(chunk.blocks[(top << 8) | (z << 4) | x]).shape == Shape::Cross))
                            --top;
                        uint8_t b = chunk.blocks[(top << 8) | (z << 4) | x];
                        size_t i = ((wz / 2) * 720 + wx / 2) * 2;
                        d.surface[i] = uint8_t(top + 1);
                        d.surface[i + 1] = b;
                        if (wx % 8 == 0 && wz % 8 == 0)
                            d.colors[(wz / 8) * 180 + wx / 8] = 0xff000000u | blockInfo(b).mapColor;
                    }
                }
            if (progress && cx == kChunks - 1)
                progress(cz * kChunks + cx + 1, kChunkCount);
        }
    d.offsets.push_back(uint32_t(d.compressed.size()));
    for (int i = 0; i < 13; ++i) {
        MapRegion r{names[i], labels[i][0], labels[i][1], int(d.heights[labels[i][1] * 1440 + labels[i][0]]),
                    i};
        d.regions.push_back(r);
    }
    d.hash = calculateHash(d);
    d.ready = true;
    // Bake need not expand the entire collider: reload the canonical package to obtain resident collision.
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    map.save(bytes);
    CompetitiveMap resident(false);
    if (!resident.load(bytes))
        throw std::runtime_error("Baked map validation failed");
    return resident;
}
} // namespace zz::br
