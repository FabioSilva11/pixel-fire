#include "terrain.h"

#include <algorithm>
#include <cmath>

#include "../core/random.h"
#include "horizonte_adapter.h"

namespace zz {

namespace {

enum Reserve : uint8_t { R_NONE = 0, R_PATH = 1, R_CAMP = 2, R_TREE = 3, R_RIVER = 4 };

// Adaptador: estruturas escrevem direto no mundo finito (sem marcar chunks sujos).
class WorldSink : public BlockSink {
public:
    explicit WorldSink(World& w) : w_(w) {}
    uint8_t get(int x, int y, int z) const override { return w_.get(x, y, z); }
    void set(int x, int y, int z, uint8_t block) override { w_.set(x, y, z, block, false); }

private:
    World& w_;
};

void absorb(WorldLayout& layout, const DecorOut& out) {
    layout.lights.insert(layout.lights.end(), out.lights.begin(), out.lights.end());
    layout.campfires.insert(layout.campfires.end(), out.campfires.begin(), out.campfires.end());
}

struct HeightField {
    int w = 0, d = 0;
    std::vector<float> h;
    std::vector<float> natural;  // relevo antes de trilhas/acampamentos (para detectar pontes)
    std::vector<uint8_t> reserve;
    float& at(int x, int z) { return h[static_cast<size_t>(z) * w + x]; }
    float get(int x, int z) const {
        x = std::max(0, std::min(w - 1, x));
        z = std::max(0, std::min(d - 1, z));
        return h[static_cast<size_t>(z) * w + x];
    }
    float naturalAt(int x, int z) const { return natural.empty() ? get(x, z) : natural[static_cast<size_t>(z) * w + x]; }
    uint8_t& res(int x, int z) { return reserve[static_cast<size_t>(z) * w + x]; }
    bool inside(int x, int z) const { return x >= 0 && z >= 0 && x < w && z < d; }
};

struct MapShape {};

float naturalHeight(const MapShape&, float x, float z, int size, const Theme&, uint32_t seed, int sea) {
    horizonte::Terrain terrain(seed);
    float h=float(terrain.groundHeight(int(x),int(z))+1);
    // Somente a borda do cenário finito desce para a água; o miolo usa o relevo original.
    float edge=std::min(std::min(x,z),std::min(size-x,size-z));
    return lerpf(float(sea-4),h,smoothstepf(3,12,edge));
}

void flattenDisc(HeightField& hf, float cx, float cz, float radius, float target, float blend, uint8_t reserveTag) {
    int r = static_cast<int>(std::ceil(radius + blend));
    for (int z = static_cast<int>(cz) - r; z <= static_cast<int>(cz) + r; ++z) {
        for (int x = static_cast<int>(cx) - r; x <= static_cast<int>(cx) + r; ++x) {
            if (!hf.inside(x, z)) continue;
            float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (z + 0.5f - cz) * (z + 0.5f - cz));
            if (d <= radius) {
                hf.at(x, z) = target;
                if (reserveTag) hf.res(x, z) = reserveTag;
            } else if (d < radius + blend) {
                float t = smoothstepf(radius, radius + blend, d);
                hf.at(x, z) = lerpf(target, hf.at(x, z), t);
            }
        }
    }
}

// Trilha sinuosa entre dois pontos; segue o relevo suavizado e vira ponte sobre a água.
void carvePath(HeightField& hf, Vec2 a, Vec2 b, uint32_t seed, int sea) {
    Vec2 delta = b - a;
    float len = delta.length();
    if (len < 2) return;
    int steps = static_cast<int>(len);
    Vec2 perp(-delta.y / len, delta.x / len);
    float wobble = 3.0f + hashFloat(static_cast<int>(a.x), static_cast<int>(b.y), seed) * 4.0f;
    float waves = 1.0f + std::floor(len / 40.0f);
    std::vector<Vec2> pts(steps + 1);
    std::vector<float> heights(steps + 1);
    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) / steps;
        float w = std::sin(t * kPi * waves) * wobble * std::sin(t * kPi);
        pts[i] = a + delta * t + perp * w;
        heights[i] = std::max(static_cast<float>(sea + 1), hf.get(static_cast<int>(pts[i].x), static_cast<int>(pts[i].y)));
    }
    std::vector<float> smooth(heights);
    for (int pass = 0; pass < 3; ++pass) {
        for (int i = 0; i <= steps; ++i) {
            float sum = 0, n = 0;
            for (int k = -4; k <= 4; ++k) {
                int j = std::max(0, std::min(steps, i + k));
                sum += smooth[j];
                n += 1;
            }
            heights[i] = sum / n;
        }
        smooth = heights;
    }
    for (int i = 1; i <= steps; ++i) smooth[i] = clampf(smooth[i], smooth[i - 1] - 0.6f, smooth[i - 1] + 0.6f);
    for (int i = 0; i <= steps; ++i) {
        float px = pts[i].x, pz = pts[i].y;
        for (int z = static_cast<int>(pz) - 4; z <= static_cast<int>(pz) + 4; ++z) {
            for (int x = static_cast<int>(px) - 4; x <= static_cast<int>(px) + 4; ++x) {
                if (!hf.inside(x, z)) continue;
                float d = std::sqrt((x + 0.5f - px) * (x + 0.5f - px) + (z + 0.5f - pz) * (z + 0.5f - pz));
                if (d <= 1.5f) {
                    if (hf.res(x, z) != R_CAMP) {
                        hf.at(x, z) = std::max(smooth[i], static_cast<float>(sea + 1));
                        hf.res(x, z) = R_PATH;
                    }
                } else if (d < 4.0f && (hf.res(x, z) == R_NONE || hf.res(x, z) == R_RIVER) && hf.at(x, z) > sea) {
                    float t = smoothstepf(1.5f, 4.0f, d);
                    hf.at(x, z) = lerpf(smooth[i], hf.at(x, z), t);
                }
            }
        }
    }
}

void fillColumns(World& world, HeightField& hf, const Theme& theme, int sea, uint32_t seed) {
    horizonte::Terrain terrain(seed);
    for (Chunk* chunk:world.chunks()) horizonte::generate(terrain,*chunk,world.height());
    for (int z=0;z<hf.d;++z) for (int x=0;x<hf.w;++x) {
        int natural=terrain.groundHeight(x,z)+1;
        int top=std::max(2,std::min(world.height()-14,int(std::lround(hf.at(x,z)))));
        bool modified=top!=natural || hf.res(x,z)==R_PATH || hf.res(x,z)==R_CAMP;
        if (!modified) continue;
        bool bridge=hf.res(x,z)==R_PATH && hf.naturalAt(x,z)<=sea+.5f;
        int low=std::max(1,std::min(natural,top)-4);
        int high=std::min(world.height(),std::max(natural,top)+8);
        for (int y=low;y<high;++y) {
            uint8_t block=y<top-1 ? (y<top-4 ? B_STONE : B_DIRT) :
                y==top-1 ? (bridge ? B_PLANKS : hf.res(x,z)==R_PATH ? B_PATH : horizonte::surface(terrain,x,z)) :
                y<=sea ? B_WATER : B_AIR;
            world.set(x,y,z,block,false);
        }
        world.setGroundHeight(x,z,bridge ? std::max(top,sea+1) : top);
    }
}

}  // namespace

Theme applySkyVariant(const Theme& base, uint32_t seed, bool allowNight) {
    Theme t = base;
    Rng rng(seed ^ 0x5151F00Du);
    float azimuth = rng.range(0, kTwoPi);
    float elevation = std::asin(clampf(t.sky.sunDirection.y, -1, 1));
    int variant = rng.rangeInt(0, allowNight ? 4 : 3);
    if (t.sky.night < 0.5f) {
        if (variant == 2) {  // hora dourada
            elevation = 0.22f;
            t.sky.zenith = {0.32f, 0.42f, 0.78f};
            t.sky.horizon = {1.0f, 0.68f, 0.46f};
            t.sky.fog = {0.96f, 0.76f, 0.6f};
            t.sky.sunColor = {1.0f, 0.82f, 0.58f};
        } else if (variant == 3) {  // nublado
            t.sky.zenith = {0.52f, 0.57f, 0.66f};
            t.sky.horizon = {0.74f, 0.77f, 0.82f};
            t.sky.fog = {0.72f, 0.75f, 0.8f};
            t.sky.sunIntensity *= 0.6f;
            t.sky.ambient = t.sky.ambient + Vec3(0.08f, 0.08f, 0.08f);
            t.sky.fogEnd *= 0.85f;
        } else if (variant == 4) {  // noite enluarada
            elevation = 0.6f;
            t.sky.zenith = {0.03f, 0.04f, 0.11f};
            t.sky.horizon = {0.12f, 0.14f, 0.26f};
            t.sky.fog = {0.08f, 0.09f, 0.17f};
            t.sky.sunColor = {0.62f, 0.68f, 0.9f};
            t.sky.sunIntensity = 0.35f;
            t.sky.ambient = {0.28f, 0.3f, 0.4f};
            t.sky.night = 1.0f;
            t.sky.fogStart = 28.0f;
            t.sky.fogEnd = 95.0f;
        }
    }
    float ce = std::cos(elevation);
    t.sky.sunDirection = normalize(Vec3(ce * std::cos(azimuth), std::sin(elevation), ce * std::sin(azimuth)));
    return t;
}

WorldLayout generateLobbyWorld(World& world, const Theme& theme) {
    const int size = 64;
    const int sea = horizonte::kSea;
    world.reset(size, horizonte::kHeight, size);
    WorldLayout layout;
    layout.seaLevel = sea;
    HeightField hf;
    hf.w = size;
    hf.d = size;
    hf.h.resize(static_cast<size_t>(size) * size);
    hf.reserve.assign(static_cast<size_t>(size) * size, R_NONE);
    const uint32_t seed = 4242u;
    for (int z = 0; z < size; ++z)
        for (int x = 0; x < size; ++x) {
            hf.at(x,z)=naturalHeight(MapShape{},x+.5f,z+.5f,size,theme,seed,sea);
        }
    const float lobbyFloor=std::max(float(sea+2),hf.get(size/2,size/2));
    Camp camp{{size * 0.5f, lobbyFloor, size * 0.5f}, 6.0f, true, K_FIRE};
    flattenDisc(hf, camp.center.x, camp.center.z, 9.0f, lobbyFloor, 5.0f, R_CAMP);
    carvePath(hf, {size * 0.5f, size * 0.5f}, {size * 0.5f + 4, size - 4.0f}, seed, sea);
    hf.natural = hf.h;
    fillColumns(world, hf, theme, sea, seed);
    // A vegetação do Horizonte já acompanha o terreno.
    int cy = world.groundHeight(static_cast<int>(camp.center.x), static_cast<int>(camp.center.z));
    camp.center.y = static_cast<float>(cy);
    int fx = static_cast<int>(camp.center.x) + 3, fz = static_cast<int>(camp.center.z) - 2;
    world.set(fx, cy - 1, fz, B_CAMPFIRE, false);
    layout.campfires.push_back({fx + 0.5f, static_cast<float>(cy), fz + 0.5f});
    layout.lights.push_back({fx + 0.5f, cy + 0.8f, fz + 0.5f});
    world.set(fx + 2, cy, fz - 2, B_CRATE, false);
    world.set(fx + 2, cy + 1, fz - 2, B_CRATE, false);
    world.set(fx + 3, cy, fz - 2, B_HAY, false);
    {
        WorldSink sink(world);
        DecorOut out;
        lanternPost(sink, static_cast<int>(camp.center.x) - 6, cy, static_cast<int>(camp.center.z) - 7, out);
        absorb(layout, out);
    }
    for (int dz = -3; dz <= 3; ++dz)
        for (int dx = -3; dx <= 3; ++dx) {
            int x = static_cast<int>(camp.center.x) + dx, z = static_cast<int>(camp.center.z) + dz;
            if (dx * dx + dz * dz <= 9 && blockInfo(world.get(x, cy, z)).shape == Shape::Cross) world.set(x, cy, z, B_AIR, false);
        }
    layout.start = camp.center;
    layout.startYaw = 0;
    world.markAllDirty();
    return layout;
}

}  // namespace zz
