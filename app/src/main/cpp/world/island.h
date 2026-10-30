// Terreno contínuo Horizonte gerado por chunks: três biomas, rios, cavernas,
// aldeias no relevo terrestre. O mapa competitivo é preparado previamente.
#pragma once

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../core/math.h"
#include "structures.h"
#include "theme.h"
#include "world.h"
#include "horizonte_adapter.h"

namespace zz {

constexpr int kIslandSize = 6144;       // limite horizontal do mundo contínuo
constexpr int kIslandHeight = horizonte::kHeight;
constexpr int kIslandCell = 128;        // grade de pontos de interesse (múltiplo de 16)
constexpr int kIslandCells = kIslandSize / kIslandCell;
constexpr float kIslandCenter = kIslandSize * 0.5f;

enum PoiRole : uint8_t { ROLE_NONE = 0, ROLE_START };

struct IslandPoi {
    bool exists = false;
    int cellX = 0, cellZ = 0;
    int id = -1;          // cellZ * kIslandCells + cellX
    Vec3 center;          // pés no centro (y = chão nivelado)
    float radius = 7;
    int kind = K_FIRE;
    PoiRole role = ROLE_NONE;
};

struct PoiStamp {
    std::unordered_map<uint64_t, uint8_t> edits;  // position_key.h: 13+13+7 bits
    int minX = 0, maxX = -1, minZ = 0, maxZ = -1;
    DecorOut decor;
};

struct ColumnInfo {
    int top = 0;          // primeiro y livre acima do terreno
    bool water = false;   // coluna submersa (top <= nível do mar)
    bool path = false;
    bool bridge = false;
    bool nearPath = false;
    bool nearPoi = false;
    float secondary = 0;  // peso do bioma secundário
    const Theme* biome = nullptr;
    float forest = 0;     // densidade de floresta 0..1
    float river = 0;      // 1 no leito do rio
};

class IslandGenerator : public ChunkSource {
public:
    IslandGenerator(const Theme& theme, int stage, uint32_t seed);
    void generate(Chunk& chunk, int height) override;
    void reservePoi(const Vec3& center, int kind, float radius);
    void suppressProceduralPois();

    float landMask(float x, float z) const;
    float secondaryWeight(float x, float z) const;
    float riverMask(float x, float z) const;
    float naturalHeight(float x, float z) const;
    ColumnInfo column(int x, int z);
    uint32_t mapColor(int x, int z);

    const IslandPoi& poi(int cellX, int cellZ);
    const IslandPoi& poiAtCell(int id) { return poi(id % kIslandCells, id / kIslandCells); }
    void poisNear(const Vec3& p, float radius, std::vector<const IslandPoi*>& out);
    const PoiStamp& stamp(const IslandPoi& poi);
    bool hasStamp(const IslandPoi& poi) const { return stamps_.count(poi.id) != 0; }

    int seaLevel() const { return sea_; }
    const Theme& primary() const { return primary_; }
    const Theme& secondary() const { return secondary_; }
    Vec3 start() const { return start_; }
    float startYaw() const { return startYaw_; }

private:
    IslandPoi basePoi(int cellX, int cellZ) const;
    float surfaceHeight(int x, int z, const IslandPoi& p) const;
    float pathDistance(int x, int z, int cellX, int cellZ);
    bool pathLink(int ax, int az, int bx, int bz);
    void chooseLayout();
    const Theme& biomeTheme(float secondaryWeight, int x, int z) const;

    Theme primary_, secondary_;
    Theme naturalBiomes_[3] = {themeForStage(1), themeForStage(6), themeForStage(11)};
    horizonte::Terrain terrain_;
    int stage_ = 1;
    uint32_t seed_ = 1;
    int sea_ = horizonte::kSea;
    Vec3 start_;
    float startYaw_ = 0;
    std::unordered_map<int, PoiRole> roles_;
    std::vector<IslandPoi> cache_;
    std::vector<uint8_t> cached_;
    std::unordered_map<int, PoiStamp> stamps_;
};

}  // namespace zz
