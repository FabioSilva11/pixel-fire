// Itens do inventário (estilo Minecraft): armas e consumíveis. Os ids espelham Items.kt.
#pragma once

#include <cstdint>

namespace zz {

enum ItemId : uint8_t {
    IT_NONE = 0,
    // Armas do template Multiplayer FPS (Unity): fuzil de assalto e escopeta. Os ids 2 e 4..8 eram as armas
    // antigas (pistola, submetralhadora, precisão, metralhadora, lança-chamas e lança-granadas), excluídas:
    // saves antigos com esses ids perdem o item ao carregar.
    IT_RIFLE = 1, IT_SHOTGUN = 3,
    IT_WEAPON_LAST = 15,
    // Consumíveis
    IT_MEDKIT = 20, IT_BANDAGE = 21, IT_ANTIDOTE = 22, IT_ADRENALINE = 23, IT_FIRE_AMMO = 24, IT_ARMOR = 25,
    IT_GRENADE = 26, IT_MOLOTOV = 27, IT_SMOKE = 28,
    IT_LAST = 28,
};

constexpr int kInvSlots = 24;    // 6 na barra rápida + 18 na mochila
constexpr int kHotbarSlots = 6;

inline bool isWeaponItem(int id) { return id == IT_RIFLE || id == IT_SHOTGUN; }
inline bool isThrowableItem(int id) { return id == IT_GRENADE || id == IT_MOLOTOV || id == IT_SMOKE; }
inline bool isValidItem(int id) { return isWeaponItem(id) || (id >= IT_MEDKIT && id <= IT_LAST); }

// Quantidade máxima por espaço (as granadas crescem com a melhoria de explosivos).
int itemMaxStack(int id, int explosivesLevel);

// WeaponType do template: Hitscan acerta na hora pelo raio da mira; Projectile solta uma bala que voa.
enum class FireKind { Hitscan, Projectile };

// ReticleType do template (a mira desenhada no HUD). Os ids espelham Reticle em WeaponSpecs.kt.
enum class ReticleType { Cross = 0, TCross = 1, OpenCircular = 2, CircularCross = 3 };

// ProjectileBehavior do template: dano só no alvo atingido ou em área no ponto de impacto.
enum class ProjectileBehavior { DirectDamage, AreaOfEffect };

// WeaponData do template, com o dano escalado para a vida dos zumbis (x1,8, mantendo a proporção entre as armas).
struct WeaponDef {
    FireKind kind;
    ReticleType reticle;
    float damage;           // por acerto
    float interval;         // segundos entre disparos (CooldownInMs do template, que na prática é em segundos)
    int magazine;           // pente base (MagazineSize)
    float reload;           // segundos (ReloadTime)
    float range;            // alcance do raio (HitscanRange) ou distância máxima do projétil
    float projectileSpeed;  // m/s (ProjectileSpeed)
    ProjectileBehavior behavior;
    float aoeRadius;        // raio do dano em área (AoeRadius)
    float headshot;         // multiplicador de tiro na cabeça (regra do Zombie Zone)
    float aimFov;           // zoom do botão MIRAR (regra do Zombie Zone)
    int sound;
    float tracerR, tracerG, tracerB;
    float length;           // comprimento do modelo (posição do cano)
};

const WeaponDef& weaponDef(int id);

}  // namespace zz
