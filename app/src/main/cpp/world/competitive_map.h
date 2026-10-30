#pragma once
// Finite Horizonte terrain with fixed competitive architecture and baked visual proxies.
// Server collision and Android chunks read identical canonical voxel data.
#include <cstdint>
#include <vector>
#include <memory>
#include <istream>
#include <ostream>
#include <functional>
#include "../core/math.h"
#include "blocks.h"

namespace zz {
class IslandGenerator;
struct Chunk;
} // namespace zz
namespace zz::br {
void configureCompetitiveMission(IslandGenerator &mission);
constexpr int kMapSize = 1440, kMapHeight = 128, kMapVersion = 4, kMissionOrigin = 2352;
constexpr uint32_t kMapSeed = 0x5A4F4E45;
struct MapRegion {
    const char *name = nullptr;
    int x = 0, z = 0, elevation = 0, style = 0;
};
struct MapBuilding {
    int x = 0, z = 0, width = 0, depth = 0, height = 0, rotation = 0, floor = 0, style = 0;
    int lootX = 0, lootY = 0, lootZ = 0;
};
class CompetitiveMap {
  public:
    explicit CompetitiveMap(bool loadDefault = true);
    int size() const { return kMapSize; }
    uint32_t hash() const;
    bool missionTerrain() const;
    const std::vector<MapRegion> &regions() const;
    const std::vector<MapBuilding> &buildings() const;
    bool inBounds(int x, int z) const { return x >= 0 && z >= 0 && x < kMapSize && z < kMapSize; }
    int heightAt(int x, int z) const;
    uint8_t blockAt(int x, int y, int z) const;
    bool solidAt(float x, float y, float z) const;
    uint32_t mapColor(int x, int z) const;
    Vec3 doorPosition(const MapBuilding &b) const { return {float(b.lootX), float(b.lootY), float(b.lootZ)}; }
    void copyChunk(int cx, int cz, std::vector<uint8_t> &blocks) const;
    void transferChunkToClient(int cx, int cz, std::vector<uint8_t> &blocks);
    void copyVisuals(int cx, int cz, Chunk &chunk) const;
    // Client calls only after World owns all canonical collider chunks. Server keeps its expanded copy.
    void releaseExpandedForClient();
    bool collisionResident() const;
    size_t residentChunkCount() const;
    void save(std::ostream &out) const;
    bool load(std::istream &in);
    static CompetitiveMap bakeMission(const std::function<void(int, int)> &progress = {});

  private:
    struct Data;
    std::shared_ptr<Data> data_;
    static std::shared_ptr<Data> &sharedData();
    const std::vector<uint8_t> &decodedChunk(int index) const;
    static uint32_t calculateHash(const Data &data);
};
} // namespace zz::br
