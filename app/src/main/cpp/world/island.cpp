#include "island.h"

#include <algorithm>
#include <cmath>

#include "../core/random.h"
#include "position_key.h"

namespace zz {

namespace {

bool validPos(int x, int y, int z) {
    return x >= 0 && z >= 0 && y >= 0 && x < kIslandSize && z < kIslandSize && y < kIslandHeight;
}

// Grava as estruturas de um ponto de interesse como um molde reaplicável em qualquer chunk.
class StampSink : public BlockSink {
public:
    explicit StampSink(PoiStamp& s) : s_(s) {}
    uint8_t get(int x, int y, int z) const override {
        if (!validPos(x, y, z)) return B_AIR;
        auto it = s_.edits.find(packPosition(x, y, z));
        return it == s_.edits.end() ? B_AIR : it->second;
    }
    void set(int x, int y, int z, uint8_t block) override {
        if (!validPos(x, y, z)) return;
        s_.edits[packPosition(x, y, z)] = block;
        if (s_.maxX < s_.minX) {
            s_.minX = s_.maxX = x;
            s_.minZ = s_.maxZ = z;
        } else {
            s_.minX = std::min(s_.minX, x);
            s_.maxX = std::max(s_.maxX, x);
            s_.minZ = std::min(s_.minZ, z);
            s_.maxZ = std::max(s_.maxZ, z);
        }
    }

private:
    PoiStamp& s_;
};

// Escreve somente dentro do chunk em geração (árvores vizinhas são recortadas na borda).
class ChunkSink : public BlockSink {
public:
    ChunkSink(Chunk& c, int height) : c_(c), h_(height), x0_(c.cx * kChunkSize), z0_(c.cz * kChunkSize) {}
    uint8_t get(int x, int y, int z) const override {
        if (!inside(x, y, z)) return B_AIR;
        return c_.blocks[index(x, y, z)];
    }
    void set(int x, int y, int z, uint8_t block) override {
        if (!inside(x, y, z)) return;
        c_.blocks[index(x, y, z)] = block;
    }
    bool inside(int x, int y, int z) const {
        return x >= x0_ && z >= z0_ && x < x0_ + kChunkSize && z < z0_ + kChunkSize && y >= 0 && y < h_;
    }

private:
    size_t index(int x, int y, int z) const {
        return (static_cast<size_t>(y) << 8) | (static_cast<size_t>(z - z0_) << 4) | static_cast<size_t>(x - x0_);
    }
    Chunk& c_;
    int h_, x0_, z0_;
};

uint32_t mixColor(uint32_t a, uint32_t b, float t) {
    t = clampf(t, 0, 1);
    auto ch = [&](int s) {
        float va = static_cast<float>((a >> s) & 0xFF), vb = static_cast<float>((b >> s) & 0xFF);
        return static_cast<uint32_t>(clampf(lerpf(va, vb, t), 0, 255)) << s;
    };
    return ch(16) | ch(8) | ch(0);
}

uint32_t shadeColor(uint32_t c, float f) {
    auto ch = [&](int s) { return static_cast<uint32_t>(clampf(((c >> s) & 0xFF) * f, 0, 255)) << s; };
    return ch(16) | ch(8) | ch(0);
}

}  // namespace

IslandGenerator::IslandGenerator(const Theme& theme, int stage, uint32_t seed)
    : primary_(theme), terrain_(seed), stage_(std::max(1, stage)), seed_(seed) {
    // Bioma secundário: outra zona, escolhida pela semente.
    int other = (theme.id + 1 + static_cast<int>(seed_ % 3u)) % 4;
    secondary_ = themeForStage(other * 5 + 1);
    cache_.resize(static_cast<size_t>(kIslandCells) * kIslandCells);
    cached_.assign(static_cast<size_t>(kIslandCells) * kIslandCells, 0);
    chooseLayout();
}

void IslandGenerator::reservePoi(const Vec3& center,int kind,float radius) {
    const int cx=int(center.x)/kIslandCell,cz=int(center.z)/kIslandCell;
    if(cx<0||cz<0||cx>=kIslandCells||cz>=kIslandCells)return;
    const int id=cz*kIslandCells+cx;
    IslandPoi p;p.exists=true;p.cellX=cx;p.cellZ=cz;p.id=id;p.kind=kind;
    p.center={std::floor(center.x)+.5f,clampf(std::round(naturalHeight(center.x,center.z)),float(sea_+2),float(kIslandHeight-28)),std::floor(center.z)+.5f};
    p.radius=radius;cache_[id]=p;cached_[id]=1;stamps_.erase(id);roles_.erase(id);
}

void IslandGenerator::suppressProceduralPois() {
    // Competitive landmarks use their own fixed architectural layout, never nearest random POIs.
    roles_.clear();stamps_.clear();
    for(size_t i=0;i<cache_.size();++i) { cache_[i]=IslandPoi{}; cached_[i]=1; }
}

float IslandGenerator::landMask(float x, float z) const {
    return x >= 0 && z >= 0 && x < kIslandSize && z < kIslandSize ? 1.0f : 0.0f;
}

float IslandGenerator::secondaryWeight(float x, float z) const {
    return terrain_.biome(int(std::floor(x)),int(std::floor(z))) * .5f;
}

float IslandGenerator::riverMask(float x, float z) const {
    double d = horizonte::riverDistance(seed_,int(std::floor(x)),int(std::floor(z)));
    return 1.0f - smoothstepf(2.0f,7.0f,float(d));
}

float IslandGenerator::naturalHeight(float x, float z) const {
    return float(terrain_.groundHeight(int(std::floor(x)),int(std::floor(z))) + 1);
}

const Theme& IslandGenerator::biomeTheme(float, int x, int z) const {
    return naturalBiomes_[terrain_.biome(x,z)];
}

IslandPoi IslandGenerator::basePoi(int cx, int cz) const {
    IslandPoi p;
    p.cellX = cx;
    p.cellZ = cz;
    if (cx < 0 || cz < 0 || cx >= kIslandCells || cz >= kIslandCells) return p;
    p.id = cz * kIslandCells + cx;
    uint32_t h = hash2(cx, cz, seed_ ^ 0x9017u);
    float px = cx * kIslandCell + 24.0f + hashFloat(cx, cz, seed_ + 1) * 80.0f;
    float pz = cz * kIslandCell + 24.0f + hashFloat(cx, cz, seed_ + 2) * 80.0f;
    p.center = {std::floor(px) + 0.5f, 0.0f, std::floor(pz) + 0.5f};
    if (landMask(px, pz) < 0.9f || riverMask(px, pz) > 0.0f) return p;
    if (!horizonte::clearFootprint(terrain_, seed_, int(px), int(pz), 38.0f)) return p;
    float nh = naturalHeight(px, pz);
    if (nh < sea_ + 1.6f || nh > sea_ + 24.0f) return p;
    p.center.y = clampf(std::round(nh), sea_ + 2.0f, sea_ + 20.0f);
    p.exists = ((h >> 8) & 0xFFFF) < static_cast<uint32_t>(0.66f * 65536.0f);
    // Quase metade dos locais são construções: casas, vilarejos, igrejas, postos, prédios e celeiros.
    int roll = static_cast<int>((h >> 24) % 100u);
    if (roll < 10) p.kind = K_FIRE;
    else if (roll < 19) p.kind = K_RUINS;
    else if (roll < 26) p.kind = K_GRAVEYARD;
    else if (roll < 34) p.kind = K_DEPOT;
    else if (roll < 40) p.kind = K_FARM;
    else if (roll < 49) p.kind = K_TOWER;
    else if (roll < 56) p.kind = K_OUTPOST;
    else if (roll < 71) p.kind = K_HOUSE;
    else if (roll < 79) p.kind = K_VILLAGE;
    else if (roll < 84) p.kind = K_CHURCH;
    else if (roll < 91) p.kind = K_GAS;
    else if (roll < 96) p.kind = K_APARTMENT;
    else p.kind = K_BARN;
    p.radius = 6.5f + hashFloat(cx, cz, seed_ + 3) * 2.0f;
    if (p.kind == K_TOWER || p.kind == K_OUTPOST) p.radius = std::max(p.radius, 7.5f);
    if (isBuildingKind(p.kind)) p.radius = buildingRadius(p.kind);
    return p;
}

const IslandPoi& IslandGenerator::poi(int cx, int cz) {
    static const IslandPoi kNone;
    if (cx < 0 || cz < 0 || cx >= kIslandCells || cz >= kIslandCells) return kNone;
    size_t id = static_cast<size_t>(cz) * kIslandCells + cx;
    if (cached_[id]) return cache_[id];
    IslandPoi p = basePoi(cx, cz);
    auto role = roles_.find(static_cast<int>(id));
    if (role != roles_.end()) {
        p.exists = true;
        p.role = role->second;
        if (p.role == ROLE_START) { p.kind = K_FIRE; p.radius = 7.0f; }

    }
    cache_[id] = p;
    cached_[id] = 1;
    return cache_[id];
}

void IslandGenerator::chooseLayout() {
    Rng rng(seed_ ^ 0x51A27u);
    auto valid = [&](const IslandPoi& p) { return p.id >= 0 && p.center.y > 0.0f; };
    auto findNear = [&](float x, float z, const std::vector<int>& used) -> int {
        int cx = static_cast<int>(x) / kIslandCell, cz = static_cast<int>(z) / kIslandCell;
        for (int ring = 0; ring < 16; ++ring)
            for (int dz = -ring; dz <= ring; ++dz)
                for (int dx = -ring; dx <= ring; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                    IslandPoi b = basePoi(cx + dx, cz + dz);
                    if (!valid(b) || std::find(used.begin(), used.end(), b.id) != used.end()) continue;
                    return b.id;
                }
        return -1;
    };

    // Acampamento inicial perto do centro da ilha.
    std::vector<int> used;
    float a0 = rng.range(0, kTwoPi), d0 = rng.range(0, 650);
    int startId = findNear(kIslandCenter + std::cos(a0) * d0, kIslandCenter + std::sin(a0) * d0, used);
    if (startId < 0) startId = (kIslandCells / 2) * kIslandCells + kIslandCells / 2;
    used.push_back(startId);
    IslandPoi sp = basePoi(startId % kIslandCells, startId / kIslandCells);
    if (sp.center.y <= 0) sp.center.y = static_cast<float>(sea_ + 4);
    roles_[startId] = ROLE_START;
    Vec2 s(sp.center.x, sp.center.z);

    start_ = sp.center + Vec3(0,0,2.5f);

}

bool IslandGenerator::pathLink(int ax, int az, int bx, int bz) {
    const IslandPoi& a = poi(ax, az);
    const IslandPoi& b = poi(bx, bz);
    if (!a.exists || !b.exists) return false;
    int lo = std::min(a.id, b.id), hi = std::max(a.id, b.id);
    if (hashFloat(lo, hi, seed_ + 33) > 0.72f && a.role == ROLE_NONE && b.role == ROLE_NONE) return false;
    return distXZ(a.center, b.center) < 245.0f;
}

float IslandGenerator::pathDistance(int x, int z, int cx, int cz) {
    const IslandPoi& a = poi(cx, cz);
    if (!a.exists) return 1e9f;
    static const int kNb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    float best = 1e9f;
    for (const auto& d : kNb) {
        if (!pathLink(cx, cz, cx + d[0], cz + d[1])) continue;
        const IslandPoi& b = poi(cx + d[0], cz + d[1]);
        // Orientação canônica: a curva sai idêntica vista das duas células.
        const IslandPoi& p0 = a.id < b.id ? a : b;
        const IslandPoi& p1 = a.id < b.id ? b : a;
        float ax = p0.center.x, az = p0.center.z;
        float dx = p1.center.x - ax, dz = p1.center.z - az;
        float len = std::sqrt(dx * dx + dz * dz);
        if (len < 1.0f) continue;
        float ux = dx / len, uz = dz / len;
        float px = x + 0.5f - ax, pz = z + 0.5f - az;
        float t = clampf((px * ux + pz * uz) / len, 0.0f, 1.0f);
        uint32_t hh = hash2(p0.id, p1.id, seed_ + 34);
        float amp = 4.0f + static_cast<float>(hh & 255u) / 255.0f * 7.0f;
        float waves = 1.0f + std::floor(len / 70.0f);
        float off = std::sin(t * kPi * waves) * amp * std::sin(t * kPi);
        float cxp = ux * t * len - uz * off, czp = uz * t * len + ux * off;
        float ddx = px - cxp, ddz = pz - czp;
        best = std::min(best, std::sqrt(ddx * ddx + ddz * ddz));
    }
    return best;
}

float IslandGenerator::surfaceHeight(int x, int z, const IslandPoi& p) const {
    float h = naturalHeight(x + 0.5f, z + 0.5f);
    if (p.exists) {
        float d = std::sqrt((x + 0.5f - p.center.x) * (x + 0.5f - p.center.x) + (z + 0.5f - p.center.z) * (z + 0.5f - p.center.z));
        if (d < p.radius + 8.0f) h = lerpf(p.center.y, h, smoothstepf(p.radius, p.radius + 8.0f, d));
    }
    return h;
}

ColumnInfo IslandGenerator::column(int x, int z) {
    ColumnInfo c;
    int cx = x / kIslandCell, cz = z / kIslandCell;
    const IslandPoi& p = poi(cx, cz);
    float fx = x + 0.5f, fz = z + 0.5f;
    c.secondary = secondaryWeight(fx, fz);
    c.biome = &biomeTheme(c.secondary, x, z);
    c.river = riverMask(fx, fz);
    float h = surfaceHeight(x, z, p);
    float poiDist = 1e9f;
    if (p.exists) {
        poiDist = std::sqrt((fx - p.center.x) * (fx - p.center.x) + (fz - p.center.z) * (fz - p.center.z));
        c.nearPoi = poiDist < p.radius + 3.5f;
    }
    float pd = pathDistance(x, z, cx, cz);
    c.nearPath = pd < 3.5f;
    c.top = std::max(2, std::min(kIslandHeight - 14, static_cast<int>(std::lround(h))));
    c.water = c.top <= sea_;
    if (pd < 1.45f && !(p.exists && poiDist < p.radius - 0.5f)) {
        c.path = true;
        c.bridge = c.water;
    }
    c.forest = c.biome->id == 1 ? 0.0f : 0.35f;
    return c;
}

namespace {

uint8_t surfaceBlock(const ColumnInfo& c, int, int, int sea, uint32_t) {
    if (c.path && !c.water) return B_PATH;
    return c.top - 1 <= sea + 1 || c.biome->id == 1 ? B_SAND :
           c.biome->id == 2 ? B_SNOW : B_GRASS;
}

}  // namespace

void IslandGenerator::generate(Chunk& chunk, int height) {
    const int x0 = chunk.cx * kChunkSize, z0 = chunk.cz * kChunkSize;
    horizonte::generate(terrain_,chunk,height);
    ChunkSink sink(chunk,height);
    auto idx = [](int x, int y, int z) { return (size_t(y)*16+z)*16+x; };
    // O núcleo mantém cavernas e aldeias. As sobreposições locais
    // só alteram o chão e a faixa de passagem das trilhas e dos POIs.
    for (int lz=0; lz<16; ++lz)
        for (int lx=0; lx<16; ++lx) {
            int x=x0+lx, z=z0+lz;
            ColumnInfo c=column(x,z);
            int natural=terrain_.groundHeight(x,z)+1;
            if (!c.nearPoi && !c.nearPath && c.top == natural) continue;
            // Caminhos preservam as casas existentes.
            bool village = !terrain_.villagesNear(x,z,0).empty();
            if (village && !c.nearPoi) continue;
            int low=std::max(1,std::min(natural,c.top)-4);
            int high=std::min(height,std::max(natural,c.top)+8);
            for (int y=low; y<high; ++y) {
                uint8_t block = y < c.top-1 ? (y < c.top-4 ? B_STONE : c.biome->filler) :
                                y == c.top-1 ? surfaceBlock(c,x,z,sea_,seed_) :
                                y <= sea_ ? B_WATER : B_AIR;
                chunk.blocks[idx(lx,y,lz)]=block;
            }
            int ground=c.top;
            if (c.bridge) { sink.set(x,sea_,z,B_PLANKS); ground=sea_+1; }
            chunk.ground[lz*16+lx]=int16_t(ground);
        }

    // Moldes de células vizinhas também podem atravessar a borda do chunk.
    int pcx=x0/kIslandCell, pcz=z0/kIslandCell;
    for (int cz=pcz-1; cz<=pcz+1; ++cz)
        for (int cx=pcx-1; cx<=pcx+1; ++cx) {
            const IslandPoi& p=poi(cx,cz);
            if (!p.exists) continue;
            float reach=p.radius+12;
            if (p.center.x+reach<x0 || p.center.x-reach>=x0+16 ||
                p.center.z+reach<z0 || p.center.z-reach>=z0+16) continue;
            const PoiStamp& s=stamp(p);
            for (const auto& e:s.edits)
                sink.set(positionX(e.first),positionY(e.first),positionZ(e.first),e.second);
            auto inChunk=[&](const Vec3& v) { return v.x>=x0 && v.z>=z0 && v.x<x0+16 && v.z<z0+16; };
            for (const Vec3& l:s.decor.lights) if (inChunk(l)) chunk.lights.push_back(l);
            for (const Vec3& f:s.decor.campfires) if (inChunk(f)) chunk.campfires.push_back(f);
        }
}

const PoiStamp& IslandGenerator::stamp(const IslandPoi& p) {
    auto it = stamps_.find(p.id);
    if (it != stamps_.end()) return it->second;
    PoiStamp& s = stamps_[p.id];
    StampSink sink(s);
    Rng rng(hash2(p.cellX, p.cellZ, seed_ + 811));
    Camp camp{p.center, p.radius, p.role == ROLE_START, p.kind};
    const Theme& t = biomeTheme(secondaryWeight(p.center.x, p.center.z), static_cast<int>(p.center.x), static_cast<int>(p.center.z));
    decorateCamp(sink,camp,t,rng,s.decor);
    return s;
}

void IslandGenerator::poisNear(const Vec3& pos, float radius, std::vector<const IslandPoi*>& out) {
    out.clear();
    int c0x = static_cast<int>((pos.x - radius) / kIslandCell), c1x = static_cast<int>((pos.x + radius) / kIslandCell);
    int c0z = static_cast<int>((pos.z - radius) / kIslandCell), c1z = static_cast<int>((pos.z + radius) / kIslandCell);
    for (int cz = c0z; cz <= c1z; ++cz)
        for (int cx = c0x; cx <= c1x; ++cx) {
            const IslandPoi& p = poi(cx, cz);
            if (p.exists && distXZ(p.center, pos) <= radius) out.push_back(&p);
        }
}

uint32_t IslandGenerator::mapColor(int x, int z) {
    ColumnInfo c = column(x, z);
    const int cx = x / kIslandCell, cz = z / kIslandCell;
    const IslandPoi& p = poi(cx, cz);
    uint32_t col;
    if (c.bridge) col = 0xB48A52;
    else if (c.water) {
        col = mixColor(0x4A8AE8, 0x1A3A88, (sea_ - c.top) / 12.0f);
    } else if (c.path) col = 0xC9A46A;
    else {
        col = blockInfo(surfaceBlock(c, x, z, sea_, seed_)).mapColor;
        const Theme& t = *c.biome;
        if (t.tree != TreeKind::Cactus && c.top <= 37) col = mixColor(col, t.tree == TreeKind::Spruce ? 0x23482E : (t.tree == TreeKind::Dead ? 0x4A4038 : 0x2F6A22), c.forest * 0.55f);
        float slope = naturalHeight(x + 0.5f, z + 0.5f) - naturalHeight(x - 2.5f, z - 2.5f);
        col = shadeColor(col, clampf(0.84f + (c.top - sea_) * 0.011f + clampf(slope * 0.06f, -0.18f, 0.18f), 0.5f, 1.3f));
    }
    // A aldeia importada é desenhada acima do chão, antes da projeção das
    // construções fixas do mapa competitivo.
    for (const auto& village : terrain_.villagesNear(x, z, 0)) {
        for (const auto& home : village.homes()) {
            if (x >= home[0] - 1 && x <= home[0] + 7 && z >= home[1] - 1 && z <= home[1] + 7)
                col = blockInfo(village.biome == 2 ? B_SNOW : B_ROOF_RED).mapColor;
        }
    }
    if (p.exists && distXZ(p.center, Vec3(x + 0.5f, 0, z + 0.5f)) < p.radius) {
        col = isBuildingKind(p.kind) ? 0xB0644A : 0x9A7A4E;
    }
    return col;
}

}  // namespace zz
