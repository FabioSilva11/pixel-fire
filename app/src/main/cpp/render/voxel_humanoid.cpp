#include "voxel_humanoid.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <utility>

#include "../core/log.h"
#include "../core/math.h"
#include "blocky.h"

namespace zz {

namespace {

constexpr float kPx = kBlockyPixel;
constexpr int kTexel = 2;  // texels por pixel do personagem (detalhe de meio pixel, como no protótipo)

struct Rgb {
    int r, g, b;
};

Rgb hex(uint32_t c) { return {static_cast<int>((c >> 16) & 255u), static_cast<int>((c >> 8) & 255u), static_cast<int>(c & 255u)}; }
Rgb scaled(Rgb c, float f) {
    return {std::min(255, static_cast<int>(c.r * f)), std::min(255, static_cast<int>(c.g * f)), std::min(255, static_cast<int>(c.b * f))};
}

// Mesmo ruído do protótipo (Math.sin em double): as texturas saem iguais às da página de teste.
double noiseHash(double x, double y, double s) {
    const double z = std::sin(x * 127.1 + y * 311.7 + s * 74.7) * 43758.5453;
    return z - std::floor(z);
}
int pick4(double v) { return std::min(3, static_cast<int>(v * 4.0)); }

struct Pal {
    Rgb c[4];
};
Pal pal4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return {{hex(a), hex(b), hex(c), hex(d)}}; }

struct Style {
    Pal skin, hair, jacket, shirt, pants, shoe, white;
    Rgb jacketLight, jacketLighter, jacketDark, pantsLight, pantsDark;
    Rgb brow, eyeWhite, iris, pupil, nose, mouth, seam, collar1, collar2, belt, buckle, sole, lace, cuff1, cuff2, cuffDark;
};

// Herói: as mesmas paletas do protótipo (cabelo castanho, jaqueta escura aberta, camiseta branca, calça e tênis).
Style heroStyle() {
    Style s;
    s.skin = pal4(0xc98a62, 0xd49870, 0xdca47c, 0xe3b089);
    s.hair = pal4(0x3f2214, 0x552e1b, 0x653822, 0x76452a);
    s.jacket = pal4(0x1f2125, 0x26292d, 0x2e3136, 0x383b42);
    s.shirt = pal4(0xd9d9d9, 0xe6e6e6, 0xefefef, 0xf7f7f7);
    s.pants = pal4(0x212327, 0x282a2f, 0x2f3237, 0x383b41);
    s.shoe = pal4(0x141518, 0x1b1c20, 0x232529, 0x2b2d32);
    s.white = pal4(0xe8e8e8, 0xf0f0f0, 0xf6f6f6, 0xffffff);
    s.jacketLight = hex(0x373a41);
    s.jacketLighter = hex(0x474b53);
    s.jacketDark = hex(0x18191c);
    s.pantsLight = hex(0x43474e);
    s.pantsDark = hex(0x1a1b1e);
    s.brow = hex(0x3a2216);
    s.eyeWhite = hex(0xf2f2f2);
    s.iris = hex(0x4a2c1c);
    s.pupil = hex(0x2a170e);
    s.nose = hex(0xb97a55);
    s.mouth = hex(0x9e5f45);
    s.seam = hex(0x121315);
    s.collar1 = hex(0x4c5058);
    s.collar2 = hex(0x3d4048);
    s.belt = hex(0x111214);
    s.buckle = hex(0x8a8f96);
    s.sole = hex(0xcfcfcf);
    s.lace = hex(0xe2e2e2);
    s.cuff1 = hex(0xd8d8d8);
    s.cuff2 = hex(0xececec);
    s.cuffDark = hex(0x1a1b1e);
    return s;
}



// ----------------------------------------------------------------------------- pintura das faces

struct Canvas {
    Image* img;
    int x0, y0, w, h;
    void px(int x, int y, Rgb c) const {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        uint8_t* p = &img->rgba[(static_cast<size_t>(y0 + y) * img->width + x0 + x) * 4];
        p[0] = static_cast<uint8_t>(c.r);
        p[1] = static_cast<uint8_t>(c.g);
        p[2] = static_cast<uint8_t>(c.b);
        p[3] = 255;
    }
    void rect(int x, int y, int rw, int rh, Rgb c) const {
        for (int j = 0; j < rh; ++j)
            for (int i = 0; i < rw; ++i) px(x + i, y + j, c);
    }
};

void noise(const Canvas& g, const Pal& p, int seed) {
    for (int y = 0; y < g.h; ++y)
        for (int x = 0; x < g.w; ++x) {
            const double v = noiseHash(x, y, seed);
            g.px(x, y, p.c[v > .9 ? 3 : v > .66 ? 2 : v > .25 ? 1 : 0]);
        }
}



// Jaqueta com xadrez discreto (blocos 2x2 claros e escuros), como na referência.
void paintJacket(const Canvas& g, const Style& s, int seed) {
    noise(g, s.jacket, seed);
    for (int y = 0; y < g.h; y += 2)
        for (int x = 0; x < g.w; x += 2) {
            const double v = noiseHash(x * .5, y * .5, seed + 9);
            if (v > .8) g.rect(x, y, 2, 2, v > .94 ? s.jacketLighter : s.jacketLight);
            else if (v < .1) g.rect(x, y, 2, 2, s.jacketDark);
        }
}

void paintPants(const Canvas& g, const Style& s, int seed) {
    noise(g, s.pants, seed);
    for (int y = 0; y < g.h; y += 2)
        for (int x = 0; x < g.w; x += 3) {
            const double v = noiseHash(x, y, seed + 3);
            if (v > .86) g.rect(x, y, 3, 2, s.pantsLight);
            else if (v < .1) g.rect(x, y, 2, 2, s.pantsDark);
        }
}

enum Painter {
    SKIN, HAIR, JACKET, PANTS, SHOE, JACKET_FRONT, CHEST_FRONT, HIPS_FRONT, SLEEVE_CUFF, SHOE_SIDE, SHOE_TOP, PANTS_CUFF,
    FACE, HEAD_SIDE, HEAD_BACK, PAINTER_COUNT
};

void paintJacketFront(const Canvas& g, const Style& s) {
    paintJacket(g, s, 42);
    const float c = g.w / 2.0f;
    const int open = 2, a = static_cast<int>(std::floor(c - open)), b = static_cast<int>(std::ceil(c + open));
    for (int y = 0; y < g.h; ++y) {
        for (int x = a; x < b; ++x) g.px(x, y, s.shirt.c[pick4(noiseHash(x, y, 7))]);
        g.px(a - 1, y, s.seam);
        g.px(b, y, s.seam);
    }

}

void paint(int painter, const Canvas& g, const Style& s) {
    switch (painter) {
        case SKIN: noise(g, s.skin, 21); break;
        case HAIR: noise(g, s.hair, 31); break;
        case JACKET: paintJacket(g, s, 41); break;
        case PANTS: paintPants(g, s, 51); break;
        case SHOE: noise(g, s.shoe, 61); break;
        case JACKET_FRONT: paintJacketFront(g, s); break;
        case CHEST_FRONT: {
            // Peito: mesma abertura, gola levantada nas duas linhas de cima.
            paintJacketFront(g, s);
            const float c = g.w / 2.0f;
            for (int x = 0; x < g.w; ++x)
                if (std::fabs(x + .5f - c) > 3) {
                    g.px(x, 0, s.collar1);
                    g.px(x, 1, s.collar2);
                }
            break;
        }
        case HIPS_FRONT: {
            paintPants(g, s, 52);
            for (int x = 0; x < g.w; ++x) g.px(x, 0, s.belt);
            g.px(g.w / 2 - 1, 0, s.buckle);
            g.px(g.w / 2, 0, s.buckle);
            break;
        }
        case SLEEVE_CUFF: {
            // Manga com o punho claro perto do pulso (o braço repousa para baixo: o pulso é a base da face).
            paintJacket(g, s, 43);
            for (int x = 0; x < g.w; ++x) {
                g.px(x, g.h - 3, s.cuff1);
                g.px(x, g.h - 2, s.cuff2);
            }
            break;
        }
        case SHOE_SIDE: {
            noise(g, s.shoe, 62);
            for (int x = 0; x < g.w; ++x) {
                g.px(x, g.h - 1, s.white.c[pick4(noiseHash(x, 1, 3))]);
                g.px(x, g.h - 2, s.sole);
            }
            break;
        }
        case SHOE_TOP: {
            noise(g, s.shoe, 63);
            for (int y = 1; y < g.h - 1; y += 2) {
                g.px(g.w / 2 - 1, y, s.lace);
                g.px(g.w / 2, y, s.lace);
            }
            break;
        }
        case PANTS_CUFF: {
            paintPants(g, s, 53);
            for (int x = 0; x < g.w; ++x) g.px(x, g.h - 1, s.cuffDark);
            break;
        }
        case FACE: {
            // Rosto 16x16: franja, sobrancelhas, olhos com pupila, nariz e boca.
            noise(g, s.skin, 22);
            for (int x = 0; x < g.w; ++x) {
                const int fringe = 3 + static_cast<int>(noiseHash(x, 0, 5) * 2.4);
                for (int y = 0; y < fringe; ++y) g.px(x, y, s.hair.c[pick4(noiseHash(x, y, 32))]);
            }
            for (int y = 0; y < 9; ++y) {
                g.px(0, y, s.hair.c[1]);
                g.px(g.w - 1, y, s.hair.c[1]);
                if (y < 6) {
                    g.px(1, y, s.hair.c[2]);
                    g.px(g.w - 2, y, s.hair.c[2]);
                }
            }
            for (int x : {3, 4, 5, 10, 11, 12}) g.px(x, 6, s.brow);
            const std::pair<int, Rgb> eyes[] = {{3, s.eyeWhite}, {4, s.eyeWhite}, {5, s.iris}, {10, s.iris}, {11, s.eyeWhite}, {12, s.eyeWhite}};
            for (const auto& e : eyes) {
                g.px(e.first, 8, e.second);
                g.px(e.first, 9, e.second);
            }
            g.px(5, 8, s.pupil);
            g.px(10, 8, s.pupil);
            g.px(7, 11, s.nose);
            g.px(8, 11, s.nose);
            for (int x : {6, 7, 8, 9}) g.px(x, 13, s.mouth);
            
            break;
        }
        case HEAD_SIDE: {
            noise(g, s.skin, 23);
            for (int x = 0; x < g.w; ++x) {
                const int d = 6 + static_cast<int>(noiseHash(x, 2, 6) * 3);
                for (int y = 0; y < d; ++y) g.px(x, y, s.hair.c[pick4(noiseHash(x, y, 33))]);
            }
            g.px(static_cast<int>(g.w * .45f), 9, s.nose);
            g.px(static_cast<int>(g.w * .45f), 10, s.nose);
            break;
        }
        case HEAD_BACK: {
            noise(g, s.hair, 34);
            for (int x = 0; x < g.w; ++x)
                for (int y = 12 + static_cast<int>(noiseHash(x, 4, 8) * 2); y < g.h; ++y) g.px(x, y, s.skin.c[pick4(noiseHash(x, y, 24))]);
            break;
        }
        default: break;
    }
}

// ----------------------------------------------------------------------------- atlas e caixas

struct Faces {
    int px, nx, py, ny, pz, nz;  // pintor de cada face (+X = esquerda do personagem, +Z = frente)
};
Faces all(int p) { return {p, p, p, p, p, p}; }

struct AtlasRect {
    int x, y, w, h;
};

// Empacota as faces em prateleiras com 1 texel de borda (copiada da própria face, sem vazar no mipmap).
class Atlas {
public:
    AtlasRect get(int painter, int w, int h) {
        const uint64_t key = (static_cast<uint64_t>(painter) << 40) | (static_cast<uint64_t>(w) << 20) | static_cast<uint64_t>(h);
        auto it = rects_.find(key);
        if (it != rects_.end()) return it->second;
        if (x_ + w + 1 > kWidth) {
            x_ = 1;
            y_ += row_ + 2;
            row_ = 0;
        }
        AtlasRect r{x_, y_, w, h};
        x_ += w + 2;
        row_ = std::max(row_, h);
        rects_[key] = r;
        order_.push_back({painter, r});
        return r;
    }
    int height() const {
        int need = y_ + row_ + 2, h = 64;
        while (h < need) h *= 2;
        return h;
    }
    static constexpr int kWidth = 256;
    const std::vector<std::pair<int, AtlasRect>>& order() const { return order_; }

private:
    std::map<uint64_t, AtlasRect> rects_;
    std::vector<std::pair<int, AtlasRect>> order_;
    int x_ = 1, y_ = 1, row_ = 0;
};

struct Part {
    int bone;
    Vec3 center, size;  // pixels do personagem, na pose de repouso
    Faces faces;
    Quat rot;
};

Quat quatFromTo(const Vec3& a, const Vec3& b) {
    const Vec3 u = normalize(a), v = normalize(b);
    const float d = dot(u, v);
    if (d > 0.9999f) return {};
    if (d < -0.9999f) return Quat::axisAngle(std::fabs(u.x) < .9f ? Vec3(1, 0, 0) : Vec3(0, 1, 0), kPi);
    return Quat::axisAngle(cross(u, v), std::acos(clampf(d, -1, 1)));
}

void emitBox(ModelMesh& mesh, const Part& p, const Vec3& joint, const AtlasRect rects[6], int atlasH) {
    const Vec3 h = p.size * 0.5f;
    const Mat4 R = Mat4::fromQuat(p.rot);
    // Cantos de cada face vistos de fora: inferior esquerdo, inferior direito, superior direito, superior esquerdo.
    const Vec3 corners[6][4] = {
        {{h.x, -h.y, h.z}, {h.x, -h.y, -h.z}, {h.x, h.y, -h.z}, {h.x, h.y, h.z}},          // +X
        {{-h.x, -h.y, -h.z}, {-h.x, -h.y, h.z}, {-h.x, h.y, h.z}, {-h.x, h.y, -h.z}},      // -X
        {{-h.x, h.y, h.z}, {h.x, h.y, h.z}, {h.x, h.y, -h.z}, {-h.x, h.y, -h.z}},          // +Y (linha de cima da textura = trás)
        {{-h.x, -h.y, -h.z}, {h.x, -h.y, -h.z}, {h.x, -h.y, h.z}, {-h.x, -h.y, h.z}},      // -Y
        {{-h.x, -h.y, h.z}, {h.x, -h.y, h.z}, {h.x, h.y, h.z}, {-h.x, h.y, h.z}},          // +Z
        {{h.x, -h.y, -h.z}, {-h.x, -h.y, -h.z}, {-h.x, h.y, -h.z}, {h.x, h.y, -h.z}},      // -Z
    };
    const Vec3 normals[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int f = 0; f < 6; ++f) {
        const AtlasRect& r = rects[f];
        const float u0 = static_cast<float>(r.x) / Atlas::kWidth, u1 = static_cast<float>(r.x + r.w) / Atlas::kWidth;
        const float v0 = static_cast<float>(r.y) / atlasH, v1 = static_cast<float>(r.y + r.h) / atlasH;
        const float uv[4][2] = {{u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}};
        const Vec3 n = R.transformDir(normals[f]);
        const uint16_t base = static_cast<uint16_t>(mesh.vertices.size() / 8);
        for (int i = 0; i < 4; ++i) {
            const Vec3 q = (p.center + R.transformDir(corners[f][i]) - joint) * kPx;
            mesh.vertices.insert(mesh.vertices.end(), {q.x, q.y, q.z, n.x, n.y, n.z, uv[i][0], uv[i][1]});
        }
        mesh.indices.insert(mesh.indices.end(), {base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2), base,
                                                 static_cast<uint16_t>(base + 2), static_cast<uint16_t>(base + 3)});
    }
}

// ----------------------------------------------------------------------------- esqueleto

struct JointDef {
    std::string name, parent;
    Vec3 pos;  // pixels, pose de repouso (braços para baixo), pés em y = 0, olhando para +Z
};

const char* kFingers[] = {"Index", "Middle", "Ring", "Pinky"};
const float kFingerZ[] = {.96f, .32f, -.32f, -.96f};
const float kFingerLen[4][3] = {{.95f, .75f, .6f}, {1.05f, .8f, .62f}, {.95f, .75f, .6f}, {.72f, .6f, .5f}};
const Vec3 kThumbDir{0.0f, -0.548f, 0.837f};
const float kThumbLen[3] = {.95f, .8f, .65f};

std::vector<JointDef> skeletonDefs() {
    std::vector<JointDef> j = {
        {"root", "", {0, 0, 0}},
        {"Hips", "root", {0, 12.5f, 0}},
        {"Spine", "Hips", {0, 14.5f, 0}},
        {"Spine1", "Spine", {0, 17.5f, 0}},
        {"Spine2", "Spine1", {0, 20.5f, 0}},
        {"Neck", "Spine2", {0, 24, 0}},
        {"Head", "Neck", {0, 24.3f, 0}},
        {"HeadTop_End", "Head", {0, 32.3f, 0}},
        {"LeftEye", "Head", {1.9f, 28.6f, 4}},
        {"RightEye", "Head", {-1.9f, 28.6f, 4}},
    };
    for (const char* side : {"Left", "Right"}) {
        const std::string s = side;
        const float k = s == "Left" ? 1.0f : -1.0f;
        j.push_back({s + "Shoulder", "Spine2", {1.2f * k, 22.6f, 0}});
        j.push_back({s + "Arm", s + "Shoulder", {6.0f * k, 22.4f, 0}});
        j.push_back({s + "ForeArm", s + "Arm", {6.0f * k, 17.6f, 0}});
        j.push_back({s + "Hand", s + "ForeArm", {6.0f * k, 12.8f, 0}});
        Vec3 t{6.0f * k, 12.3f, 1.2f};
        for (int i = 1; i <= 4; ++i) {
            j.push_back({s + "HandThumb" + std::to_string(i), i == 1 ? s + "Hand" : s + "HandThumb" + std::to_string(i - 1), t});
            if (i <= 3) t = t + kThumbDir * kThumbLen[i - 1];
        }
        for (int f = 0; f < 4; ++f) {
            float y = 10.5f;
            for (int i = 1; i <= 4; ++i) {
                const std::string name = s + "Hand" + kFingers[f] + std::to_string(i);
                j.push_back({name, i == 1 ? s + "Hand" : s + "Hand" + kFingers[f] + std::to_string(i - 1), {6.0f * k, y, kFingerZ[f]}});
                if (i <= 3) y -= kFingerLen[f][i - 1];
            }
        }
        j.push_back({s + "UpLeg", "Hips", {2.0f * k, 12, 0}});
        j.push_back({s + "Leg", s + "UpLeg", {2.0f * k, 6.4f, 0}});
        j.push_back({s + "Foot", s + "Leg", {2.0f * k, 1.4f, 0}});
        j.push_back({s + "ToeBase", s + "Foot", {2.0f * k, .6f, 2.3f}});
        j.push_back({s + "Toe_End", s + "ToeBase", {2.0f * k, .6f, 3.6f}});
    }
    return j;
}

// ----------------------------------------------------------------------------- clipes do Mixamo

struct ZzaClip {
    std::string id;
    float fps = 30;
    bool loop = true;
    int frames = 0;
    std::vector<std::string> bones;
    std::vector<float> rot;   // ossos x quadros x 4
    std::vector<float> hips;  // quadros x 3 (translação local do quadril, unidades do modelo)
};

bool parseZza(const std::string& id, const std::vector<uint8_t>& b, ZzaClip& out) {
    size_t o = 0;
    auto u32 = [&](uint32_t& v) { if (o + 4 > b.size()) return false; std::memcpy(&v, &b[o], 4); o += 4; return true; };
    auto f32 = [&](float& v) { if (o + 4 > b.size()) return false; std::memcpy(&v, &b[o], 4); o += 4; return true; };
    if (b.size() < 20 || std::memcmp(b.data(), "ZZA1", 4) != 0) return false;
    o = 4;
    uint32_t frames = 0, bones = 0, flags = 0;
    if (!u32(frames) || !f32(out.fps) || !u32(bones) || !u32(flags)) return false;
    if (frames < 2 || frames > 4000 || bones == 0 || bones > 128 || !(out.fps > 0)) return false;
    out.id = id;
    out.frames = static_cast<int>(frames);
    out.loop = (flags & 1u) != 0;
    out.rot.resize(static_cast<size_t>(bones) * frames * 4);
    for (uint32_t i = 0; i < bones; ++i) {
        if (o + 1 > b.size()) return false;
        const size_t len = b[o++];
        if (o + len > b.size()) return false;
        out.bones.emplace_back(reinterpret_cast<const char*>(&b[o]), len);
        o += len;
        const size_t bytes = static_cast<size_t>(frames) * 16;
        if (o + bytes > b.size()) return false;
        std::memcpy(&out.rot[static_cast<size_t>(i) * frames * 4], &b[o], bytes);
        o += bytes;
    }
    out.hips.resize(static_cast<size_t>(frames) * 3);
    const size_t bytes = static_cast<size_t>(frames) * 12;
    if (o + bytes != b.size()) return false;
    std::memcpy(out.hips.data(), &b[o], bytes);
    return true;
}

const std::vector<ZzaClip>& mixamoLibrary() {
    static std::mutex mutex;
    static std::vector<ZzaClip> clips;
    static bool loaded = false;
    std::lock_guard<std::mutex> lock(mutex);
    if (loaded) return clips;
    loaded = true;
    for (const char* id : {"hero-aim-idle", "die", "dance", "swim"}) {
        std::vector<uint8_t> bytes;
        ZzaClip clip;
        if (!platform::readAsset(std::string("anims/") + id + ".zza", bytes) || !parseZza(id, bytes, clip)) {
            LOGE("Animação %s ausente ou inválida", id);
            continue;
        }
        clips.push_back(std::move(clip));
    }
    return clips;
}

}  // namespace

void buildVoxelHumanoid(const Look& look, uint32_t seed, std::vector<ModelNode>& nodes, std::vector<ModelMesh>& meshes, Image& image,
                        HumanoidRig& rig) {
    (void)seed;
    const Style style = heroStyle();
    const std::vector<JointDef> defs = skeletonDefs();
    std::map<std::string, int> index;
    nodes.clear();
    for (const JointDef& d : defs) {
        ModelNode n;
        n.name = d.name;
        n.parent = d.parent.empty() ? -1 : index.at(d.parent);
        const Vec3 parentPos = d.parent.empty() ? Vec3() : defs[static_cast<size_t>(index.at(d.parent))].pos;
        n.translation = (d.pos - parentPos) * kPx;
        index[d.name] = static_cast<int>(nodes.size());
        nodes.push_back(n);
    }
    auto bone = [&](const std::string& n) { return index.at(n); };
    rig.root = bone("root");
    rig.torso = bone("Spine");
    rig.head = bone("Head");
    rig.armR = bone("RightArm");
    rig.armL = bone("LeftArm");
    rig.legR = bone("RightUpLeg");
    rig.legL = bone("LeftUpLeg");
    rig.foreR = bone("RightForeArm");
    rig.foreL = bone("LeftForeArm");
    rig.kneeR = bone("RightLeg");
    rig.kneeL = bone("LeftLeg");
    rig.footR = bone("RightFoot");
    rig.footL = bone("LeftFoot");

    // Peças (pixels, pose de repouso), as mesmas do protótipo com os braços para baixo.
    std::vector<Part> parts;
    auto add = [&](const std::string& b, Vec3 c, Vec3 s, Faces f, Quat r = Quat()) { parts.push_back({bone(b), c, s, f, r}); };
    add("Hips", {0, 12.6f, 0}, {8, 3.2f, 4}, {PANTS, PANTS, PANTS, PANTS, HIPS_FRONT, PANTS});
    add("Spine", {0, 15.75f, 0}, {8.2f, 3.4f, 4.2f}, {JACKET, JACKET, JACKET, JACKET, JACKET_FRONT, JACKET});
    add("Spine1", {0, 18.95f, 0}, {8.2f, 3.1f, 4.2f}, {JACKET, JACKET, JACKET, JACKET, JACKET_FRONT, JACKET});
    add("Spine2", {0, 22.2f, 0}, {8.2f, 3.6f, 4.2f}, {JACKET, JACKET, JACKET, JACKET, CHEST_FRONT, JACKET});
    add("Spine2", {0, 24.2f, -1.6f}, {5.6f, .7f, 1.2f}, all(JACKET));
    add("Head", {0, 28.3f, 0}, {8, 8, 8}, {HEAD_SIDE, HEAD_SIDE, HAIR, SKIN, FACE, HEAD_BACK});
    const float tufts[][6] = {{-3.4f, 32.5f, 3.3f, 1.4f, .8f, 1.6f}, {-1.6f, 32.7f, 3.6f, 1.6f, 1, 1.2f}, {.6f, 32.6f, 3.5f, 1.5f, .9f, 1.4f},
                              {2.6f, 32.5f, 3.2f, 1.6f, .8f, 1.8f}, {-3.9f, 30.6f, 1.2f, .8f, 3.2f, 3.6f}, {3.9f, 30.6f, 1.0f, .8f, 3.0f, 3.8f},
                              {0, 32.6f, 0, 7.6f, .7f, 6.2f}, {-2.2f, 33.0f, -1.2f, 2.4f, .7f, 2.6f}, {1.8f, 33.1f, -.4f, 2.2f, .8f, 2.4f},
                              {0, 30.6f, -3.9f, 7.4f, 3.6f, .8f}, {-3.95f, 29.0f, -2.4f, .8f, 2.6f, 2.6f}, {3.95f, 29.1f, -2.2f, .8f, 2.4f, 2.8f},
                              {-.6f, 31.9f, 4.25f, 2.0f, 1.2f, .7f}, {2.0f, 31.7f, 4.25f, 1.4f, 1.4f, .6f}, {-2.8f, 31.6f, 4.2f, 1.2f, 1.6f, .6f}};
    const int tuftCount = 15;
    for (int i = 0; i < tuftCount; ++i) {
        const float* t = tufts[i];
        add("Head", {t[0], t[1], t[2]}, {t[3], t[4], t[5]}, all(HAIR));
    }
    for (const char* sideName : {"Left", "Right"}) {
        const std::string s = sideName;
        const float k = s == "Left" ? 1.0f : -1.0f;
        add(s + "Arm", {6.0f * k, 20.75f, 0}, {3.4f, 6.7f, 3.4f}, all(JACKET));
        add(s + "ForeArm", {6.0f * k, 15.25f, 0}, {3.15f, 5.1f, 3.15f}, {SLEEVE_CUFF, SLEEVE_CUFF, JACKET, JACKET, SLEEVE_CUFF, SLEEVE_CUFF});
        
            add(s + "Hand", {6.0f * k, 11.65f, 0}, {1.15f, 2.3f, 2.7f}, all(SKIN));
            for (int f = 0; f < 4; ++f) {
                float y = 10.5f;
                for (int i = 1; i <= 3; ++i) {
                    const float len = kFingerLen[f][i - 1];
                    add(s + "Hand" + kFingers[f] + std::to_string(i), {6.0f * k, y - len * .5f, kFingerZ[f]}, {.56f, len + .08f, .56f}, all(SKIN));
                    y -= len;
                }
            }
            Vec3 t{6.0f * k, 12.3f, 1.2f};
            const Quat along = quatFromTo({0, 1, 0}, kThumbDir);
            for (int i = 1; i <= 3; ++i) {
                const float len = kThumbLen[i - 1];
                add(s + "HandThumb" + std::to_string(i), t + kThumbDir * (len * .5f), {.72f, len + .1f, .72f}, all(SKIN), along);
                t = t + kThumbDir * len;
            }
        
        add(s + "UpLeg", {2.0f * k, 9.25f, 0}, {3.95f, 6.1f, 3.95f}, all(PANTS));
        add(s + "Leg", {2.0f * k, 3.85f, 0}, {3.85f, 5.0f, 3.85f}, {PANTS_CUFF, PANTS_CUFF, PANTS, PANTS, PANTS_CUFF, PANTS_CUFF});
        
            add(s + "Foot", {2.0f * k, .75f, -.1f}, {4.05f, 1.5f, 4.3f}, {SHOE_SIDE, SHOE_SIDE, SHOE_TOP, SHOE, SHOE_SIDE, SHOE_SIDE});
            add(s + "ToeBase", {2.0f * k, .65f, 2.85f}, {4.05f, 1.3f, 1.5f}, {SHOE_SIDE, SHOE_SIDE, SHOE, SHOE, SHOE_SIDE, SHOE_SIDE});
        
    }

    // Atlas: cada face (pintor, largura, altura) pintada uma vez.
    Atlas atlas;
    struct Pending {
        AtlasRect rects[6];
    };
    std::vector<Pending> pending(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        const Part& p = parts[i];
        const int fw[6] = {static_cast<int>(std::lround(p.size.z * kTexel)), static_cast<int>(std::lround(p.size.z * kTexel)),
                           static_cast<int>(std::lround(p.size.x * kTexel)), static_cast<int>(std::lround(p.size.x * kTexel)),
                           static_cast<int>(std::lround(p.size.x * kTexel)), static_cast<int>(std::lround(p.size.x * kTexel))};
        const int fh[6] = {static_cast<int>(std::lround(p.size.y * kTexel)), static_cast<int>(std::lround(p.size.y * kTexel)),
                           static_cast<int>(std::lround(p.size.z * kTexel)), static_cast<int>(std::lround(p.size.z * kTexel)),
                           static_cast<int>(std::lround(p.size.y * kTexel)), static_cast<int>(std::lround(p.size.y * kTexel))};
        const int painters[6] = {p.faces.px, p.faces.nx, p.faces.py, p.faces.ny, p.faces.pz, p.faces.nz};
        for (int f = 0; f < 6; ++f) pending[i].rects[f] = atlas.get(painters[f], std::max(1, fw[f]), std::max(1, fh[f]));
    }
    const int atlasH = atlas.height();
    image.width = Atlas::kWidth;
    image.height = atlasH;
    image.rgba.assign(static_cast<size_t>(image.width) * image.height * 4, 255);
    for (const auto& [painter, r] : atlas.order()) {
        paint(painter, Canvas{&image, r.x, r.y, r.w, r.h}, style);
        // Borda de 1 texel repetindo a face: nada vaza entre faces vizinhas ao reduzir a textura.
        auto copy = [&](int dx, int dy, int sx, int sy) {
            if (dx < 0 || dy < 0 || dx >= image.width || dy >= image.height) return;
            std::memcpy(&image.rgba[(static_cast<size_t>(dy) * image.width + dx) * 4], &image.rgba[(static_cast<size_t>(sy) * image.width + sx) * 4], 4);
        };
        for (int x = -1; x <= r.w; ++x) {
            const int sx = r.x + std::clamp(x, 0, r.w - 1);
            copy(r.x + x, r.y - 1, sx, r.y);
            copy(r.x + x, r.y + r.h, sx, r.y + r.h - 1);
        }
        for (int y = 0; y < r.h; ++y) {
            copy(r.x - 1, r.y + y, r.x, r.y + y);
            copy(r.x + r.w, r.y + y, r.x + r.w - 1, r.y + y);
        }
    }

    // Uma malha por osso com peças.
    meshes.clear();
    std::vector<int> meshOf(nodes.size(), -1);
    for (size_t i = 0; i < parts.size(); ++i) {
        const Part& p = parts[i];
        if (meshOf[static_cast<size_t>(p.bone)] < 0) {
            meshOf[static_cast<size_t>(p.bone)] = static_cast<int>(meshes.size());
            meshes.emplace_back();
        }
        emitBox(meshes[static_cast<size_t>(meshOf[static_cast<size_t>(p.bone)])], p, defs[static_cast<size_t>(p.bone)].pos, pending[i].rects, atlasH);
    }
    for (size_t i = 0; i < nodes.size(); ++i) nodes[i].mesh = meshOf[i];
}

namespace {

// Ossos que as poses da página movem. Com a arma nas mãos a IK assume os braços; sem ela (correndo com a
// arma nas costas, no lobby) eles balançam pelos canais destes clipes.
enum SiteBone {
    SB_HIPS, SB_SPINE, SB_SPINE1, SB_SPINE2, SB_NECK, SB_HEAD, SB_ULEG_L, SB_ULEG_R, SB_LEG_L, SB_LEG_R, SB_FOOT_L, SB_FOOT_R,
    SB_ARM_L, SB_ARM_R, SB_FORE_L, SB_FORE_R, SB_COUNT
};
const char* kSiteBones[SB_COUNT] = {"Hips", "Spine", "Spine1", "Spine2", "Neck", "Head", "LeftUpLeg", "RightUpLeg", "LeftLeg", "RightLeg",
                                    "LeftFoot", "RightFoot", "LeftArm", "RightArm", "LeftForeArm", "RightForeArm"};

struct SitePose {
    Quat r[SB_COUNT];
    float hipsY = 12.5f;  // pixels
};

// Euler 'YXZ' do three.js (a página usa rot(nome, x, y, z)).
Quat euler(float x, float y, float z) { return Quat::axisAngle({0, 1, 0}, y) * Quat::axisAngle({1, 0, 0}, x) * Quat::axisAngle({0, 0, 1}, z); }

// legs() da página: coxa balança, joelho dobra na fase de balanço, pé compensa, quadril quica.
float siteLegs(SitePose& p, float t, float freq, float amp, float knee, float bob) {
    const float ph = t * kTwoPi * freq;
    for (int side = 0; side < 2; ++side) {
        const float q = ph + (side ? kPi : 0.0f);
        const float thigh = -amp * std::sin(q);
        const float k = knee * std::max(0.0f, std::sin(q + 1.25f)) + knee * .12f;
        p.r[SB_ULEG_L + side] = euler(thigh, 0, 0);
        p.r[SB_LEG_L + side] = euler(k, 0, 0);
        p.r[SB_FOOT_L + side] = euler(-(thigh + k) * .55f, 0, 0);
    }
    p.hipsY = 12.5f - .15f + bob * std::fabs(std::cos(ph));
    return ph;
}

// Braços balançando ao contrário das pernas (repouso para baixo: rx positivo leva o braço para trás).
void siteArms(SitePose& p, float ph, float swing, float elbow) {
    const float s = std::sin(ph);
    p.r[SB_ARM_L] = euler(swing * s - .15f * swing, 0, .08f);
    p.r[SB_ARM_R] = euler(-swing * s - .15f * swing, 0, -.08f);
    p.r[SB_FORE_L] = euler(-elbow - .25f * elbow * std::max(0.0f, -s), 0, 0);
    p.r[SB_FORE_R] = euler(-elbow - .25f * elbow * std::max(0.0f, s), 0, 0);
}

// Agachado: coxas à frente, joelhos dobrados e pés planos; o quadril desce o que as pernas encurtaram
// (coxa 5,6 px e canela 5 px), então os pés continuam no chão.
void crouchLegs(SitePose& p, float thighL, float thighR, float kneeL, float kneeR) {
    const float thigh[2] = {thighL, thighR}, knee[2] = {kneeL, kneeR};
    float drop = 0;
    for (int side = 0; side < 2; ++side) {
        p.r[SB_ULEG_L + side] = euler(thigh[side], 0, side ? -.08f : .08f);
        p.r[SB_LEG_L + side] = euler(knee[side], 0, 0);
        p.r[SB_FOOT_L + side] = euler(-(thigh[side] + knee[side]), 0, 0);
        drop = std::max(drop, 5.6f * (1.0f - std::cos(thigh[side])) + 5.0f * (1.0f - std::cos(thigh[side] + knee[side])));
    }
    p.hipsY = 12.5f - drop;
}

AnimClip siteClip(const std::vector<ModelNode>& nodes, const char* name, float duration, const std::function<void(float, SitePose&)>& fn) {
    AnimClip clip;
    clip.name = name;
    clip.duration = duration;
    const int keys = std::max(2, static_cast<int>(duration * 30.0f) + 1);
    std::vector<float> times(static_cast<size_t>(keys));
    std::vector<SitePose> poses(static_cast<size_t>(keys));
    for (int k = 0; k < keys; ++k) {
        times[static_cast<size_t>(k)] = duration * k / (keys - 1);
        fn(times[static_cast<size_t>(k)], poses[static_cast<size_t>(k)]);
    }
    auto nodeOf = [&](const char* n) {
        for (size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].name == n) return static_cast<int>(i);
        return -1;
    };
    for (int b = 0; b < SB_COUNT; ++b) {
        const int n = nodeOf(kSiteBones[b]);
        if (n < 0) continue;
        AnimChannel c;
        c.node = n;
        c.path = 1;
        c.times = times;
        for (const SitePose& p : poses) c.values.insert(c.values.end(), {p.r[b].x, p.r[b].y, p.r[b].z, p.r[b].w});
        clip.channels.push_back(std::move(c));
    }
    const int hips = nodeOf("Hips");
    if (hips >= 0) {
        AnimChannel c;
        c.node = hips;
        c.path = 0;
        c.times = times;
        for (const SitePose& p : poses) c.values.insert(c.values.end(), {0.0f, p.hipsY * kPx, 0.0f});
        clip.channels.push_back(std::move(c));
    }
    return clip;
}

void putClip(std::vector<AnimClip>& clips, AnimClip clip) {
    auto same = std::find_if(clips.begin(), clips.end(), [&](const AnimClip& a) { return a.name == clip.name; });
    if (same != clips.end()) *same = std::move(clip);
    else clips.push_back(std::move(clip));
}

}  // namespace

void addHeroClips(const std::vector<ModelNode>& nodes, std::vector<AnimClip>& clips) {
    // "Arma pronta" da página: tronco girando de leve, cabeça para o alvo, pernas afastadas e joelhos dobrados.
    putClip(clips, siteClip(nodes, "idle", kTwoPi / 2.2f, [](float t, SitePose& p) {
        const float br = std::sin(t * 2.2f) * .025f;
        p.r[SB_SPINE] = euler(0, -.06f, 0);
        p.r[SB_SPINE1] = euler(br, -.08f, 0);
        p.r[SB_SPINE2] = euler(br * .5f, -.08f, 0);
        p.r[SB_NECK] = euler(0, .1f, 0);
        p.r[SB_HEAD] = euler(.05f, .1f, 0);
        p.r[SB_ULEG_L] = euler(-.04f, 0, .07f);
        p.r[SB_ULEG_R] = euler(.04f, 0, -.07f);
        p.r[SB_LEG_L] = euler(.08f, 0, 0);
        p.r[SB_LEG_R] = euler(.1f, 0, 0);
        p.hipsY = 12.5f - .1f;
    }));
    // "Andando": passada com joelhos dobrando e o tronco acompanhando.
    putClip(clips, siteClip(nodes, "walk", 1.0f / .95f, [](float t, SitePose& p) {
        const float ph = siteLegs(p, t, .95f, .5f, .9f, .35f);
        siteArms(p, ph, .4f, .25f);
        p.r[SB_SPINE] = euler(0, std::sin(ph) * .06f, 0);
        p.r[SB_SPINE1] = euler(.03f, -.06f, 0);
        p.r[SB_SPINE2] = euler(0, -.06f - std::sin(ph) * .04f, 0);
        p.r[SB_HEAD] = euler(.05f, .1f, 0);
    }));
    // "Correndo": passadas longas, tronco inclinado e quique.
    putClip(clips, siteClip(nodes, "sprint", 1.0f / 1.45f, [](float t, SitePose& p) {
        const float ph = siteLegs(p, t, 1.45f, .85f, 1.45f, .7f);
        siteArms(p, ph, 1.0f, 1.25f);
        p.r[SB_SPINE] = euler(.12f, std::sin(ph) * .1f, 0);
        p.r[SB_SPINE1] = euler(.1f, -.05f, 0);
        p.r[SB_SPINE2] = euler(.06f, -.05f - std::sin(ph) * .06f, 0);
        p.r[SB_NECK] = euler(-.1f, .05f, 0);
        p.r[SB_HEAD] = euler(-.12f, .05f, 0);
    }));
    // Agachar parado: joelhos bem dobrados, tronco inclinado e cabeça olhando adiante.
    putClip(clips, siteClip(nodes, "crouch-idle", kTwoPi / 2.0f, [](float t, SitePose& p) {
        const float br = std::sin(t * 2.0f) * .02f;
        crouchLegs(p, -1.0f, -.85f, 1.6f, 1.45f);
        p.r[SB_SPINE] = euler(.32f + br, -.06f, 0);
        p.r[SB_SPINE1] = euler(.1f, -.06f, 0);
        p.r[SB_SPINE2] = euler(.04f, -.06f, 0);
        p.r[SB_NECK] = euler(-.2f, .08f, 0);
        p.r[SB_HEAD] = euler(-.22f, .08f, 0);
    }));
    // Andar agachado (para a frente e para trás): passos curtos sem levantar o corpo.
    for (int dir : {1, -1}) {
        putClip(clips, siteClip(nodes, dir > 0 ? "crouch-walk" : "crouch-back", 1.1f, [dir](float t, SitePose& p) {
            const float ph = t / 1.1f * kTwoPi;
            const float s = std::sin(ph) * dir;
            const float liftL = std::max(0.0f, std::sin(ph + 1.25f)), liftR = std::max(0.0f, std::sin(ph + kPi + 1.25f));
            crouchLegs(p, -.85f - .32f * s, -.85f + .32f * s, 1.3f + .35f * liftL, 1.3f + .35f * liftR);
            p.hipsY += .15f * std::fabs(std::cos(ph));
            p.r[SB_SPINE] = euler(.3f, std::sin(ph) * .05f, 0);
            p.r[SB_SPINE1] = euler(.08f, -.05f, 0);
            p.r[SB_NECK] = euler(-.18f, .06f, 0);
            p.r[SB_HEAD] = euler(-.2f, .06f, 0);
        }));
    }
}

void addMixamoClips(const std::vector<ModelNode>& nodes, std::vector<AnimClip>& clips) {
    // Clipe convertido -> nome usado pelo jogo (cada tipo de personagem recebe os seus).
    static const std::pair<const char*, const char*> kHero[] = {{"hero-aim-idle", "aim-idle"}, {"die", "die"}, {"dance", "dance"}, {"swim", "swim"}};
    auto nodeOf = [&](const std::string& name) {
        for (size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].name == name) return static_cast<int>(i);
        return -1;
    };
    const int hips = nodeOf("Hips");
    for (const ZzaClip& z : mixamoLibrary()) {
        const char* gameName = nullptr;
        
            for (const auto& m : kHero) if (z.id == m.first) gameName = m.second;
        
        if (!gameName) continue;
        AnimClip clip;
        clip.name = gameName;
        clip.duration = (z.frames - 1) / z.fps;
        std::vector<float> times(static_cast<size_t>(z.frames));
        for (int f = 0; f < z.frames; ++f) times[static_cast<size_t>(f)] = f / z.fps;
        for (size_t b = 0; b < z.bones.size(); ++b) {
            const int n = nodeOf(z.bones[b]);
            if (n < 0) continue;
            AnimChannel c;
            c.node = n;
            c.path = 1;
            c.times = times;
            c.values.assign(z.rot.begin() + static_cast<long>(b * z.frames * 4), z.rot.begin() + static_cast<long>((b + 1) * z.frames * 4));
            clip.channels.push_back(std::move(c));
        }
        if (hips >= 0) {
            AnimChannel c;
            c.node = hips;
            c.path = 0;
            c.times = times;
            c.values = z.hips;
            clip.channels.push_back(std::move(c));
        }
        auto same = std::find_if(clips.begin(), clips.end(), [&](const AnimClip& a) { return a.name == clip.name; });
        if (same != clips.end()) *same = std::move(clip);
        else clips.push_back(std::move(clip));
    }
}

void addHumanoidAliases(CharacterModel& model) {
    static const std::pair<const char*, const char*> kAliases[] = {
        {"torso", "Spine"}, {"head", "Head"}, {"arm-right", "RightArm"}, {"arm-left", "LeftArm"}, {"leg-right", "RightUpLeg"},
        {"leg-left", "LeftUpLeg"}, {"hand-right", "RightHand"}, {"hand-left", "LeftHand"},
    };
    for (const auto& a : kAliases) {
        const int n = model.nodeIndex(a.second);
        if (n >= 0) model.addAlias(a.first, n);
    }
}

}  // namespace zz
