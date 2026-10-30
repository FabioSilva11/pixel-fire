#pragma once
#include "world.h"
#include "competitive_map.h"
#include <stdexcept>

namespace zz {
class CompetitiveMapSource : public ChunkSource {
  public:
    br::CompetitiveMap map;
    bool transferToClient = false;
    void generate(Chunk &chunk, int height) override {
        if (height != 128)
            throw std::runtime_error("Competitive map height mismatch");
        if (transferToClient)
            map.transferChunkToClient(chunk.cx, chunk.cz, chunk.blocks);
        else
            map.copyChunk(chunk.cx, chunk.cz, chunk.blocks);
        chunk.maxY = 0;
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                const int wx = chunk.cx * 16 + x, wz = chunk.cz * 16 + z, ground = map.heightAt(wx, wz);
                chunk.ground[z * 16 + x] = int16_t(ground);
                for (int y = height - 1; y >= 0; --y)
                    if (chunk.blocks[(y << 8) | (z << 4) | x] != B_AIR) {
                        chunk.maxY = std::max(chunk.maxY, y + 1);
                        break;
                    }
            }
        map.copyVisuals(chunk.cx, chunk.cz, chunk);
        chunk.generated = true;
        chunk.dirty = true;
    }
};
} // namespace zz
