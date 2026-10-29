#include "world.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace zz {

namespace {

struct FaceDef {
    int nx, ny, nz;
    int corners[4][3];
    float shade;
};

// Cantos em ordem anti-horária vista de fora; uv: v0(0,1) v1(1,1) v2(1,0) v3(0,0).
const FaceDef kFaces[6] = {
    {1, 0, 0, {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, 0.72f},   // +X
    {-1, 0, 0, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, 0.72f},  // -X
    {0, 1, 0, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}, 1.0f},    // +Y
    {0, -1, 0, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, 0.5f},   // -Y
    {0, 0, 1, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}, 0.86f},   // +Z
    {0, 0, -1, {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, 0.86f},  // -Z
};
const uint8_t kUv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
const float kAoCurve[4] = {0.46f, 0.64f, 0.82f, 1.0f};
constexpr int kPad = kChunkSize + 2;

bool isLeaf(uint8_t b) { return isLeafBlock(b); }

// FNV-1a de 64 bits sobre os blocos do chunk, semeado com a posição. A soma entre chunks é independente da ordem.
uint64_t hashChunk(const Chunk& c) {
    uint64_t h = 1469598103934665603ull ^ (static_cast<uint64_t>(static_cast<uint32_t>(c.cx)) << 32) ^ static_cast<uint32_t>(c.cz);
    for (uint8_t b : c.blocks) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}

}  // namespace

void World::clear() {
    for (Chunk* c : list_)
        if (c->gpu.contextId >= 0) gpuGarbage_.push_back(c->gpu);
    list_.clear();
    grid_.clear();
    unloadCursor_ = 0;
    genEmaMs_ = 2.0f;
    meshEmaMs_ = 1.2f;
    contentHash_ = 0;
    edits_.clear();
    revision_ = 0;
}

void World::reset(int width, int height, int depth) {
    clear();
    ++generation_;
    source_ = nullptr;
    width_ = width;
    height_ = height;
    depth_ = depth;
    chunksX_ = (width + kChunkSize - 1) / kChunkSize;
    chunksZ_ = (depth + kChunkSize - 1) / kChunkSize;
    grid_.resize(static_cast<size_t>(chunksX_) * chunksZ_);
    for (int cz = 0; cz < chunksZ_; ++cz) {
        for (int cx = 0; cx < chunksX_; ++cx) {
            Chunk* c = createChunk(cx, cz);
            c->blocks.assign(static_cast<size_t>(kChunkSize) * kChunkSize * height_, B_AIR);
            c->generated = true;
        }
    }
}

void World::resetStreaming(int width, int height, int depth, ChunkSource* source) {
    clear();
    ++generation_;
    source_ = source;
    width_ = width;
    height_ = height;
    depth_ = depth;
    chunksX_ = (width + kChunkSize - 1) / kChunkSize;
    chunksZ_ = (depth + kChunkSize - 1) / kChunkSize;
    grid_.resize(static_cast<size_t>(chunksX_) * chunksZ_);
}

Chunk* World::createChunk(int cx, int cz) {
    auto chunk = std::make_unique<Chunk>();
    Chunk* c = chunk.get();
    c->cx = cx;
    c->cz = cz;
    c->bounds.min = {static_cast<float>(cx * kChunkSize), 0, static_cast<float>(cz * kChunkSize)};
    c->bounds.max = {static_cast<float>((cx + 1) * kChunkSize), static_cast<float>(height_), static_cast<float>((cz + 1) * kChunkSize)};
    c->listIndex = static_cast<int>(list_.size());
    list_.push_back(c);
    grid_[static_cast<size_t>(cz) * chunksX_ + cx] = std::move(chunk);
    return c;
}

void World::destroyChunk(Chunk* c) {
    if (c->gpu.contextId >= 0) gpuGarbage_.push_back(c->gpu);
    int idx = c->listIndex;
    if (idx >= 0 && idx < static_cast<int>(list_.size())) {
        list_[static_cast<size_t>(idx)] = list_.back();
        list_[static_cast<size_t>(idx)]->listIndex = idx;
        list_.pop_back();
    }
    grid_[static_cast<size_t>(c->cz) * chunksX_ + c->cx].reset();
}

void World::set(int x, int y, int z, uint8_t block, bool markDirty) {
    if (!inBounds(x, y, z)) return;
    Chunk* c = chunkAt(x >> 4, z >> 4);
    if (!c || !c->generated) return;
    const size_t index = blockIndex(x & 15, y, z & 15);
    uint8_t previous = c->blocks[index];
    if (previous == block) return;
    c->blocks[index] = block;
    if (block != B_AIR && y + 1 > c->maxY) c->maxY = y + 1;
    if (!markDirty) return;
    edits_[uint32_t(c->cz * chunksX_ + c->cx)][uint16_t(index)] = block;
    ++revision_;
    lowerGround(*c, x & 15, z & 15);
    if (previous == B_LANTERN || previous == B_CAMPFIRE) refreshLights(*c);
    markDirtyAt(x, z);
    if ((x & 15) == 0) markDirtyAt(x - 1, z);
    if ((x & 15) == 15) markDirtyAt(x + 1, z);
    if ((z & 15) == 0) markDirtyAt(x, z - 1);
    if ((z & 15) == 15) markDirtyAt(x, z + 1);
    if ((x & 15) == 0 && (z & 15) == 0) markDirtyAt(x - 1, z - 1);
    if ((x & 15) == 15 && (z & 15) == 0) markDirtyAt(x + 1, z - 1);
    if ((x & 15) == 0 && (z & 15) == 15) markDirtyAt(x - 1, z + 1);
    if ((x & 15) == 15 && (z & 15) == 15) markDirtyAt(x + 1, z + 1);
}

void World::lowerGround(Chunk& c, int x, int z) {
    int y = std::min(height_, int(c.ground[z * 16 + x]));
    while (y > 1 && !isSolid(c.blocks[blockIndex(x, y - 1, z)])) --y;
    c.ground[z * 16 + x] = int16_t(y);
}

void World::refreshLights(Chunk& c) {
    auto supported = [&](const Vec3& p) {
        int x = int(std::floor(p.x)) - c.cx * 16, z = int(std::floor(p.z)) - c.cz * 16;
        if (x < 0 || z < 0 || x >= 16 || z >= 16) return true;
        for (int y = std::max(0, int(p.y) - 2); y <= std::min(height_ - 1, int(p.y) + 1); ++y)
            if (blockInfo(c.blocks[blockIndex(x,y,z)]).emissive) return true;
        return false;
    };
    auto remove = [&](std::vector<Vec3>& list) {
        list.erase(std::remove_if(list.begin(), list.end(), [&](const Vec3& p) { return !supported(p); }), list.end());
    };
    remove(c.lights);
    remove(c.campfires);
}

void World::restoreEdits(Chunk& c) {
    auto it = edits_.find(uint32_t(c.cz * chunksX_ + c.cx));
    if (it == edits_.end()) return;
    for (const auto& e : it->second)
        if (e.first < c.blocks.size()) c.blocks[e.first] = e.second;
    for (int z=0; z<16; ++z) for (int x=0; x<16; ++x) lowerGround(c,x,z);
    refreshLights(c);
}

void World::markDirtyAt(int x, int z) {
    if (!columnInBounds(x, z)) return;
    Chunk* c = chunkAt(x >> 4, z >> 4);
    if (c) c->dirty = true;
}

void World::markAllDirty() {
    for (Chunk* c : list_) c->dirty = true;
}

void World::setGroundHeight(int x, int z, int h) {
    if (!columnInBounds(x, z)) return;
    Chunk* c = chunkAt(x >> 4, z >> 4);
    if (c) c->ground[((z & 15) << 4) | (x & 15)] = static_cast<int16_t>(h);
}

int World::groundHeight(int x, int z) const {
    if (!columnInBounds(x, z)) return 0;
    const Chunk* c = chunkAt(x >> 4, z >> 4);
    return (c && c->generated) ? c->ground[((z & 15) << 4) | (x & 15)] : 0;
}

int World::standHeight(int x, int z) const {
    if (!columnInBounds(x, z)) return height_;
    int y = std::max(0, groundHeight(x, z));
    while (y < height_ && solid(x, y, z)) ++y;
    return y;
}

int World::topBlockY(int x, int z) const {
    if (!columnInBounds(x, z)) return -1;
    const Chunk* c = chunkAt(x >> 4, z >> 4);
    if (!c || !c->generated) return -1;
    for (int y = std::min(height_, c->maxY) - 1; y >= 0; --y) {
        uint8_t b = c->blocks[blockIndex(x & 15, y, z & 15)];
        if (b != B_AIR && blockInfo(b).shape != Shape::Cross) return y;
    }
    return -1;
}

void World::updateMaxY(Chunk& c) {
    c.maxY = 0;
    for (int y = height_ - 1; y >= 0 && c.maxY == 0; --y) {
        const uint8_t* row = &c.blocks[blockIndex(0, y, 0)];
        for (int i = 0; i < kChunkSize * kChunkSize; ++i) {
            if (row[i] != B_AIR) {
                c.maxY = y + 1;
                break;
            }
        }
    }
}

bool World::neighborsReady(int cx, int cz) const {
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            int nx = cx + dx, nz = cz + dz;
            if (nx < 0 || nz < 0 || nx >= chunksX_ || nz >= chunksZ_) continue;  // fora do mundo = ar
            const Chunk* n = chunkAt(nx, nz);
            if (!n || !n->generated) return false;
        }
    }
    return true;
}

int World::rebuildDirty(int maxChunks) {
    int rebuilt = 0;
    for (Chunk* c : list_) {
        if (!c->dirty || !c->generated || !neighborsReady(c->cx, c->cz)) continue;
        buildChunk(*c);
        c->dirty = false;
        c->meshed = true;
        ++c->version;
        if (++rebuilt >= maxChunks) break;
    }
    return rebuilt;
}

StreamStats World::stream(float fx, float fz, int radius, const StreamBudget& b) {
    StreamStats st;
    if (!source_) return st;
    ZZ_TRACE_SCOPE("ZZ stream");
    const double t0 = perf::nowMs();
    const int genRadius = radius + 1;
    if (spiralRadius_ != genRadius) {
        spiral_.clear();
        float lim = (genRadius + 0.5f) * (genRadius + 0.5f);
        for (int dz = -genRadius; dz <= genRadius; ++dz)
            for (int dx = -genRadius; dx <= genRadius; ++dx)
                if (dx * dx + dz * dz <= lim) spiral_.push_back({dx, dz});
        std::sort(spiral_.begin(), spiral_.end(), [](const auto& a, const auto& b) {
            return a.first * a.first + a.second * a.second < b.first * b.first + b.second * b.second;
        });
        spiralRadius_ = genRadius;
    }
    const int pcx = static_cast<int>(std::floor(fx)) >> 4, pcz = static_cast<int>(std::floor(fz)) >> 4;
    const float meshLim = (radius + 0.5f) * (radius + 0.5f);
    // Faixa urgente em distância de Chebyshev: geração até urgentRadius+1 (5x5), malha até urgentRadius (3x3).
    // -1 desliga (nenhum 'cheb' >= 0 é <= -1), que é o caso do modo legado/bulk.
    const int urgentMeshCheb = b.urgentRadius >= 0 ? b.urgentRadius : -1;
    const int urgentGenCheb = b.urgentRadius >= 0 ? b.urgentRadius + 1 : -1;

    // Pode executar mais uma unidade? 'done' = unidades já feitas nesta chamada deste tipo.
    auto allowed = [&](bool urgent, int done, int floorUnits, int capUnits, float predictedMs) {
        if (!b.timed) return done < capUnits;                        // modo legado: só contagem
        const float spent = static_cast<float>(perf::nowMs() - t0);
        if (urgent) return spent < b.urgentMaxMs;                    // urgente: ignora contagem e maxMs
        if (done >= capUnits) return false;
        if (done < floorUnits) return true;                          // piso: sempre progride
        return spent + predictedMs <= b.maxMs;                       // cabe no orçamento?
    };

    for (const auto& off : spiral_) {
        const int cx = pcx + off.first, cz = pcz + off.second;
        if (cx < 0 || cz < 0 || cx >= chunksX_ || cz >= chunksZ_) continue;
        const float d2 = static_cast<float>(off.first * off.first + off.second * off.second);
        const int cheb = std::max(std::abs(off.first), std::abs(off.second));
        const bool inMesh = d2 <= meshLim;
        Chunk* c = chunkAt(cx, cz);
        if (!c) c = createChunk(cx, cz);
        if (!c->generated) {
            if (!allowed(cheb <= urgentGenCheb, st.generated, b.minGenerate, b.maxGenerate, genEmaMs_)) {
                st.timeCapped = true;
                if (inMesh) {
                    ++st.pending;
                    if (cheb <= urgentMeshCheb) ++st.pendingUrgent;
                }
                continue;
            }
            const double g0 = perf::nowMs();
            c->blocks.assign(static_cast<size_t>(kChunkSize) * kChunkSize * height_, B_AIR);
            std::fill(std::begin(c->ground), std::end(c->ground), static_cast<int16_t>(0));
            c->lights.clear();
            c->campfires.clear();
            {
                ZZ_TRACE_SCOPE("ZZ genChunk");
                source_->generate(*c, height_);
                restoreEdits(*c);
            }
            updateMaxY(*c);
            c->generated = true;
            c->dirty = true;
            const float ms = static_cast<float>(perf::nowMs() - g0);
            genEmaMs_ += (ms - genEmaMs_) * 0.2f;
            st.genMs += ms;
            ++st.generated;
            if (trace_) trace_->genMs.push_back(ms);
            if (hashContent_) contentHash_ += hashChunk(*c);
        }
        if (inMesh && c->dirty) {
            const bool ready = neighborsReady(cx, cz);
            if (ready && allowed(cheb <= urgentMeshCheb, st.meshed, b.minMesh, b.maxMesh, meshEmaMs_)) {
                const double m0 = perf::nowMs();
                {
                    ZZ_TRACE_SCOPE("ZZ meshChunk");
                    buildChunk(*c);
                }
                c->dirty = false;
                c->meshed = true;
                ++c->version;
                const float ms = static_cast<float>(perf::nowMs() - m0);
                meshEmaMs_ += (ms - meshEmaMs_) * 0.2f;
                st.meshMs += ms;
                ++st.meshed;
                if (trace_) trace_->meshMs.push_back(ms);
            } else {
                ++st.pending;
                if (cheb <= urgentMeshCheb) ++st.pendingUrgent;
                if (ready) st.timeCapped = true;  // adiado pelo orçamento, não por vizinho faltando
            }
        }
    }
    // Descarta aos poucos os chunks que ficaram para trás.
    const int unloadDist = genRadius + 2;
    int checks = std::min<int>(48, static_cast<int>(list_.size()));
    for (int i = 0; i < checks && !list_.empty(); ++i) {
        if (unloadCursor_ >= list_.size()) unloadCursor_ = 0;
        Chunk* c = list_[unloadCursor_];
        int dx = c->cx - pcx, dz = c->cz - pcz;
        if (dx * dx + dz * dz > unloadDist * unloadDist) {
            destroyChunk(c);
            ++st.unloaded;
        } else {
            ++unloadCursor_;
        }
    }
    st.totalMs = static_cast<float>(perf::nowMs() - t0);
    return st;
}

uint64_t World::residentHash(float fx, float fz, int radius) const {
    const int pcx = static_cast<int>(std::floor(fx)) >> 4, pcz = static_cast<int>(std::floor(fz)) >> 4;
    uint64_t sum = 0;
    for (const Chunk* c : list_) {
        if (!c->generated) continue;
        const int dx = c->cx - pcx, dz = c->cz - pcz;
        if (dx * dx + dz * dz > radius * radius) continue;
        sum += hashChunk(*c);
    }
    return sum;
}

void World::buildChunk(Chunk& chunk) {
    ChunkMesh& mesh = scratch_;  // capacidade reaproveitada; ao final copia com tamanho exato para o chunk
    mesh.opaque.clear();
    mesh.cutout.clear();
    mesh.water.clear();
    const int H = height_;

    // Copia o chunk e uma borda de 1 bloco dos vizinhos para consultas rápidas.
    pad_.assign(static_cast<size_t>(kPad) * kPad * H, B_AIR);
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const Chunk* n = chunkAt(chunk.cx + dx, chunk.cz + dz);
            if (!n || !n->generated) continue;
            int lx0 = dx < 0 ? 15 : 0, lx1 = dx > 0 ? 0 : 15;
            int lz0 = dz < 0 ? 15 : 0, lz1 = dz > 0 ? 0 : 15;
            int ymax = std::min(H, n->maxY);
            for (int y = 0; y < ymax; ++y)
                for (int lz = lz0; lz <= lz1; ++lz)
                    for (int lx = lx0; lx <= lx1; ++lx) {
                        int px = lx + 1 + dx * kChunkSize, pz = lz + 1 + dz * kChunkSize;
                        pad_[(static_cast<size_t>(y) * kPad + pz) * kPad + px] = n->blocks[blockIndex(lx, y, lz)];
                    }
        }
    }
    auto at = [&](int px, int y, int pz) -> uint8_t {
        if (y < 0) return B_BEDROCK;
        if (y >= H) return B_AIR;
        return pad_[(static_cast<size_t>(y) * kPad + pz) * kPad + px];
    };
    auto occludes = [&](int px, int y, int pz) { return isOpaque(at(px, y, pz)) ? 1 : 0; };

    const int ymax = std::min(H, chunk.maxY);
    for (int y = 0; y < ymax; ++y) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const int px = lx + 1, pz = lz + 1;
                uint8_t b = at(px, y, pz);
                if (b == B_AIR) continue;
                const BlockInfo& info = blockInfo(b);

                if (info.shape == Shape::Cross) {
                    // Duas faces diagonais, frente e verso (plantas).
                    const int diag[2][4][3] = {
                        {{0, 0, 0}, {1, 0, 1}, {1, 1, 1}, {0, 1, 0}},
                        {{1, 0, 0}, {0, 0, 1}, {0, 1, 1}, {1, 1, 0}},
                    };
                    for (const auto& d : diag) {
                        for (int side = 0; side < 2; ++side) {
                            for (int k = 0; k < 4; ++k) {
                                int i = side ? 3 - k : k;
                                VoxelVertex v{};
                                v.x = static_cast<int16_t>((lx + d[i][0]) * kPosScale);
                                v.y = static_cast<int16_t>((y + d[i][1]) * kPosScale);
                                v.z = static_cast<int16_t>((lz + d[i][2]) * kPosScale);
                                v.u = (i == 1 || i == 2) ? 1 : 0;
                                v.v = (i == 0 || i == 1) ? 1 : 0;
                                v.layer = info.side;
                                v.light = 230;
                                v.flags = static_cast<uint8_t>(VF_PLANT | (d[i][1] ? VF_SWAY : 0));
                                mesh.cutout.push_back(v);
                            }
                        }
                    }
                    continue;
                }

                const bool water = info.shape == Shape::Liquid;
                const float waterTop = water ? waterHeight(b,isWaterBlock(at(px,y+1,pz))) : 1.0f;
                if (info.shape != Shape::Cube && !water) continue;
                const bool leaf = isLeaf(b);
                const bool glass = b == B_GLASS;

                for (int f = 0; f < 6; ++f) {
                    const FaceDef& face = kFaces[f];
                    uint8_t nb = at(px + face.nx, y + face.ny, pz + face.nz);
                    float waterBottom = 0;
                    if (water) {
                        if (isOpaque(nb)) continue;
                        if (isWaterBlock(nb)) {
                            if (face.ny != 0) continue;
                            waterBottom=waterHeight(nb,isWaterBlock(at(px+face.nx,y+1,pz+face.nz)));
                            if (waterBottom>=waterTop) continue;
                        }
                    } else {
                        if (isOpaque(nb)) continue;
                        if (glass && nb == B_GLASS) continue;
                        if (y + face.ny < 0) continue;
                    }
                    uint8_t layer = face.ny > 0 ? info.top : (face.ny < 0 ? info.bottom : info.side);
                    int ao[4];
                    for (int k = 0; k < 4; ++k) {
                        const int* c = face.corners[k];
                        if (info.emissive || water) { ao[k] = 3; continue; }
                        int qx = px + face.nx, qy = y + face.ny, qz = pz + face.nz;
                        int t1[3] = {0, 0, 0}, t2[3] = {0, 0, 0};
                        if (face.nx != 0) { t1[1] = c[1] ? 1 : -1; t2[2] = c[2] ? 1 : -1; }
                        else if (face.ny != 0) { t1[0] = c[0] ? 1 : -1; t2[2] = c[2] ? 1 : -1; }
                        else { t1[0] = c[0] ? 1 : -1; t2[1] = c[1] ? 1 : -1; }
                        int s1 = occludes(qx + t1[0], qy + t1[1], qz + t1[2]);
                        int s2 = occludes(qx + t2[0], qy + t2[1], qz + t2[2]);
                        int cc = occludes(qx + t1[0] + t2[0], qy + t1[1] + t2[1], qz + t1[2] + t2[2]);
                        ao[k] = (s1 && s2) ? 0 : 3 - (s1 + s2 + cc);
                    }
                    // Gira o quad quando a AO é anisotrópica para evitar costura diagonal.
                    int start = (ao[0] + ao[2] < ao[1] + ao[3]) ? 1 : 0;
                    std::vector<VoxelVertex>& target = water ? mesh.water : (glass ? mesh.cutout : mesh.opaque);
                    uint8_t flags = static_cast<uint8_t>((water ? VF_WATER : 0) | (info.emissive ? VF_EMISSIVE : 0) | (leaf ? VF_LEAF : 0));
                    if (b == B_GRASS && face.ny > 0) flags |= VF_GRASS;
                    for (int n = 0; n < 4; ++n) {
                        int k = (start + n) % 4;
                        const int* c = face.corners[k];
                        VoxelVertex v{};
                        float vy = static_cast<float>(y + c[1]);
                        if (water) vy = float(y) + (c[1] ? waterTop : waterBottom);
                        v.x = static_cast<int16_t>((lx + c[0]) * kPosScale);
                        v.y = static_cast<int16_t>(std::lround(vy * kPosScale));
                        v.z = static_cast<int16_t>((lz + c[2]) * kPosScale);
                        v.u = kUv[k][0];
                        v.v = kUv[k][1];
                        v.layer = layer;
                        float light = info.emissive ? 1.0f : face.shade * kAoCurve[ao[k]];
                        v.light = static_cast<uint8_t>(clampf(light * 255.0f, 0, 255));
                        v.flags = flags;
                        target.push_back(v);
                    }
                }
            }
        }
    }
    chunk.mesh.opaque.assign(mesh.opaque.begin(), mesh.opaque.end());
    chunk.mesh.cutout.assign(mesh.cutout.begin(), mesh.cutout.end());
    chunk.mesh.water.assign(mesh.water.begin(), mesh.water.end());
}

bool World::raycast(const Vec3& origin, const Vec3& direction, float maxDistance, RayHit& hit) const {
    Vec3 dir = normalize(direction);
    if (length(dir) < 0.5f) return false;
    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));
    const int stepX = dir.x > 0 ? 1 : -1, stepY = dir.y > 0 ? 1 : -1, stepZ = dir.z > 0 ? 1 : -1;
    const float inf = 1e30f;
    float tDeltaX = std::fabs(dir.x) > 1e-6f ? std::fabs(1.0f / dir.x) : inf;
    float tDeltaY = std::fabs(dir.y) > 1e-6f ? std::fabs(1.0f / dir.y) : inf;
    float tDeltaZ = std::fabs(dir.z) > 1e-6f ? std::fabs(1.0f / dir.z) : inf;
    float tMaxX = std::fabs(dir.x) > 1e-6f ? ((stepX > 0 ? (x + 1 - origin.x) : (origin.x - x)) * tDeltaX) : inf;
    float tMaxY = std::fabs(dir.y) > 1e-6f ? ((stepY > 0 ? (y + 1 - origin.y) : (origin.y - y)) * tDeltaY) : inf;
    float tMaxZ = std::fabs(dir.z) > 1e-6f ? ((stepZ > 0 ? (z + 1 - origin.z) : (origin.z - z)) * tDeltaZ) : inf;
    int nx = 0, ny = 0, nz = 0;
    float t = 0;
    for (int i = 0; i < 1024 && t <= maxDistance; ++i) {
        uint8_t b = get(x, y, z);
        if (isSolid(b)) {
            hit = {x, y, z, nx, ny, nz, t, b};
            return true;
        }
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX; t = tMaxX; tMaxX += tDeltaX; nx = -stepX; ny = 0; nz = 0;
        } else if (tMaxY < tMaxZ) {
            y += stepY; t = tMaxY; tMaxY += tDeltaY; nx = 0; ny = -stepY; nz = 0;
        } else {
            z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ; nx = 0; ny = 0; nz = -stepZ;
        }
        if (y >= height_ && stepY > 0) break;
    }
    return false;
}

bool World::lineOfSight(const Vec3& from, const Vec3& to) const {
    Vec3 d = to - from;
    float dist = length(d);
    if (dist < 1e-3f) return true;
    RayHit hit;
    return !raycast(from, d, dist - 0.05f, hit);
}

bool World::boxCollides(const Vec3& mn, const Vec3& mx) const {
    int x0 = static_cast<int>(std::floor(mn.x)), x1 = static_cast<int>(std::floor(mx.x));
    int y0 = static_cast<int>(std::floor(mn.y)), y1 = static_cast<int>(std::floor(mx.y));
    int z0 = static_cast<int>(std::floor(mn.z)), z1 = static_cast<int>(std::floor(mx.z));
    for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x)
                if (solid(x, y, z)) return true;
    // Bordas do mundo funcionam como barreira invisível.
    return mn.x < 1.0f || mn.z < 1.0f || mx.x > width_ - 1.0f || mx.z > depth_ - 1.0f;
}

namespace {

bool overlaps(const World& world, const BodyState& b, const Vec3& p) {
    Vec3 mn(p.x - b.halfWidth, p.y, p.z - b.halfWidth);
    Vec3 mx(p.x + b.halfWidth, p.y + b.height, p.z + b.halfWidth);
    const float e = 1e-4f;
    return world.boxCollides(mn + Vec3(e, e, e), mx - Vec3(e, e, e));
}

// Move num eixo e resolve contra blocos; retorna true se colidiu.
bool moveAxis(const World& world, BodyState& b, int axis, float amount) {
    if (amount == 0) return false;
    Vec3 p = b.position;
    p[axis] += amount;
    if (!overlaps(world, b, p)) {
        b.position = p;
        return false;
    }
    // Busca binária pela maior fração livre.
    float lo = 0, hi = 1;
    for (int i = 0; i < 10; ++i) {
        float mid = (lo + hi) * 0.5f;
        Vec3 q = b.position;
        q[axis] += amount * mid;
        if (overlaps(world, b, q)) hi = mid; else lo = mid;
    }
    b.position[axis] += amount * lo;
    return true;
}

}  // namespace

void moveBody(const World& world, BodyState& body, const Vec3& displacement, float stepHeight) {
    body.hitWall = false;
    int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::fabs(displacement.x), std::max(std::fabs(displacement.y), std::fabs(displacement.z))) / 0.35f)));
    Vec3 d = displacement / static_cast<float>(steps);
    bool grounded = false;
    for (int s = 0; s < steps; ++s) {
        if (moveAxis(world, body, 1, d.y)) {
            if (d.y < 0) grounded = true;
            body.velocity.y = 0;
        }
        for (int axis = 0; axis <= 2; axis += 2) {
            float amount = d[axis];
            if (amount == 0) continue;
            Vec3 before = body.position;
            if (!moveAxis(world, body, axis, amount)) continue;
            // Tenta subir um degrau de até stepHeight.
            bool stepped = false;
            if ((body.onGround || grounded) && stepHeight > 0) {
                Vec3 raised = before;
                raised.y += stepHeight;
                if (!overlaps(world, body, raised)) {
                    Vec3 target = raised;
                    target[axis] += amount;
                    if (!overlaps(world, body, target)) {
                        // Desce até apoiar no degrau.
                        BodyState probe = body;
                        probe.position = target;
                        moveAxis(world, probe, 1, -stepHeight);
                        body.stepOffset += probe.position.y - before.y;
                        body.position = probe.position;
                        stepped = true;
                    }
                }
            }
            if (!stepped) body.hitWall = true;
        }
    }
    body.onGround = grounded;
    if (!grounded && body.velocity.y <= 0) {
        // Checagem de contato com o chão (para pulo/animação).
        Vec3 probe = body.position;
        probe.y -= 0.05f;
        body.onGround = overlaps(world, body, probe);
    }
    Vec3 mid = body.position;
    mid.y += 0.6f;
    body.inWater = world.isWaterAt(mid) || world.isWaterAt(body.position + Vec3(0, 0.15f, 0));
}

}  // namespace zz
