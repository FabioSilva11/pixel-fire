// Estruturas procedurais (árvores, acampamentos, torre, ninho, portal) escritas num "BlockSink":
// o mundo finito (lobby), um chunk em geração ou um molde guardado da ilha em streaming.
#pragma once

#include <cstdint>
#include <vector>

#include "../core/math.h"
#include "../core/random.h"
#include "theme.h"

namespace zz {

class BlockSink {
public:
    virtual ~BlockSink() = default;
    virtual uint8_t get(int x, int y, int z) const = 0;
    virtual void set(int x, int y, int z, uint8_t block) = 0;
};

enum CampKind {
    K_FIRE = 0, K_RUINS = 1, K_GRAVEYARD = 2, K_DEPOT = 3, K_FARM = 4,
    K_TOWER = 5, K_OUTPOST = 6, K_NEST = 7, K_PORTAL = 8,
    // Construções (todas com portas abertas: dá para entrar, nunca são labirintos)
    K_HOUSE = 9, K_VILLAGE = 10, K_CHURCH = 11, K_GAS = 12, K_APARTMENT = 13, K_BARN = 14,
};

inline bool isBuildingKind(int kind) { return kind >= K_HOUSE && kind <= K_BARN; }
// Raio mínimo da área aplainada para a construção caber.
inline float buildingRadius(int kind) {
    switch (kind) {
        case K_VILLAGE: return 15.0f;
        case K_CHURCH: return 10.5f;
        case K_APARTMENT: return 9.5f;
        case K_GAS: return 10.0f;
        case K_BARN: return 9.0f;
        default: return 8.5f;
    }
}

struct Camp {
    Vec3 center;   // posição dos pés no centro do acampamento
    float radius;
    bool start;
    bool portal;
    int kind = 0;
};

// Saídas da decoração: luzes, fogueiras e baús (posições dos pés).
struct DecorOut {
    std::vector<Vec3> lights;
    std::vector<Vec3> campfires;
    std::vector<Vec3> chests;
};

void placeTree(BlockSink& w, TreeKind kind, int x, int y, int z, Rng& rng);
// Espécie da árvore numa posição: bosques coloridos (cerejeiras, ipês...) por região e bioma.
TreeKind pickTreeKind(const Theme& theme, int x, int z, uint32_t seed, bool shore);
// Construções (buildings.cpp): casa, vilarejo, igreja, posto, prédio e celeiro.
void buildBuilding(BlockSink& w, const Camp& camp, const Theme& theme, Rng& rng, DecorOut& out);
void lanternPost(BlockSink& w, int x, int y, int z, DecorOut& out);
void decorateCamp(BlockSink& w, const Camp& camp, const Theme& theme, Rng& rng, DecorOut& out);
// Moldura de obsidiana com a abertura voltada para `towards`.
void buildPortal(BlockSink& w, const Camp& camp, const Vec3& towards, Vec3& base, float& yaw, DecorOut& out);
// Cratera infectada com pedestal; `core` recebe o ponto onde flutua o núcleo do ninho.
void buildNest(BlockSink& w, const Camp& camp, Rng& rng, DecorOut& out, Vec3& core);

}  // namespace zz
