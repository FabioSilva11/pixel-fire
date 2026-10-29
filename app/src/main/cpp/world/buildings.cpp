// Construções procedurais dos pontos de interesse: casas com telhado, vilarejos, igreja com torre,
// posto de gasolina, prédio de três andares com escadas e celeiro. Todas têm portas abertas e janelas
// (vidro recortado); o interior tem móveis, lanternas e baús. Materiais mudam com o bioma.
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "blocks.h"
#include "structures.h"

namespace zz {

namespace {

struct Mats {
    uint8_t wall = B_PLASTER;
    uint8_t trim = B_LOG_OAK;
    uint8_t roof = B_ROOF_RED;
    uint8_t floor = B_PLANKS;
    uint8_t found = B_COBBLE;
    bool flatRoof = false;  // deserto: telhado plano com mureta
    bool snowy = false;     // neve por cima do telhado
    bool ruined = false;    // terra devastada: buracos nas paredes e no telhado
};

Mats materials(const Theme& t, Rng& rng) {
    Mats m;
    switch (t.id) {
        case 1:
            m.wall = rng.chance(0.5f) ? B_SANDSTONE : B_TERRACOTTA;
            m.trim = B_PLANKS;
            m.roof = B_SANDSTONE;
            m.found = B_SANDSTONE;
            m.flatRoof = true;
            break;
        case 2:
            m.wall = rng.chance(0.5f) ? B_PLANKS : B_STONE_BRICK;
            m.trim = B_LOG_SPRUCE;
            m.roof = B_ROOF_SLATE;
            m.snowy = true;
            break;
        case 3:
            m.wall = rng.chance(0.5f) ? B_STONE_BRICK : B_BRICK;
            m.trim = B_LOG_DEAD;
            m.roof = B_ROOF_SLATE;
            m.floor = rng.chance(0.5f) ? B_PLANKS : B_GRAVEL;
            m.ruined = true;
            break;
        default: {
            int r = rng.rangeInt(0, 2);
            m.wall = r == 0 ? B_PLASTER : (r == 1 ? B_BRICK : B_PLANKS);
            m.trim = B_LOG_OAK;
            m.roof = rng.chance(0.6f) ? B_ROOF_RED : B_ROOF_SLATE;
            break;
        }
    }
    return m;
}

// Sistema local de coordenadas girado em passos de 90° (lx, lz) -> mundo; ly = 0 é o piso onde se anda.
struct Frame {
    int cx = 0, cy = 0, cz = 0, rot = 0;
    void map(int lx, int lz, int& x, int& z) const {
        switch (rot & 3) {
            case 0: x = cx + lx; z = cz + lz; break;
            case 1: x = cx - lz; z = cz + lx; break;
            case 2: x = cx - lx; z = cz - lz; break;
            default: x = cx + lz; z = cz - lx; break;
        }
    }
    Frame sub(int lx, int lz, int r) const {
        Frame f = *this;
        map(lx, lz, f.cx, f.cz);
        f.rot = (rot + r) & 3;
        return f;
    }
};

struct Builder {
    BlockSink& w;
    Frame f;
    Rng& rng;
    DecorOut& out;

    void set(int lx, int ly, int lz, uint8_t b) {
        int x, z;
        f.map(lx, lz, x, z);
        w.set(x, f.cy + ly, z, b);
    }
    uint8_t get(int lx, int ly, int lz) const {
        int x, z;
        f.map(lx, lz, x, z);
        return w.get(x, f.cy + ly, z);
    }
    void fill(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t b) {
        for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y)
            for (int z = std::min(z0, z1); z <= std::max(z0, z1); ++z)
                for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x) set(x, y, z, b);
    }
    Vec3 pos(int lx, float ly, int lz) const {
        int x, z;
        f.map(lx, lz, x, z);
        return {x + 0.5f, static_cast<float>(f.cy) + ly, z + 0.5f};
    }
    void lantern(int lx, int ly, int lz) {
        set(lx, ly, lz, B_LANTERN);
        out.lights.push_back(pos(lx, ly + 0.5f, lz));
    }
    void chest(int lx, int ly, int lz) { out.chests.push_back(pos(lx, static_cast<float>(ly), lz)); }

    // Piso, fundação, paredes de altura h e interior vazio (remove plantas e terreno que sobrar).
    void room(int x0, int z0, int x1, int z1, int h, const Mats& m, uint8_t wall) {
        fill(x0, -3, z0, x1, -2, z1, m.found);
        fill(x0, -1, z0, x1, -1, z1, m.floor);
        for (int y = 0; y < h; ++y)
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x) {
                    bool edge = x == x0 || x == x1 || z == z0 || z == z1;
                    bool corner = (x == x0 || x == x1) && (z == z0 || z == z1);
                    set(x, y, z, edge ? (corner ? m.trim : wall) : B_AIR);
                }
    }

    // Janela de vidro (às vezes quebrada) em (lx, lz) da parede, com 2 de altura a partir de ly.
    void window(int lx, int ly, int lz, const Mats& m, int height = 2) {
        for (int k = 0; k < height; ++k) set(lx, ly + k, lz, rng.chance(m.ruined ? 0.45f : 0.12f) ? B_AIR : B_GLASS);
    }

    // Telhado de duas águas ao longo de X sobre [x0,x1]x[z0,z1]; o topo das paredes fica em ly = top - 1.
    // Cada degrau tem duas fileiras (fecha o vão sobre as paredes) e as pontas viram empenas.
    void gableRoof(int x0, int z0, int x1, int z1, int top, const Mats& m, uint8_t gableWall) {
        for (int k = 0;; ++k) {
            int za = z0 - 1 + k, zb = z1 + 1 - k;
            if (za > zb) break;
            int y = top + k;
            for (int z = za + 2; z <= zb - 2; ++z) {
                set(x0, y, z, gableWall);
                set(x1, y, z, gableWall);
            }
            for (int x = x0 - 1; x <= x1 + 1; ++x) {
                for (int z : {za, za + 1, zb - 1, zb}) {
                    if (z < za || z > zb) continue;
                    if (m.ruined && rng.chance(0.05f)) continue;
                    set(x, y, z, m.roof);
                    if (m.snowy && get(x, y + 1, z) == B_AIR) set(x, y + 1, z, B_SNOW);
                }
            }
        }
    }

    void flatRoof(int x0, int z0, int x1, int z1, int top, const Mats& m) {
        fill(x0, top, z0, x1, top, z1, m.roof);
        for (int x = x0; x <= x1; ++x) {
            set(x, top + 1, z0, m.wall);
            set(x, top + 1, z1, m.wall);
        }
        for (int z = z0; z <= z1; ++z) {
            set(x0, top + 1, z, m.wall);
            set(x1, top + 1, z, m.wall);
        }
    }

    void ruin(int x0, int z0, int x1, int z1, int h, const Mats& m) {
        if (!m.ruined) return;
        int holes = rng.rangeInt(2, 5);
        for (int i = 0; i < holes; ++i) {
            bool alongX = rng.chance(0.5f);
            int x = alongX ? rng.rangeInt(x0 + 1, x1 - 1) : (rng.chance(0.5f) ? x0 : x1);
            int z = alongX ? (rng.chance(0.5f) ? z0 : z1) : rng.rangeInt(z0 + 1, z1 - 1);
            int y = rng.rangeInt(1, std::max(1, h - 1));
            set(x, y, z, B_AIR);
            if (rng.chance(0.5f)) set(x, y + 1, z, B_AIR);
        }
    }

    // Carro abandonado (cobertura baixa): carroceria de 2x4 e para-brisa.
    void car(int lx, int lz, bool alongX) {
        for (int a = 0; a < 4; ++a)
            for (int b = 0; b < 2; ++b) {
                int x = lx + (alongX ? a : b), z = lz + (alongX ? b : a);
                set(x, 0, z, B_WOOL_RED);
                if (a == 1 || a == 2) set(x, 1, z, B_GLASS);
            }
    }
};

// Casa: paredes, portas largas, janelas, móveis, lanterna, baú e telhado.
void house(Builder& b, const Mats& m, int w2, int d2, bool furnish) {
    const int h = 4;
    b.room(-w2, -d2, w2, d2, h, m, m.wall);
    // Porta da frente (2 de largura) e porta dos fundos.
    b.fill(-1, 0, -d2, 0, 2, -d2, B_AIR);
    b.fill(w2 - 2, 0, d2, w2 - 2, 1, d2, B_AIR);
    // Janelas.
    for (int x : {-w2 + 2, w2 - 1}) {
        if (x == -1 || x == 0) continue;  // vão da porta
        b.window(x, 1, -d2, m);
    }
    b.window(-w2 + 2, 1, d2, m);
    b.window(-w2, 1, 0, m);
    b.window(w2, 1, 0, m);
    b.ruin(-w2, -d2, w2, d2, h, m);
    if (m.flatRoof) b.flatRoof(-w2, -d2, w2, d2, h, m);
    else b.gableRoof(-w2, -d2, w2, d2, h, m, m.wall);
    // Chaminé.
    if (!m.flatRoof && w2 >= 4) b.fill(w2 - 1, h, 1, w2 - 1, h + d2 + 2, 1, B_BRICK);
    if (!furnish) {
        b.lantern(0, h - 1, 0);
        return;
    }
    // Cama, estante, mesa e caixas.
    b.set(-w2 + 1, 0, d2 - 1, B_WOOL_RED);
    b.set(-w2 + 2, 0, d2 - 1, B_WOOL_RED);
    b.set(-w2 + 1, 0, d2 - 2, B_PLANKS);
    if (w2 >= 4) b.fill(0, 0, d2 - 1, 1, 1, d2 - 1, B_BOOKSHELF);
    b.set(1, 0, 0, B_PLANKS);
    b.set(w2 - 1, 0, -d2 + 1, B_CRATE);
    if (b.rng.chance(0.5f)) b.set(w2 - 1, 1, -d2 + 1, B_CRATE);
    b.lantern(0, h - 1, 0);
    b.chest(w2 - 1, 0, d2 - 1);
}

void village(Builder& b, const Theme& theme, const Mats& m) {
    // Poço no centro.
    b.fill(-1, -3, -1, 1, -1, 1, B_COBBLE);
    b.set(0, -1, 0, B_WATER);
    for (int z = -1; z <= 1; ++z)
        for (int x = -1; x <= 1; ++x)
            if (x != 0 || z != 0) b.set(x, 0, z, B_COBBLE);
    for (int x : {-1, 1})
        for (int z : {-1, 1}) b.fill(x, 1, z, x, 2, z, m.trim);
    b.fill(-1, 3, -1, 1, 3, 1, B_PLANKS);
    // Casinhas viradas para o poço.
    int count = b.rng.rangeInt(3, 4);
    float base = b.rng.range(0, kTwoPi);
    int chestHouse = b.rng.rangeInt(0, count - 1);
    for (int i = 0; i < count; ++i) {
        float ang = base + i * kTwoPi / count;
        int ox = static_cast<int>(std::lround(std::cos(ang) * 9.0f));
        int oz = static_cast<int>(std::lround(std::sin(ang) * 9.0f));
        // A frente (lz negativo) aponta para o centro.
        int rot;
        if (std::abs(ox) > std::abs(oz)) rot = ox > 0 ? 3 : 1;
        else rot = oz > 0 ? 0 : 2;
        Builder hb{b.w, b.f.sub(ox, oz, rot), b.rng, b.out};
        Mats hm = materials(theme, b.rng);
        hm.ruined = m.ruined;
        house(hb, hm, 3, 2, i == chestHouse);
    }
    // Postes de luz entre as casas.
    for (int i = 0; i < count; ++i) {
        float ang = base + (i + 0.5f) * kTwoPi / count;
        int x = static_cast<int>(std::lround(std::cos(ang) * 6.0f));
        int z = static_cast<int>(std::lround(std::sin(ang) * 6.0f));
        b.fill(x, 0, z, x, 1, z, m.trim);
        b.lantern(x, 2, z);
    }
}

void church(Builder& b, const Mats& base) {
    Mats m = base;
    if (m.wall != B_SANDSTONE) m.wall = B_STONE_BRICK;
    m.trim = m.wall == B_SANDSTONE ? B_SANDSTONE : B_STONE_BRICK;
    const int h = 6;
    b.room(-3, -6, 3, 6, h, m, m.wall);
    // Porta dupla em arco e vitrais altos.
    b.fill(-1, 0, -6, 1, 2, -6, B_AIR);
    b.set(0, 3, -6, B_AIR);
    for (int z : {-4, -1, 2, 5}) {
        b.window(-3, 1, z, m, 3);
        b.window(3, 1, z, m, 3);
    }
    // Tapete vermelho, bancos e altar.
    for (int z = -5; z <= 4; ++z) b.set(0, -1, z, B_WOOL_RED);
    for (int z : {-4, -2, 0, 2}) {
        b.fill(-2, 0, z, -1, 0, z, B_PLANKS);
        b.fill(1, 0, z, 2, 0, z, B_PLANKS);
    }
    b.fill(-1, 0, 5, 1, 0, 5, m.wall);
    b.lantern(-1, 1, 5);
    b.lantern(1, 1, 5);
    b.chest(0, 0, 4);
    b.ruin(-3, -6, 3, 6, h, m);
    // Telhado ao longo do comprimento (gira o sistema local para usar o mesmo telhado de duas águas).
    Builder rb{b.w, b.f.sub(0, 0, 1), b.rng, b.out};
    if (m.flatRoof) b.flatRoof(-3, -6, 3, 6, h, m);
    else rb.gableRoof(-6, -3, 6, 3, h, m, m.wall);
    // Torre do sino atrás da nave.
    const int th = 12;
    for (int y = -3; y < th; ++y)
        for (int z = 7; z <= 9; ++z)
            for (int x = -1; x <= 1; ++x) {
                bool shell = x != 0 || z != 8;
                if (y < 0) b.set(x, y, z, m.found);
                else b.set(x, y, z, shell ? m.wall : B_AIR);
            }
    // Arcos abertos no alto da torre com o sino (lanterna) no meio.
    for (int y = 8; y <= 9; ++y) {
        b.set(0, y, 7, B_AIR);
        b.set(0, y, 9, B_AIR);
        b.set(-1, y, 8, B_AIR);
        b.set(1, y, 8, B_AIR);
    }
    b.lantern(0, 9, 8);
    b.fill(-2, th, 6, 2, th, 10, m.roof == B_SANDSTONE ? B_SANDSTONE : B_ROOF_SLATE);
    b.fill(-1, th + 1, 7, 1, th + 1, 9, m.roof == B_SANDSTONE ? B_SANDSTONE : B_ROOF_SLATE);
    b.set(0, th + 2, 8, B_LOG_DEAD);
    b.set(0, th + 3, 8, B_LOG_DEAD);
}

void gasStation(Builder& b, const Mats& m) {
    // Pátio de asfalto.
    for (int z = -7; z <= 3; ++z)
        for (int x = -7; x <= 7; ++x) {
            b.fill(x, -2, z, x, -2, z, m.found);
            b.set(x, -1, z, B_ASPHALT);
            for (int y = 0; y < 5; ++y) b.set(x, y, z, B_AIR);
        }
    // Cobertura sobre as bombas.
    for (int x : {-4, 4})
        for (int z : {-4, 0}) b.fill(x, 0, z, x, 3, z, B_CONCRETE);
    for (int z = -5; z <= 1; ++z)
        for (int x = -5; x <= 5; ++x) {
            bool rim = x == -5 || x == 5 || z == -5 || z == 1;
            b.set(x, 4, z, rim ? B_WOOL_RED : B_CONCRETE);
        }
    b.lantern(-2, 3, -2);
    b.lantern(2, 3, -2);
    for (int x : {-2, 2}) b.fill(x, 0, -2, x, 1, -2, B_PUMP);
    // Lojinha de conveniência com vitrine.
    b.room(-4, 4, 4, 8, 4, m, m.flatRoof ? m.wall : B_CONCRETE);
    for (int x = -3; x <= 3; ++x) b.window(x, 1, 4, m);
    b.fill(0, 0, 4, 1, 2, 4, B_AIR);
    b.flatRoof(-4, 4, 4, 8, 4, m);
    b.fill(-3, 0, 7, -1, 1, 7, B_BOOKSHELF);
    b.set(3, 0, 7, B_CRATE);
    b.set(3, 0, 6, B_CRATE);
    b.lantern(0, 3, 6);
    b.chest(2, 0, 7);
    // Carros abandonados.
    b.car(-6, -6, false);
    if (b.rng.chance(0.6f)) b.car(5, -6, false);
}

void apartment(Builder& b, const Mats& base) {
    Mats m = base;
    if (m.wall == B_PLANKS) m.wall = B_BRICK;
    const int floors = 3, fh = 4;
    const int top = floors * fh - 1;  // laje do terraço
    b.fill(-4, -3, -4, 4, -2, 4, m.found);
    for (int y = -1; y <= top; ++y)
        for (int z = -4; z <= 4; ++z)
            for (int x = -4; x <= 4; ++x) {
                bool edge = x == -4 || x == 4 || z == -4 || z == 4;
                bool corner = (x == -4 || x == 4) && (z == -4 || z == 4);
                bool slab = (y + 1) % fh == 0;  // -1, 3, 7, 11
                uint8_t blk = B_AIR;
                if (edge) blk = corner ? B_CONCRETE : m.wall;
                else if (slab) blk = y == -1 ? m.floor : B_CONCRETE;
                b.set(x, y, z, blk);
            }
    // Mureta do terraço.
    for (int x = -4; x <= 4; ++x) {
        b.set(x, top + 1, -4, B_CONCRETE);
        b.set(x, top + 1, 4, B_CONCRETE);
    }
    for (int z = -4; z <= 4; ++z) {
        b.set(-4, top + 1, z, B_CONCRETE);
        b.set(4, top + 1, z, B_CONCRETE);
    }
    // Janelas em todos os andares.
    for (int f = 0; f < floors; ++f) {
        int y = f * fh + 1;
        for (int k : {-2, 2}) {
            b.window(k, y, -4, m);
            b.window(k, y, 4, m);
            b.window(-4, y, k, m);
            b.window(4, y, k, m);
        }
    }
    // Porta da frente.
    b.fill(2, 0, -4, 3, 2, -4, B_AIR);
    // Escadas alternando os lados (degraus de 1 bloco), com abertura na laje de cima.
    auto stairs = [&](int floorY, bool forward) {
        for (int k = 0; k < fh; ++k) {
            int x = forward ? -3 + k : 3 - k;
            int z0 = forward ? -3 : 2;
            for (int z = z0; z <= z0 + 1; ++z) {
                b.fill(x, floorY, z, x, floorY + k, z, B_CONCRETE);
                if (k < fh - 1) b.set(x, floorY + fh - 1, z, B_AIR);  // vão na laje para passar a cabeça
            }
        }
    };
    stairs(0, true);
    stairs(fh, false);
    stairs(fh * 2, true);
    // Móveis e luzes.
    b.set(3, 0, 3, B_CRATE);
    b.set(2, 0, 3, B_CRATE);
    b.set(-3, fh, 3, B_BOOKSHELF);
    b.set(-3, fh, -1, B_WOOL_RED);
    b.set(3, fh * 2, -3, B_CRATE);
    for (int f = 0; f < floors; ++f) {
        b.set(0, f * fh + fh - 1, 0, B_LANTERN);
        b.out.lights.push_back(b.pos(0, f * fh + fh - 1.5f, 0));
    }
    b.lantern(3, top + 1, 3);
    b.chest(-2, 0, 2);
    b.chest(0, top + 1, 0);  // no terraço: recompensa por subir
    if (m.ruined) b.ruin(-4, -4, 4, 4, top, m);
}

void barn(Builder& b, const Mats& base) {
    Mats m = base;
    uint8_t wall = m.flatRoof ? m.wall : B_WOOL_RED;
    const int h = 5;
    b.room(-5, -4, 5, 4, h, m, wall);
    b.fill(-1, 0, -4, 1, 3, -4, B_AIR);  // portão grande
    b.fill(-1, 0, 4, 0, 2, 4, B_AIR);
    b.window(-5, 2, 0, m);
    b.window(5, 2, 0, m);
    if (m.flatRoof) b.flatRoof(-5, -4, 5, 4, h, m);
    else b.gableRoof(-5, -4, 5, 4, h, m, wall);
    // Fardos de feno e um mezanino de tábuas.
    for (int x = -4; x <= -2; ++x)
        for (int z = 1; z <= 3; ++z) {
            b.set(x, 0, z, B_HAY);
            if (z > 1) b.set(x, 1, z, B_HAY);
        }
    // Degraus de feno até o mezanino.
    b.fill(2, 2, 2, 4, 2, 3, B_PLANKS);
    b.set(4, 0, 0, B_HAY);
    b.set(4, 0, 1, B_HAY);
    b.set(4, 1, 1, B_HAY);
    b.set(3, 0, -3, B_CRATE);
    b.lantern(0, h - 1, 0);
    b.chest(3, 3, 3);  // no mezanino
    b.chest(-4, 0, -3);
}

}  // namespace

void buildBuilding(BlockSink& w, const Camp& camp, const Theme& theme, Rng& rng, DecorOut& out) {
    Frame f;
    f.cx = static_cast<int>(std::floor(camp.center.x));
    f.cz = static_cast<int>(std::floor(camp.center.z));
    f.cy = static_cast<int>(camp.center.y);
    f.rot = rng.rangeInt(0, 3);
    Builder b{w, f, rng, out};
    Mats m = materials(theme, rng);
    switch (camp.kind) {
        case K_VILLAGE: village(b, theme, m); break;
        case K_CHURCH: church(b, m); break;
        case K_GAS: gasStation(b, m); break;
        case K_APARTMENT: apartment(b, m); break;
        case K_BARN: barn(b, m); break;
        default: house(b, m, 4, 3, true); break;
    }
}

}  // namespace zz
