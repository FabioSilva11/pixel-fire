// Mundo voxel em chunks de 16x16: mundo finito (lobby) ou mundo em streaming (ilhas das fases),
// malha por chunk (opaca, recorte e água), raycast e colisão AABB.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

#include "../core/math.h"
#include "../core/perf.h"
#include "blocks.h"

namespace zz {

constexpr int kChunkSize = 16;
constexpr float kPosScale = 8.0f;  // vértices em 1/8 de bloco; x/z locais ao chunk

enum VertexFlags : uint8_t {
    VF_WATER = 1,
    VF_EMISSIVE = 2,
    VF_SWAY = 4,    // topo de plantas em cruz (balança com o vento)
    VF_LEAF = 8,    // folhas das árvores (tremulam com o vento)
    VF_GRASS = 16,  // topo de grama (ondas de vento na cor)
    VF_PLANT = 32,  // qualquer vértice de planta em cruz (normal difusa na iluminação)
};

#pragma pack(push, 1)
struct VoxelVertex {
    int16_t x, y, z;
    uint8_t u, v;
    uint8_t layer, light, flags, pad;
};
#pragma pack(pop)
static_assert(sizeof(VoxelVertex) == 12, "VoxelVertex deve ter 12 bytes");

struct ChunkMesh {
    std::vector<VoxelVertex> opaque;
    std::vector<VoxelVertex> cutout;
    std::vector<VoxelVertex> water;
};

// Recursos de GPU do chunk (preenchidos e lidos apenas pelo renderer, na thread GL).
struct ChunkGpu {
    uint32_t vao[3] = {0, 0, 0};
    uint32_t vbo[3] = {0, 0, 0};
    int quads[3] = {0, 0, 0};
    uint32_t version = 0;
    int contextId = -1;
    uint32_t bytes = 0;  // tamanho dos VBOs enviados (diagnóstico)
};

struct Chunk {
    int cx = 0, cz = 0;
    bool generated = false;  // blocos preenchidos
    bool dirty = true;       // precisa remalhar
    bool meshed = false;     // já tem malha válida
    uint32_t version = 0;    // incrementa a cada remalha; o renderer reenvia ao GPU
    int maxY = 0;            // maior y ocupado + 1 (acelera a malha)
    int listIndex = -1;
    std::vector<uint8_t> blocks;   // kChunkSize * kChunkSize * altura
    int16_t ground[kChunkSize * kChunkSize] = {};
    ChunkMesh mesh;
    AABB bounds;
    std::vector<Vec3> lights;     // lanternas/fogueiras geradas com o chunk
    std::vector<Vec3> campfires;  // partículas de fogo
    ChunkGpu gpu;
};

class World;

// Gera o conteúdo de um chunk sob demanda (mundo em streaming).
class ChunkSource {
public:
    virtual ~ChunkSource() = default;
    virtual void generate(Chunk& chunk, int height) = 0;
};

// Contadores do último World::stream (diagnóstico e entrada do orçamento adaptativo).
struct StreamStats {
    int pending = 0;          // chunks do raio de malha ainda sem geração/malha
    int pendingUrgent = 0;    // idem, dentro da faixa urgente (deve ser 0 em regime)
    int generated = 0, meshed = 0, unloaded = 0;
    float genMs = 0, meshMs = 0, totalMs = 0;
    bool timeCapped = false;  // o orçamento de tempo adiou trabalho nesta chamada
};

// Orçamento de uma chamada a World::stream.
struct StreamBudget {
    float maxMs = 3.0f;          // tempo (geração + malha) permitido por chamada, fora urgente/piso
    int minGenerate = 1;         // piso por chamada, se houver pendência
    int minMesh = 1;
    int maxGenerate = 4;         // teto absoluto por chamada
    int maxMesh = 6;
    int urgentRadius = 1;        // chunks; < 0 desliga a faixa urgente
    float urgentMaxMs = 12.0f;   // teto de segurança do trabalho urgente numa chamada
    bool timed = true;           // false = só contagem (comportamento antigo)

    static StreamBudget legacy(int gen, int mesh) {
        StreamBudget b;
        b.timed = false;
        b.minGenerate = b.minMesh = 0;
        b.maxGenerate = gen;
        b.maxMesh = mesh;
        b.urgentRadius = -1;
        return b;
    }
    static StreamBudget bulk() { return legacy(48, 48); }  // carga inicial
};

// Registro opcional de amostras por chunk (diagnóstico).
struct StreamTrace {
    std::vector<float> genMs, meshMs;
};

struct RayHit {
    int x = 0, y = 0, z = 0;
    int nx = 0, ny = 0, nz = 0;
    float distance = 0;
    uint8_t block = B_AIR;
};

class World {
public:
    // Mundo finito [0,w) x [0,d): todos os chunks existem desde o início (cheios de ar).
    void reset(int width, int height, int depth);
    // Mundo em streaming: chunks gerados por `source` ao redor do jogador e descartados ao longe.
    void resetStreaming(int width, int height, int depth, ChunkSource* source);

    int width() const { return width_; }
    int height() const { return height_; }
    int depth() const { return depth_; }
    bool streaming() const { return source_ != nullptr; }
    bool inBounds(int x, int y, int z) const {
        return x >= 0 && z >= 0 && y >= 0 && x < width_ && z < depth_ && y < height_;
    }
    bool columnInBounds(int x, int z) const { return x >= 0 && z >= 0 && x < width_ && z < depth_; }

    uint8_t get(int x, int y, int z) const {
        if (y < 0) return B_BEDROCK;
        if (y >= height_ || !columnInBounds(x, z)) return B_AIR;
        const Chunk* c = chunkAt(x >> 4, z >> 4);
        if (!c || !c->generated) return B_AIR;
        return c->blocks[blockIndex(x & 15, y, z & 15)];
    }
    void set(int x, int y, int z, uint8_t block, bool markDirty = true);
    bool solid(int x, int y, int z) const { return isSolid(get(x, y, z)); }

    // Altura em que um ator fica de pé na coluna (primeiro ar acima do chão gerado).
    int standHeight(int x, int z) const;
    void setGroundHeight(int x, int z, int h);
    int groundHeight(int x, int z) const;
    // Topo visível (inclui árvores) — usado no minimapa.
    int topBlockY(int x, int z) const;
    // A coluna já foi gerada (o terreno existe ali)?
    bool columnReady(int x, int z) const {
        if (!columnInBounds(x, z)) return false;
        const Chunk* c = chunkAt(x >> 4, z >> 4);
        return c && c->generated;
    }
    // A coluna tem malha pronta (o jogador consegue vê-la)?
    bool columnMeshed(int x, int z) const {
        if (!columnInBounds(x, z)) return false;
        const Chunk* c = chunkAt(x >> 4, z >> 4);
        return c && c->meshed;
    }

    bool raycast(const Vec3& origin, const Vec3& direction, float maxDistance, RayHit& hit) const;
    bool lineOfSight(const Vec3& from, const Vec3& to) const;
    bool boxCollides(const Vec3& minCorner, const Vec3& maxCorner) const;
    bool isWaterAt(const Vec3& p) const {
        int x=int(std::floor(p.x)), y=int(std::floor(p.y)), z=int(std::floor(p.z));
        uint8_t b=get(x,y,z);
        return isWaterBlock(b) && p.y-y < waterHeight(b,isWaterBlock(get(x,y+1,z)));
    }

    const std::vector<Chunk*>& chunks() const { return list_; }
    Chunk* chunkAt(int cx, int cz) {
        if (cx < 0 || cz < 0 || cx >= chunksX_ || cz >= chunksZ_) return nullptr;
        return grid_[static_cast<size_t>(cz) * chunksX_ + cx].get();
    }
    const Chunk* chunkAt(int cx, int cz) const {
        if (cx < 0 || cz < 0 || cx >= chunksX_ || cz >= chunksZ_) return nullptr;
        return grid_[static_cast<size_t>(cz) * chunksX_ + cx].get();
    }
    int chunksX() const { return chunksX_; }
    int chunksZ() const { return chunksZ_; }
    uint32_t generation() const { return generation_; }
    uint32_t revision() const { return revision_; }
    void markAllDirty();
    // Remalha até maxChunks chunks sujos (com vizinhos prontos); retorna quantos foram reconstruídos.
    int rebuildDirty(int maxChunks);
    // Streaming: gera/malha os chunks mais próximos de (x,z) e descarta os distantes.
    // Retorna estatísticas da chamada (inclusive quantos chunks dentro do raio ainda faltam ficar prontos).
    StreamStats stream(float x, float z, int radius, const StreamBudget& budget);
    // Compatibilidade: delega para o modo legado (só contagem) e devolve apenas 'pending'.
    int stream(float x, float z, int radius, int maxGenerate, int maxMesh) {
        return stream(x, z, radius, StreamBudget::legacy(maxGenerate, maxMesh)).pending;
    }
    // Diagnóstico
    void setTrace(StreamTrace* t) { trace_ = t; }
    void enableContentHash(bool on) {
        hashContent_ = on;
        contentHash_ = 0;
    }
    // Soma ordem-independente do hash do conteúdo dos chunks gerados dentro do raio (em chunks) de (x,z).
    uint64_t residentHash(float x, float z, int radius) const;
    // Recursos de GPU de chunks descartados (o renderer apaga).
    void drainGpuGarbage(std::vector<ChunkGpu>& out) {
        out.insert(out.end(), gpuGarbage_.begin(), gpuGarbage_.end());
        gpuGarbage_.clear();
    }

private:
    static size_t blockIndex(int lx, int y, int lz) {
        return (static_cast<size_t>(y) << 8) | (static_cast<size_t>(lz) << 4) | static_cast<size_t>(lx);
    }
    void clear();
    Chunk* createChunk(int cx, int cz);
    void destroyChunk(Chunk* chunk);
    bool neighborsReady(int cx, int cz) const;
    void markDirtyAt(int x, int z);
    void buildChunk(Chunk& chunk);
    void updateMaxY(Chunk& chunk);
    void restoreEdits(Chunk& chunk);
    void lowerGround(Chunk& chunk, int lx, int lz);
    void refreshLights(Chunk& chunk);

    int width_ = 0, height_ = 0, depth_ = 0;
    int chunksX_ = 0, chunksZ_ = 0;
    uint32_t generation_ = 0;
    uint32_t revision_ = 0;
    std::unordered_map<uint32_t, std::unordered_map<uint16_t, uint8_t>> edits_;
    ChunkSource* source_ = nullptr;
    std::vector<std::unique_ptr<Chunk>> grid_;
    std::vector<Chunk*> list_;
    std::vector<ChunkGpu> gpuGarbage_;
    std::vector<std::pair<int, int>> spiral_;
    int spiralRadius_ = -1;
    size_t unloadCursor_ = 0;
    std::vector<uint8_t> pad_;  // cópia 18x18xH do chunk + vizinhos para a malha
    ChunkMesh scratch_;         // malha em construção (capacidade reaproveitada entre chunks)
    float genEmaMs_ = 2.0f, meshEmaMs_ = 1.2f;  // previsão de custo por unidade (orçamento de tempo)
    StreamTrace* trace_ = nullptr;
    bool hashContent_ = false;
    uint64_t contentHash_ = 0;
};

// Movimento de uma caixa (pés no centro da base) contra blocos sólidos, com subida automática de degrau.
struct BodyState {
    Vec3 position;       // centro dos pés
    Vec3 velocity;
    float halfWidth = 0.3f;
    float height = 1.8f;
    bool onGround = false;
    bool inWater = false;
    bool hitWall = false;
    float stepOffset = 0;  // degrau recém-subido (para suavizar câmera/modelo)
};

void moveBody(const World& world, BodyState& body, const Vec3& displacement, float stepHeight);

}  // namespace zz
