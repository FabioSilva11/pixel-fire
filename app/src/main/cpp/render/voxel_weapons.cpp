#include "voxel_weapons.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>

#include "character.h"
#include "renderer.h"

namespace zz {

namespace {

// ----------------------------------------------------------------------------- texturas

// Atlas 256x256 com 4x4 materiais de 64x64 texels: cada "pixel" da textura voxel ocupa 4x4 texels e cada
// face mostra a grade inteira de 16x16 (como o canvas 32x32 em blocos de 2 do visualizador). Os materiais
// ficam alinhados aos mipmaps, então de longe cada um vira a sua cor média sem se misturar com o vizinho.
constexpr int kAtlas = 256;
constexpr int kTile = 64;
constexpr int kCell = 4;

enum Mat {
    WOOD, WOOD_LIGHT, WOOD_DARK, METAL, METAL_LIGHT, METAL_DARK, RIB, BLACK,
    EDGE, OLIVE, STEEL, SMOKE_PAINT, GLASS, RAG, RUBBER, BRASS, MAT_COUNT
};

struct MatPalette {
    uint32_t colors[4];
    int seed;
    bool wood;  // veios: linhas mais escuras
};

// As oito primeiras paletas são as do visualizador (wood, woodLight, woodDark, metal, metalLight, metalDark, rib,
// black); as demais seguem o mesmo esquema para granadas, faca e molotov.
const MatPalette kPalettes[MAT_COUNT] = {
    {{0x834018, 0xa95823, 0xb9662d, 0xd57934}, 2, true},
    {{0x984717, 0xc16a27, 0xcc772b, 0xe68b3d}, 3, true},
    {{0x693417, 0x85421b, 0x95501d, 0xb5632a}, 4, true},
    {{0x272b32, 0x343a43, 0x404751, 0x4d535d}, 5, false},
    {{0x39404a, 0x464e58, 0x545d67, 0x69737c}, 6, false},
    {{0x191d23, 0x232831, 0x2b3139, 0x333a43}, 7, false},
    {{0x474d56, 0x575e67, 0x656d77, 0x7b8289}, 8, false},
    {{0x111417, 0x111417, 0x111417, 0x111417}, 0, false},
    {{0x59616c, 0x59616c, 0x59616c, 0x59616c}, 0, false},
    {{0x3b4824, 0x4a5a2d, 0x566836, 0x6a7d44}, 9, false},
    {{0x8e98a3, 0xa3adb7, 0xb8c1c9, 0xd3dae0}, 10, false},
    {{0x6f7a80, 0x829097, 0x95a2a8, 0xaebbc0}, 11, false},
    {{0x1f5e2e, 0x2a7a3c, 0x35924a, 0x4fb062}, 12, false},
    {{0xb08a5a, 0xc79d68, 0xd8b07a, 0xe9c995}, 13, true},
    {{0x1b1d20, 0x24272b, 0x2d3136, 0x383d43}, 14, false},
    {{0x9c7a2c, 0xb48d34, 0xc9a040, 0xe0b955}, 15, false},
};

double seeded(double x, double y, double s) {
    const double z = std::sin(x * 127.1 + y * 311.7 + s * 74.7) * 43758.5453;
    return z - std::floor(z);
}

void paintAtlas(Image& img) {
    img.width = img.height = kAtlas;
    img.rgba.assign(static_cast<size_t>(kAtlas) * kAtlas * 4, 255);
    for (int m = 0; m < MAT_COUNT; ++m) {
        const MatPalette& p = kPalettes[m];
        const int ox = (m % 4) * kTile, oy = (m / 4) * kTile;
        for (int j = 0; j < 16; ++j) {
            for (int i = 0; i < 16; ++i) {
                const int x = i * 2, y = j * 2;
                const double v = seeded(x, y, p.seed);
                int ci = v > .94 ? 3 : v > .73 ? 2 : v > .23 ? 1 : 0;
                if (p.wood && (y == 6 || y == 18 || y == 28) && v > .3) ci = std::max(0, ci - 1);
                const uint32_t c = p.colors[ci];
                for (int dy = 0; dy < kCell; ++dy) {
                    for (int dx = 0; dx < kCell; ++dx) {
                        const size_t at = (static_cast<size_t>(oy + j * kCell + dy) * kAtlas + ox + i * kCell + dx) * 4;
                        img.rgba[at] = static_cast<uint8_t>(c >> 16);
                        img.rgba[at + 1] = static_cast<uint8_t>(c >> 8);
                        img.rgba[at + 2] = static_cast<uint8_t>(c);
                    }
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------- malha

using Pt = std::array<float, 2>;

float cross2(const Pt& a, const Pt& b, const Pt& c) {
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

float signedArea(const std::vector<Pt>& pts) {
    float area = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        const Pt& a = pts[i];
        const Pt& b = pts[(i + 1) % pts.size()];
        area += a[0] * b[1] - b[0] * a[1];
    }
    return area * 0.5f;
}

// Triângulos (anti-horários) de um polígono simples por corte de orelhas.
std::vector<int> triangulate(const std::vector<Pt>& pts) {
    std::vector<int> idx(pts.size());
    std::iota(idx.begin(), idx.end(), 0);
    if (signedArea(pts) < 0) std::reverse(idx.begin(), idx.end());
    std::vector<int> out;
    while (idx.size() > 3) {
        bool clipped = false;
        const size_t n = idx.size();
        for (size_t i = 0; i < n && !clipped; ++i) {
            const int a = idx[(i + n - 1) % n], b = idx[i], c = idx[(i + 1) % n];
            if (cross2(pts[a], pts[b], pts[c]) <= 1e-7f) continue;  // vértice côncavo
            bool ear = true;
            for (int q : idx) {
                if (q == a || q == b || q == c) continue;
                if (cross2(pts[a], pts[b], pts[q]) >= 0 && cross2(pts[b], pts[c], pts[q]) >= 0 && cross2(pts[c], pts[a], pts[q]) >= 0) {
                    ear = false;
                    break;
                }
            }
            if (!ear) continue;
            out.insert(out.end(), {a, b, c});
            idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
        }
        if (!clipped) break;  // polígono degenerado: o que sobrou fica sem tampa
    }
    if (idx.size() == 3) out.insert(out.end(), {idx[0], idx[1], idx[2]});
    return out;
}

// Monta a malha no espaço do desenho (X = cano, Y = cima, Z = lado, mesmas unidades do visualizador) e converte
// para o espaço da mão: z = X, y = Y, x = -Z (rotação, mantém o sentido das faces), escala [s] e o ponto
// [anchor] do desenho levado a [place].
struct VoxelBuilder {
    ModelMesh mesh;
    float s;
    Vec3 anchor;
    Vec3 place;

    Vec3 toHand(const Vec3& p) const {
        const Vec3 d = p - anchor;
        return place + Vec3(-d.z, d.y, d.x) * s;
    }

    static Vec3 toHandDir(const Vec3& n) { return Vec3(-n.z, n.y, n.x); }

    uint16_t next() const { return static_cast<uint16_t>(mesh.vertices.size() / 8); }

    void vertex(const Vec3& p, const Vec3& n, Mat m, float u, float v) {
        const float u0 = static_cast<float>((m % 4) * kTile), v0 = static_cast<float>((m / 4) * kTile);
        const Vec3 q = toHand(p), nn = toHandDir(n);
        mesh.vertices.insert(mesh.vertices.end(), {q.x, q.y, q.z, nn.x, nn.y, nn.z,
                                                   (u0 + 0.5f + u * (kTile - 1)) / kAtlas, (v0 + 0.5f + v * (kTile - 1)) / kAtlas});
    }

    void quad(const Vec3 c[4], const Vec3& n, Mat m) {
        const uint16_t b = next();
        vertex(c[0], n, m, 0, 1);
        vertex(c[1], n, m, 1, 1);
        vertex(c[2], n, m, 1, 0);
        vertex(c[3], n, m, 0, 0);
        mesh.indices.insert(mesh.indices.end(), {b, static_cast<uint16_t>(b + 1), static_cast<uint16_t>(b + 2), b,
                                                 static_cast<uint16_t>(b + 2), static_cast<uint16_t>(b + 3)});
    }

    // cube(grupo, centro, dimensões, material, id, rotação em Z) do visualizador: cada face mostra o material inteiro.
    void cube(const Vec3& c, const Vec3& d, Mat m, float rotZ = 0) {
        const Vec3 h = d * 0.5f;
        const float cs = std::cos(rotZ), sn = std::sin(rotZ);
        auto P = [&](float x, float y, float z) { return Vec3(c.x + x * cs - y * sn, c.y + x * sn + y * cs, c.z + z); };
        auto N = [&](float x, float y, float z) { return Vec3(x * cs - y * sn, x * sn + y * cs, z); };
        const Vec3 front[4] = {P(-h.x, -h.y, h.z), P(h.x, -h.y, h.z), P(h.x, h.y, h.z), P(-h.x, h.y, h.z)};
        const Vec3 back[4] = {P(h.x, -h.y, -h.z), P(-h.x, -h.y, -h.z), P(-h.x, h.y, -h.z), P(h.x, h.y, -h.z)};
        const Vec3 right[4] = {P(h.x, -h.y, h.z), P(h.x, -h.y, -h.z), P(h.x, h.y, -h.z), P(h.x, h.y, h.z)};
        const Vec3 left[4] = {P(-h.x, -h.y, -h.z), P(-h.x, -h.y, h.z), P(-h.x, h.y, h.z), P(-h.x, h.y, -h.z)};
        const Vec3 top[4] = {P(-h.x, h.y, h.z), P(h.x, h.y, h.z), P(h.x, h.y, -h.z), P(-h.x, h.y, -h.z)};
        const Vec3 bottom[4] = {P(-h.x, -h.y, -h.z), P(h.x, -h.y, -h.z), P(h.x, -h.y, h.z), P(-h.x, -h.y, h.z)};
        quad(front, N(0, 0, 1), m);
        quad(back, N(0, 0, -1), m);
        quad(right, N(1, 0, 0), m);
        quad(left, N(-1, 0, 0), m);
        quad(top, N(0, 1, 0), m);
        quad(bottom, N(0, -1, 0), m);
    }

    // Face plana do polígono no plano z (a textura cobre o retângulo que envolve o polígono).
    void facade(const std::vector<Pt>& pts, float z, Mat m, bool facingPlus) {
        float minX = pts[0][0], maxX = minX, minY = pts[0][1], maxY = minY;
        for (const Pt& p : pts) {
            minX = std::min(minX, p[0]);
            maxX = std::max(maxX, p[0]);
            minY = std::min(minY, p[1]);
            maxY = std::max(maxY, p[1]);
        }
        const float w = std::max(maxX - minX, 1e-4f), h = std::max(maxY - minY, 1e-4f);
        const uint16_t b = next();
        const Vec3 n(0, 0, facingPlus ? 1.0f : -1.0f);
        for (const Pt& p : pts) vertex({p[0], p[1], z}, n, m, (p[0] - minX) / w, 1.0f - (p[1] - minY) / h);
        const std::vector<int> tris = triangulate(pts);
        for (size_t i = 0; i + 2 < tris.size(); i += 3) {
            const int a = tris[i], c = tris[i + 2];
            const int mid = tris[i + 1];
            if (facingPlus) mesh.indices.insert(mesh.indices.end(), {static_cast<uint16_t>(b + a), static_cast<uint16_t>(b + mid), static_cast<uint16_t>(b + c)});
            else mesh.indices.insert(mesh.indices.end(), {static_cast<uint16_t>(b + a), static_cast<uint16_t>(b + c), static_cast<uint16_t>(b + mid)});
        }
    }

    void bothFacades(const std::vector<Pt>& pts, float z, Mat m) {
        facade(pts, z, m, true);
        facade(pts, -z, m, false);
    }

    // shape(grupo, pontos, espessura, material, id, deslocamento): perfil extrudado em Z, centrado na espessura.
    void shape(const std::vector<Pt>& pts, float depth, Mat m, float zOffset = 0) {
        const float z1 = zOffset + depth * 0.5f, z0 = zOffset - depth * 0.5f;
        facade(pts, z1, m, true);
        facade(pts, z0, m, false);
        const bool ccw = signedArea(pts) > 0;
        for (size_t i = 0; i < pts.size(); ++i) {
            const Pt& a = pts[i];
            const Pt& b = pts[(i + 1) % pts.size()];
            const float dx = b[0] - a[0], dy = b[1] - a[1];
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-5f) continue;
            const Vec3 n = ccw ? Vec3(dy / len, -dx / len, 0) : Vec3(-dy / len, dx / len, 0);
            const Vec3 side[4] = {{a[0], a[1], z1}, {b[0], b[1], z1}, {b[0], b[1], z0}, {a[0], a[1], z0}};
            quad(side, n, m);
        }
    }
};

// ----------------------------------------------------------------------------- modelos

// AK-47: as mesmas peças e medidas do visualizador "AK-47 Voxel" (coronha, corpo, empunhadura, carregador curvo,
// guarda-mão e cano), com a empunhadura na mão.
const Vec3 kAkAnchor{-1.33f, -0.28f, 0.0f};
const Vec3 kGunPlace{0.0f, -0.14f, 0.18f};
constexpr float kGunScale = 0.149f;  // mesma proporção da AK na página de teste (1,75 px por unidade)

void buildAk(VoxelBuilder& b) {
    // 1. Coronha de madeira em formato poligonal / blocado.
    b.shape({{-4.30f, 1.03f}, {-3.96f, 1.19f}, {-1.64f, 1.08f}, {-1.48f, .88f}, {-1.5f, .39f}, {-3.98f, .16f}, {-4.30f, .23f}}, .54f, WOOD);
    b.shape({{-4.32f, 1.04f}, {-4.15f, 1.06f}, {-4.15f, .23f}, {-4.32f, .25f}}, .61f, METAL_DARK);
    b.cube({-1.54f, .74f, 0}, {.19f, .72f, .66f}, WOOD_DARK);
    const float scratches[5][4] = {{-3.95f, .83f, .32f, .13f}, {-3.46f, .49f, .35f, .12f}, {-2.84f, .89f, .32f, .1f}, {-2.41f, .45f, .38f, .14f}, {-1.92f, .77f, .36f, .09f}};
    for (float sign : {-1.0f, 1.0f}) {
        for (int i = 0; i < 5; ++i) b.cube({scratches[i][0], scratches[i][1], sign * .275f}, {scratches[i][2], scratches[i][3], .014f}, i % 2 == 0 ? WOOD_LIGHT : WOOD_DARK);
    }

    // 2. Corpo metálico e tampa superior.
    b.cube({-.31f, .71f, 0}, {2.39f, .89f, .68f}, METAL);
    b.cube({-.31f, 1.19f, 0}, {2.46f, .17f, .74f}, METAL_DARK);
    b.cube({-.5f, 1.30f, 0}, {1.75f, .055f, .52f}, METAL_LIGHT);
    b.cube({-1.48f, .67f, 0}, {.18f, .92f, .79f}, METAL_DARK);
    b.cube({.88f, .58f, 0}, {.15f, .72f, .76f}, METAL_DARK);
    b.cube({-.27f, .30f, 0}, {2.25f, .11f, .66f}, METAL_DARK);
    for (float sign : {-1.0f, 1.0f}) {
        const float z = sign * .352f;
        b.cube({-.56f, 1.01f, z}, {1.54f, .095f, .025f}, METAL_LIGHT);
        b.cube({.40f, .97f, z}, {.69f, .28f, .029f}, METAL_DARK);
        b.cube({.39f, .97f, z + sign * .02f}, {.52f, .11f, .03f}, METAL_LIGHT);
        b.cube({-.76f, .77f, z}, {.78f, .085f, .035f}, METAL_LIGHT, .10f);
        b.cube({-1.07f, .85f, z}, {.12f, .12f, .045f}, EDGE);
        b.cube({-.15f, .55f, z}, {.45f, .07f, .04f}, METAL_DARK);
        b.cube({.57f, .54f, z}, {.29f, .08f, .037f}, METAL_LIGHT);
    }
    // Alça de mira traseira e suas pequenas torres.
    b.cube({.65f, 1.36f, 0}, {.36f, .17f, .47f}, METAL_DARK);
    b.cube({.64f, 1.47f, -.20f}, {.1f, .18f, .12f}, METAL);
    b.cube({.64f, 1.47f, .20f}, {.1f, .18f, .12f}, METAL);
    b.cube({.63f, 1.42f, 0}, {.11f, .09f, .06f}, BLACK);
    // Guarda-mato: barras com espaço interno visível.
    b.cube({-.82f, .15f, 0}, {.65f, .08f, .13f}, METAL_DARK);
    b.cube({-.49f, .08f, 0}, {.08f, .24f, .13f}, METAL_DARK);
    b.cube({-1.13f, .10f, 0}, {.08f, .17f, .13f}, METAL_DARK);
    b.cube({-.89f, .06f, 0}, {.075f, .21f, .10f}, EDGE, -.32f);

    // 3. Empunhadura de madeira, inclinação voxel.
    b.shape({{-1.49f, .36f}, {-1.00f, .36f}, {-.97f, .08f}, {-1.26f, -.92f}, {-1.72f, -.88f}, {-1.57f, -.21f}}, .43f, WOOD_DARK);
    for (float sign : {-1.0f, 1.0f}) {
        for (int i = 0; i < 4; ++i) b.cube({-1.39f + i * .025f, -.18f - i * .16f, sign * .225f}, {.27f, .045f, .017f}, i % 2 ? WOOD : WOOD_LIGHT, -.27f);
    }

    // 4. Carregador curvo construído por lados trapezoidais sucessivos.
    struct Row { float y, l, r; };
    const Row rows[] = {{.22f, -.27f, .28f}, {.04f, -.24f, .30f}, {-.13f, -.18f, .34f}, {-.30f, -.11f, .39f}, {-.47f, -.03f, .45f},
                        {-.64f, .08f, .53f}, {-.81f, .20f, .64f}, {-.98f, .34f, .77f}, {-1.15f, .49f, .91f}, {-1.29f, .61f, .99f}};
    constexpr int kRows = sizeof(rows) / sizeof(rows[0]);
    std::vector<Pt> outline;
    for (const Row& r : rows) outline.push_back({r.l, r.y});
    for (int i = kRows - 1; i >= 0; --i) outline.push_back({rows[i].r, rows[i].y});
    b.shape(outline, .48f, METAL_DARK);
    b.cube({0, .25f, 0}, {.64f, .11f, .55f}, METAL_LIGHT);
    auto lerp = [](float u, float v, float f) { return u + (v - u) * f; };
    for (int i = 0; i < kRows - 1; ++i) {
        const Row& a = rows[i];
        const Row& c = rows[i + 1];
        const float t = .32f;
        b.bothFacades({{a.l + .064f, a.y - .016f}, {a.r - .057f, a.y - .016f}, {lerp(a.r, c.r, t) - .057f, lerp(a.y, c.y, t)}, {lerp(a.l, c.l, t) + .064f, lerp(a.y, c.y, t)}},
                      .249f, i % 2 == 0 ? RIB : METAL_LIGHT);
    }
    b.shape({{.59f, -1.28f}, {.99f, -1.28f}, {1.025f, -1.34f}, {.67f, -1.38f}}, .54f, METAL_LIGHT);

    // 5. Guarda-mão alaranjado e entradas de ventilação quadradas.
    b.shape({{.88f, 1.07f}, {2.17f, 1.07f}, {2.36f, .91f}, {2.36f, .47f}, {2.22f, .37f}, {.89f, .39f}}, .56f, WOOD_LIGHT);
    b.cube({.90f, .74f, 0}, {.14f, .79f, .69f}, METAL_DARK);
    b.cube({2.32f, .72f, 0}, {.17f, .75f, .67f}, METAL_DARK);
    b.cube({1.61f, 1.11f, 0}, {1.43f, .10f, .58f}, WOOD);
    for (float sign : {-1.0f, 1.0f}) {
        const float z = sign * .291f;
        b.cube({1.23f, .95f, z}, {.26f, .09f, .022f}, BLACK);
        b.cube({1.79f, .95f, z}, {.32f, .09f, .022f}, BLACK);
        b.cube({1.56f, .59f, z}, {.94f, .09f, .025f}, WOOD);
        b.cube({2.02f, .47f, z}, {.43f, .085f, .024f}, WOOD_DARK);
    }

    // 6. Cano, tubo de gás, mira frontal e boca quadrada.
    b.cube({3.30f, .73f, 0}, {1.94f, .21f, .25f}, METAL_DARK);
    b.cube({2.84f, .99f, 0}, {1.05f, .18f, .22f}, METAL);
    b.cube({3.34f, .78f, 0}, {.86f, .17f, .32f}, METAL_LIGHT);
    b.cube({2.48f, .82f, 0}, {.15f, .38f, .42f}, METAL_DARK);
    b.cube({3.98f, .74f, 0}, {.17f, .34f, .35f}, METAL);
    b.cube({4.28f, .74f, 0}, {.65f, .31f, .33f}, METAL_DARK);
    b.cube({4.63f, .74f, 0}, {.09f, .31f, .37f}, METAL_LIGHT);
    b.cube({4.684f, .74f, 0}, {.009f, .155f, .17f}, BLACK);
    b.cube({3.91f, 1.12f, -.16f}, {.13f, .62f, .10f}, METAL_DARK);
    b.cube({3.91f, 1.12f, .16f}, {.13f, .62f, .10f}, METAL_DARK);
    b.cube({3.91f, 1.17f, 0}, {.11f, .40f, .07f}, METAL_LIGHT);
    b.cube({3.90f, 1.46f, 0}, {.12f, .07f, .40f}, METAL_DARK);
}

// Escopeta de bomba no mesmo estilo: coronha e empunhadura de madeira, caixa metálica, cano longo com o tubo
// do carregador embaixo e a telha de bombear com sulcos.
const Vec3 kShotgunAnchor{-1.45f, -0.20f, 0.0f};

void buildShotgun(VoxelBuilder& b) {
    b.shape({{-4.62f, 1.00f}, {-4.25f, 1.13f}, {-1.72f, .99f}, {-1.52f, .80f}, {-1.55f, .42f}, {-4.22f, .02f}, {-4.62f, .10f}}, .52f, WOOD);
    b.shape({{-4.74f, 1.02f}, {-4.58f, 1.04f}, {-4.58f, .08f}, {-4.74f, .10f}}, .58f, RUBBER);
    const float scratches[4][4] = {{-4.0f, .78f, .34f, .12f}, {-3.4f, .46f, .36f, .12f}, {-2.8f, .84f, .30f, .1f}, {-2.3f, .50f, .36f, .12f}};
    for (float sign : {-1.0f, 1.0f}) {
        for (int i = 0; i < 4; ++i) b.cube({scratches[i][0], scratches[i][1], sign * .265f}, {scratches[i][2], scratches[i][3], .014f}, i % 2 == 0 ? WOOD_LIGHT : WOOD_DARK);
    }
    b.shape({{-1.62f, .44f}, {-1.10f, .44f}, {-1.08f, .14f}, {-1.38f, -.86f}, {-1.84f, -.80f}, {-1.70f, -.15f}}, .42f, WOOD_DARK);
    for (float sign : {-1.0f, 1.0f}) {
        for (int i = 0; i < 4; ++i) b.cube({-1.50f + i * .025f, -.10f - i * .16f, sign * .22f}, {.27f, .045f, .017f}, i % 2 ? WOOD : WOOD_LIGHT, -.27f);
    }

    // Caixa da culatra.
    b.cube({-.32f, .74f, 0}, {2.42f, .66f, .62f}, METAL);
    b.cube({-.32f, 1.10f, 0}, {2.42f, .10f, .52f}, METAL_DARK);
    b.cube({-.30f, 1.17f, 0}, {1.60f, .05f, .18f}, METAL_LIGHT);
    b.cube({-1.50f, .72f, 0}, {.16f, .70f, .68f}, METAL_DARK);
    b.cube({.86f, .70f, 0}, {.14f, .62f, .66f}, METAL_DARK);
    b.cube({-.30f, .40f, 0}, {2.30f, .10f, .60f}, METAL_DARK);
    for (float sign : {-1.0f, 1.0f}) {
        const float z = sign * .318f;
        b.cube({-.15f, .82f, z}, {.80f, .24f, .02f}, BLACK);
        b.cube({-.95f, .62f, z}, {.60f, .07f, .03f}, METAL_LIGHT);
        b.cube({-1.15f, .86f, z}, {.10f, .10f, .04f}, EDGE);
    }
    b.cube({-.85f, .22f, 0}, {.65f, .08f, .13f}, METAL_DARK);
    b.cube({-.52f, .15f, 0}, {.08f, .22f, .13f}, METAL_DARK);
    b.cube({-1.16f, .17f, 0}, {.08f, .16f, .13f}, METAL_DARK);
    b.cube({-.92f, .13f, 0}, {.075f, .20f, .10f}, EDGE, -.32f);

    // Cano, tubo do carregador, abraçadeira, boca e massa de mira.
    b.cube({2.95f, .92f, 0}, {4.30f, .24f, .26f}, METAL_DARK);
    b.cube({2.35f, .60f, 0}, {3.10f, .24f, .26f}, METAL);
    b.cube({5.12f, .92f, 0}, {.14f, .30f, .32f}, METAL_LIGHT);
    b.cube({5.195f, .92f, 0}, {.01f, .14f, .15f}, BLACK);
    b.cube({3.95f, .60f, 0}, {.12f, .28f, .30f}, METAL_LIGHT);
    b.cube({3.70f, .76f, 0}, {.20f, .58f, .32f}, METAL_DARK);
    b.cube({4.98f, 1.08f, 0}, {.08f, .08f, .08f}, EDGE);

    // Telha de bombear com sulcos.
    b.shape({{1.05f, .82f}, {2.75f, .82f}, {2.90f, .70f}, {2.90f, .40f}, {2.75f, .30f}, {1.05f, .30f}, {.95f, .40f}, {.95f, .70f}}, .44f, WOOD_LIGHT);
    for (float sign : {-1.0f, 1.0f}) {
        for (int k = 0; k < 5; ++k) b.cube({1.25f + k * .34f, .56f, sign * .226f}, {.07f, .34f, .02f}, WOOD_DARK);
    }
}

// Pino com argola das granadas (latão), à esquerda do detonador.
void grenadePin(VoxelBuilder& b, float y) {
    b.cube({-.36f, y, 0}, {.14f, .06f, .06f}, BRASS);
    b.cube({-.62f, y + .12f, 0}, {.26f, .05f, .05f}, BRASS);
    b.cube({-.62f, y - .12f, 0}, {.26f, .05f, .05f}, BRASS);
    b.cube({-.75f, y, 0}, {.05f, .26f, .05f}, BRASS);
    b.cube({-.49f, y, 0}, {.05f, .26f, .05f}, BRASS);
}

// Granada de fragmentação "abacaxi": corpo arredondado em blocos, sulcos, detonador, alavanca e argola.
void buildGrenade(VoxelBuilder& b) {
    b.cube({0, 0, 0}, {1.30f, 1.50f, 1.30f}, OLIVE);
    b.cube({0, 0, 0}, {1.55f, 1.10f, 1.10f}, OLIVE);
    b.cube({0, 0, 0}, {1.10f, 1.10f, 1.55f}, OLIVE);
    b.cube({0, .80f, 0}, {.80f, .20f, .80f}, OLIVE);
    b.cube({0, -.80f, 0}, {.80f, .16f, .80f}, OLIVE);
    for (float y : {.28f, -.22f}) {
        b.cube({0, y, 0}, {1.57f, .06f, 1.12f}, METAL_DARK);
        b.cube({0, y, 0}, {1.12f, .06f, 1.57f}, METAL_DARK);
    }
    for (float t : {-.30f, .30f}) {
        for (float side : {-1.0f, 1.0f}) {
            b.cube({t, 0, side * .785f}, {.06f, 1.10f, .02f}, METAL_DARK);
            b.cube({side * .785f, 0, t}, {.02f, 1.10f, .06f}, METAL_DARK);
        }
    }
    b.cube({0, 1.02f, 0}, {.50f, .30f, .50f}, METAL);
    b.cube({0, 1.20f, 0}, {.36f, .08f, .36f}, METAL_LIGHT);
    b.cube({.42f, .98f, 0}, {.36f, .12f, .22f}, METAL_LIGHT);
    b.cube({.66f, .35f, 0}, {.10f, 1.20f, .22f}, METAL_LIGHT, .08f);
    grenadePin(b, 1.05f);
}

// Granada de fumaça: lata cinza arredondada, faixa escura, tampa, alavanca e argola.
void buildSmoke(VoxelBuilder& b) {
    b.cube({0, 0, 0}, {1.10f, 1.80f, 1.10f}, SMOKE_PAINT);
    b.cube({0, 0, 0}, {1.30f, 1.60f, .80f}, SMOKE_PAINT);
    b.cube({0, 0, 0}, {.80f, 1.60f, 1.30f}, SMOKE_PAINT);
    b.cube({0, .35f, 0}, {1.32f, .22f, .82f}, METAL_DARK);
    b.cube({0, .35f, 0}, {.82f, .22f, 1.32f}, METAL_DARK);
    b.cube({0, .35f, 0}, {1.12f, .22f, 1.12f}, METAL_DARK);
    b.cube({0, -.91f, 0}, {.70f, .04f, .70f}, BLACK);
    b.cube({0, 1.00f, 0}, {.60f, .22f, .60f}, METAL);
    b.cube({.35f, 1.06f, 0}, {.32f, .10f, .22f}, METAL_LIGHT);
    b.cube({.70f, .45f, 0}, {.10f, 1.10f, .22f}, METAL_LIGHT, .06f);
    grenadePin(b, 1.06f);
}

// Coquetel molotov: garrafa verde em blocos, rótulo e pano no gargalo.
void buildMolotov(VoxelBuilder& b) {
    b.cube({0, -.25f, 0}, {.95f, 1.25f, .95f}, GLASS);
    b.cube({0, -.25f, 0}, {1.10f, 1.05f, .75f}, GLASS);
    b.cube({0, -.25f, 0}, {.75f, 1.05f, 1.10f}, GLASS);
    b.cube({0, .45f, 0}, {.65f, .25f, .65f}, GLASS);
    b.cube({0, .80f, 0}, {.34f, .50f, .34f}, GLASS);
    b.cube({0, -.30f, .48f}, {.62f, .50f, .02f}, RAG);
    b.cube({0, 1.12f, 0}, {.42f, .20f, .42f}, RAG);
    b.cube({.12f, 1.32f, .05f}, {.22f, .30f, .18f}, RAG, .30f);
    b.cube({-.08f, 1.45f, 0}, {.16f, .22f, .14f}, RAG, -.40f);
}

// Faca de combate: lâmina de aço com ponta, lombada e sulco, guarda, cabo emborrachado com anéis e pomo.
const Vec3 kKnifeAnchor{-0.95f, 0.0f, 0.0f};

void buildKnife(VoxelBuilder& b) {
    b.shape({{0.0f, .24f}, {2.55f, .24f}, {3.30f, -.02f}, {2.75f, -.30f}, {0.0f, -.30f}}, .08f, STEEL);
    b.cube({1.27f, .255f, 0}, {2.55f, .05f, .10f}, METAL_LIGHT);
    b.cube({1.20f, .06f, .042f}, {1.90f, .08f, .006f}, METAL_LIGHT);
    b.cube({1.20f, .06f, -.042f}, {1.90f, .08f, .006f}, METAL_LIGHT);
    b.cube({-.08f, -.03f, 0}, {.16f, .86f, .30f}, METAL_DARK);
    b.cube({-.95f, -.03f, 0}, {1.58f, .40f, .26f}, RUBBER);
    for (int k = 0; k < 5; ++k) b.cube({-1.55f + k * .30f, -.03f, 0}, {.07f, .44f, .29f}, BLACK);
    b.cube({-1.82f, -.03f, 0}, {.18f, .48f, .32f}, METAL);
}

struct ItemSpec {
    float scale;
    Vec3 anchor;
    Vec3 place;
    Vec3 muzzle;  // no espaço do desenho
    Vec3 sight;
    Vec3 grip, foregrip, butt;  // contato da mão direita, da mão esquerda e do ombro
};

ItemSpec spec(VoxelItem item) {
    switch (item) {
        case VoxelItem::Pistol:return {.13f,{0,0,0},kGunPlace,{2.2f,.7f,0},{1.9f,1.1f,0},{-.3f,-.4f,0},{.4f,-.3f,0},{-1.1f,.4f,0}};
        case VoxelItem::Smg:return {.13f,{0,0,0},kGunPlace,{3.5f,.7f,0},{2.9f,1.2f,0},{-.8f,-.3f,0},{1.4f,.1f,0},{-2.7f,.5f,0}};
        case VoxelItem::Sniper:return {.13f,{0,0,0},kGunPlace,{6.8f,.6f,0},{.7f,1.8f,0},{-1.0f,-.3f,0},{2.1f,.1f,0},{-4.3f,.5f,0}};
        case VoxelItem::Rifle: return {kGunScale, kAkAnchor, kGunPlace, {4.69f, .74f, 0}, {3.90f, 1.50f, 0}, {-1.27f, -.12f, 0}, {1.55f, .40f, 0}, {-4.3f, .62f, 0}};
        case VoxelItem::Shotgun: return {kGunScale, kShotgunAnchor, kGunPlace, {5.20f, .92f, 0}, {4.98f, 1.12f, 0}, {-1.40f, -.05f, 0}, {1.9f, .30f, 0}, {-4.7f, .55f, 0}};
        case VoxelItem::Knife: return {0.13f, kKnifeAnchor, {0.0f, 0.0f, 0.08f}, {3.30f, 0, 0}, {3.30f, 0, 0}, {-.95f, 0, 0}, {-.95f, 0, 0}, {-1.8f, 0, 0}};
        default: return {0.10f, {0, 0, 0}, {0.0f, 0.0f, 0.02f}, {0, 1.2f, 0}, {0, 1.2f, 0}, {0, 0, 0}, {0, 0, 0}, {0, -1, 0}};
    }
}

Vec3 handPoint(VoxelItem item, const Vec3& p) {
    const ItemSpec s = spec(item);
    const Vec3 d = p - s.anchor;
    return s.place + Vec3(-d.z, d.y, d.x) * s.scale;
}

}  // namespace

void buildVoxelItem(VoxelItem item, CharacterAsset& out) {
    const ItemSpec s = spec(item);
    VoxelBuilder b{ModelMesh{}, s.scale, s.anchor, s.place};
    switch (item) {
        case VoxelItem::Pistol:
            b.cube({.4f,.7f,0},{3.6f,.65f,.65f},METAL_DARK);
            b.cube({-.4f,-.2f,0},{.8f,1.6f,.6f},RUBBER,-.15f);
            b.cube({.8f,-.1f,0},{1.3f,.15f,.25f},METAL);
            b.cube({1.9f,1.1f,0},{.2f,.2f,.25f},METAL_LIGHT);break;
        case VoxelItem::Smg:
            b.cube({.3f,.6f,0},{4.1f,.8f,.7f},METAL_DARK);
            b.cube({2.8f,.7f,0},{1.4f,.35f,.35f},BLACK);
            b.cube({-2.1f,.5f,0},{1.4f,.4f,.4f},METAL);
            b.cube({-.8f,-.3f,0},{.6f,1.3f,.6f},RUBBER);
            b.cube({.7f,-.6f,0},{.5f,2,.5f},METAL);
            b.cube({2.9f,1.2f,0},{.2f,.3f,.2f},METAL_LIGHT);break;
        case VoxelItem::Sniper:
            b.cube({1.7f,.6f,0},{5.6f,.7f,.6f},METAL_DARK);
            b.cube({5.2f,.6f,0},{3.2f,.3f,.3f},BLACK);
            b.cube({-2.5f,.4f,0},{3.6f,1,.7f},RUBBER);
            b.cube({-1,-.3f,0},{.7f,1.4f,.6f},RUBBER);
            b.cube({.7f,1.7f,0},{2.7f,.5f,.6f},METAL);
            b.cube({.4f,-.4f,0},{.8f,1,.5f},BLACK);break;
        case VoxelItem::Rifle: buildAk(b); break;
        case VoxelItem::Shotgun: buildShotgun(b); break;
        case VoxelItem::Grenade: buildGrenade(b); break;
        case VoxelItem::Smoke: buildSmoke(b); break;
        case VoxelItem::Molotov: buildMolotov(b); break;
        case VoxelItem::Knife: buildKnife(b); break;
        case VoxelItem::Count: break;
    }
    paintAtlas(out.image);
    ModelNode root;
    root.name = "voxel-item";
    root.mesh = 0;
    out.model.build({root}, {std::move(b.mesh)}, {});
    out.pixelArt = true;
    out.doubleSided = true;
    // Madeira e metal grafite foscos, como os materiais do visualizador.
}

Vec3 voxelMuzzle(VoxelItem item) { return handPoint(item, spec(item).muzzle); }

Vec3 voxelFrontSight(VoxelItem item) { return handPoint(item, spec(item).sight); }

Vec3 voxelGrip(VoxelItem item) { return handPoint(item, spec(item).grip); }

Vec3 voxelForegrip(VoxelItem item) { return handPoint(item, spec(item).foregrip); }

Vec3 voxelButt(VoxelItem item) { return handPoint(item, spec(item).butt); }

}  // namespace zz
