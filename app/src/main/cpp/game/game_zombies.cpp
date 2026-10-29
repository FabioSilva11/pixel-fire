// Zumbis gerados proceduralmente: tipo, mutações, aparência e atributos sorteados; IA com habilidades
// (arremessos, investidas, gritos, explosões), projéteis, áreas de fogo/ácido e explosões em cadeia.
#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "game.h"

namespace zz {

using namespace proto;

namespace {

constexpr float kGravity = 28.0f;
constexpr float kStepHeight = 1.05f;

struct KindInfo {
    float hp, speed, damage, scale;
    int minStage;
    float weight;
    Vec3 tint;
};

const KindInfo kKinds[static_cast<int>(ZombieKind::Count)] = {
    {1.00f, 2.25f, 1.00f, 1.00f, 1, 10.0f, {0.70f, 0.95f, 0.60f}},  // Errante
    {0.70f, 3.60f, 0.80f, 0.95f, 1, 4.0f, {0.80f, 0.84f, 0.98f}},   // Corredor
    {2.60f, 1.85f, 1.70f, 1.32f, 3, 1.6f, {0.56f, 0.76f, 0.50f}},   // Brutamontes
    {0.85f, 2.10f, 0.90f, 1.00f, 2, 2.6f, {0.78f, 0.80f, 0.70f}},   // Arremessador
    {0.90f, 2.00f, 0.80f, 1.00f, 3, 2.2f, {1.00f, 0.74f, 0.55f}},   // Incendiário
    {0.90f, 3.10f, 0.70f, 0.98f, 2, 2.6f, {0.86f, 0.70f, 0.70f}},   // Retalhador
    {0.80f, 3.00f, 1.00f, 1.05f, 4, 1.8f, {0.70f, 1.00f, 0.42f}},   // Inchado
    {0.60f, 2.40f, 0.60f, 0.95f, 5, 1.1f, {0.86f, 0.76f, 0.96f}},   // Gritador
    {0.80f, 2.20f, 0.70f, 1.00f, 6, 1.8f, {0.88f, 0.96f, 0.42f}},   // Cuspidor
    {0.80f, 2.60f, 0.80f, 1.00f, 7, 1.8f, {0.72f, 0.82f, 0.92f}},   // Atirador de facas
};

bool isRanged(ZombieKind k) {
    return k == ZombieKind::Thrower || k == ZombieKind::Molotov || k == ZombieKind::Spitter || k == ZombieKind::KnifeThrower;
}

ProjectileKind projectileFor(ZombieKind k) {
    switch (k) {
        case ZombieKind::Molotov: return ProjectileKind::Molotov;
        case ZombieKind::Spitter: return ProjectileKind::Acid;
        case ZombieKind::KnifeThrower: return ProjectileKind::Knife;
        default: return ProjectileKind::Rock;
    }
}

// Caixa da malha de um nó do modelo (para capacete/colete dos zumbis blindados).
bool nodeBounds(const CharacterAsset* asset, int node, Vec3& mn, Vec3& mx) {
    static std::unordered_map<const CharacterAsset*, std::vector<std::pair<Vec3, Vec3>>> cache;
    auto& list = cache[asset];
    const auto& nodes = asset->model.nodes();
    if (list.size() != nodes.size()) {
        list.assign(nodes.size(), {Vec3(1, 1, 1), Vec3(-1, -1, -1)});
        for (size_t i = 0; i < nodes.size(); ++i) {
            int m = nodes[i].mesh;
            if (m < 0 || m >= static_cast<int>(asset->model.meshes().size())) continue;
            const auto& v = asset->model.meshes()[static_cast<size_t>(m)].vertices;
            if (v.size() < 8) continue;
            Vec3 a(v[0], v[1], v[2]), b = a;
            for (size_t k = 0; k + 7 < v.size(); k += 8) {
                a = {std::min(a.x, v[k]), std::min(a.y, v[k + 1]), std::min(a.z, v[k + 2])};
                b = {std::max(b.x, v[k]), std::max(b.y, v[k + 1]), std::max(b.z, v[k + 2])};
            }
            list[i] = {a, b};
        }
    }
    if (node < 0 || node >= static_cast<int>(list.size())) return false;
    mn = list[static_cast<size_t>(node)].first;
    mx = list[static_cast<size_t>(node)].second;
    return mx.x >= mn.x;
}

}  // namespace

void Game::loadZombieAssets() {
    // Zumbis de blocos: pele esverdeada, roupas rasgadas e variações por partida.
    zombieLooks_.clear();
    for (int i = 0; i < 12; ++i) zombieLooks_.push_back(zombieLook(i, config_.seed));
    for (uint32_t look : zombieLooks_) character(look);
}

ZombieKind Game::rollKind() {
    const int count = static_cast<int>(ZombieKind::Count);
    float w[static_cast<int>(ZombieKind::Count)];
    float total = 0;
    const int stage = config_.stage;
    for (int k = 0; k < count; ++k) {
        const KindInfo& info = kKinds[k];
        if (stage < info.minStage) { w[k] = 0; continue; }
        float ramp = std::min(1.0f, (stage - info.minStage + 1) / 3.0f);
        w[k] = info.weight * (0.35f + 0.65f * ramp);
    }
    // Cada bioma favorece certos tipos.
    switch (theme_.id) {
        case 0: w[static_cast<int>(ZombieKind::Runner)] *= 1.2f; w[static_cast<int>(ZombieKind::Thrower)] *= 1.2f; break;
        case 1: w[static_cast<int>(ZombieKind::Molotov)] *= 1.7f; w[static_cast<int>(ZombieKind::Spitter)] *= 1.2f; break;
        case 2: w[static_cast<int>(ZombieKind::KnifeThrower)] *= 1.7f; w[static_cast<int>(ZombieKind::Knife)] *= 1.3f; break;
        default: w[static_cast<int>(ZombieKind::Exploder)] *= 1.6f; w[static_cast<int>(ZombieKind::Spitter)] *= 1.5f; break;
    }
    if (horizonte::daylight(worldTime_) < 0.5f) w[static_cast<int>(ZombieKind::Runner)] *= 1.3f;
    for (int k = 0; k < count; ++k) total += w[k];
    float r = rng_.range(0, total);
    for (int k = 0; k < count; ++k) {
        if (r < w[k]) return static_cast<ZombieKind>(k);
        r -= w[k];
    }
    return ZombieKind::Walker;
}

uint8_t Game::rollMutations() {
    float chance = std::min(0.04f + config_.stage * 0.012f, 0.32f);
    uint8_t m = 0;
    if (rng_.chance(chance)) {
        m |= static_cast<uint8_t>(1 << rng_.rangeInt(0, 5));
        if (config_.stage >= 12 && rng_.chance(0.3f)) m |= static_cast<uint8_t>(1 << rng_.rangeInt(0, 5));
    }
    return m;
}

Zombie Game::makeZombie(ZombieKind kind, uint8_t mutations, const Vec3& pos) {
    Zombie z;
    z.kind = kind;
    z.mutations = mutations;
    const KindInfo& info = kKinds[static_cast<int>(kind)];
    if (!zombieLooks_.empty()) z.asset = character(zombieLooks_[static_cast<size_t>(rng_.rangeInt(0, static_cast<int>(zombieLooks_.size()) - 1))]);
    // Tom de pele: cor do tipo com variação (esverdeado, acinzentado ou azulado nos comuns).
    Vec3 tint = info.tint;
    if (kind == ZombieKind::Walker || kind == ZombieKind::Runner) {
        int palette = rng_.rangeInt(0, 3);
        if (palette == 1) tint = {0.76f, 0.8f, 0.74f};
        else if (palette == 2) tint = {0.66f, 0.78f, 0.92f};
        else if (palette == 3) tint = {0.74f, 0.9f, 0.56f};
    }
    float v = rng_.range(0.88f, 1.06f);
    z.tint = Vec3(tint.x * v + rng_.range(-0.04f, 0.04f), tint.y * v + rng_.range(-0.04f, 0.04f), tint.z * v + rng_.range(-0.04f, 0.04f));
    if (mutations & MUT_TOXIC) z.tint = lerp(z.tint, Vec3(0.5f, 1.0f, 0.35f), 0.35f);
    z.maxHealth = config_.zombieHealth * info.hp * rng_.range(0.9f, 1.15f);
    z.speed = info.speed * config_.zombieSpeed * rng_.range(0.92f, 1.08f);
    z.damage = config_.zombieDamage * info.damage;
    z.scale = info.scale * rng_.range(0.94f, 1.06f);
    if (kind == ZombieKind::Exploder) z.bloat = 1.28f;
    if (mutations & MUT_GIANT) { z.scale *= 1.22f; z.maxHealth *= 1.6f; z.damage *= 1.25f; }
    if (mutations & MUT_SWIFT) z.speed *= 1.3f;
    if (mutations & MUT_ARMORED) z.maxHealth *= 1.15f;
    if (mutations) z.maxHealth *= 1.25f;
    z.health = z.maxHealth;
    z.body.halfWidth = 0.32f * z.scale * std::min(1.15f, z.bloat);
    z.body.height = 1.75f * z.scale;
    z.body.position = pos;
    z.home = pos;
    z.lastPos = pos;
    z.yaw = rng_.range(-kPi, kPi);
    z.groanTimer = rng_.range(2.0f, 7.0f);
    z.wanderTimer = rng_.range(0.5f, 4.0f);
    z.abilityTimer = rng_.range(1.0f, 3.0f);
    z.strafe = rng_.chance(0.5f) ? 1.0f : -1.0f;
    z.strafeTimer = rng_.range(1.5f, 3.5f);
    z.perceptionTimer = rng_.range(0.0f, 0.25f);
    if (z.asset) {
        z.anim.play(z.asset->model.clipIndex("idle"), true, 0);
        z.anim.update(rng_.range(0.0f, 2.0f));
    }
    return z;
}

void Game::spawnZombie(const Vec3& pos, bool alerted, int nest, bool forceElite, int forceKind) {
    if (noEnemies_) return;  // autopiloto de desempenho sem inimigos
    // forceKind >= 0 ignora a tabela de sorteio (e o `minStage` dela): usado pelo ninho instável.
    ZombieKind kind = forceKind >= 0 ? static_cast<ZombieKind>(forceKind) : rollKind();
    uint8_t mut = rollMutations();
    if (forceElite && !mut) mut = static_cast<uint8_t>(1 << rng_.rangeInt(0, 5));
    Zombie z = makeZombie(kind, mut, pos);
    z.nest = nest;
    if (alerted) {
        z.alerted = true;
        z.state = ZState::Chase;
    }
    zombies_.push_back(std::move(z));
}

void Game::updateZombies(float dt) {
    for (size_t i = 0; i < zombies_.size(); ++i) updateZombie(zombies_[i], i, dt);
    separateBodies();
    // Chamados do grito (fora do laço: push_back invalidaria as referências).
    for (const Vec3& p : pendingSpawns_) spawnZombie(p, true);
    pendingSpawns_.clear();
}

// Colisão: zumbis não atravessam o jogador nem se empilham uns dentro dos outros.
// O empurrão respeita as paredes (moveBody) e não mexe no estado de chão/água.
void Game::separateBodies() {
    auto push = [&](BodyState& b, const Vec3& d) {
        bool ground = b.onGround, water = b.inWater;
        moveBody(world_, b, d, 0.0f);
        b.onGround = ground;
        b.inWater = water;
    };
    for (Zombie& z : zombies_) {
        if (z.state == ZState::Dead) continue;
        Vec3 d(z.body.position.x - player_.position.x, 0, z.body.position.z - player_.position.z);
        float dist = lengthXZ(d);
        float minDist = player_.halfWidth + z.body.halfWidth;
        if (dist >= minDist || std::fabs(z.body.position.y - player_.position.y) > 1.7f) continue;
        Vec3 n = dist > 1e-3f ? d / dist : forwardFromYaw(z.yaw);
        float pen = minDist - dist;
        bool heavy = z.kind == ZombieKind::Brute || (z.mutations & MUT_GIANT);
        float share = heavy ? 0.35f : 0.7f;  // quanto o zumbi recua (o resto empurra o jogador)
        push(z.body, n * (pen * share));
        if (!dead_ && mantleTime_ < 0) push(player_, n * (-pen * (1.0f - share)));
    }
    for (size_t i = 0; i < zombies_.size(); ++i) {
        Zombie& a = zombies_[i];
        if (a.state == ZState::Dead) continue;
        for (size_t j = i + 1; j < zombies_.size(); ++j) {
            Zombie& b = zombies_[j];
            if (b.state == ZState::Dead) continue;
            Vec3 d(b.body.position.x - a.body.position.x, 0, b.body.position.z - a.body.position.z);
            float minDist = a.body.halfWidth + b.body.halfWidth;
            if (std::fabs(d.x) >= minDist || std::fabs(d.z) >= minDist) continue;
            float dist = lengthXZ(d);
            if (dist >= minDist || std::fabs(a.body.position.y - b.body.position.y) > 1.7f) continue;
            Vec3 n = dist > 1e-3f ? d / dist : Vec3(1, 0, 0);
            float pen = (minDist - dist) * 0.5f;
            push(a.body, n * -pen);
            push(b.body, n * pen);
        }
    }
}

void Game::alertNear(const Vec3& at, float radius) {
    for (auto& z : zombies_) {
        if (z.state == ZState::Dead || z.alerted) continue;
        if (distXZ(z.body.position, at) < radius) alertZombie(z);
    }
}

void Game::updateZombie(Zombie& z, size_t index, float dt) {
    const CharacterModel* model = z.asset ? &z.asset->model : nullptr;
    z.hitFlash = std::max(0.0f, z.hitFlash - dt);
    z.barTimer = std::max(0.0f, z.barTimer - dt);
    if (z.state == ZState::Dead) {
        z.deathTime += dt;
        if (z.deathTime > 1.8f) z.body.position.y -= dt * 0.7f;
        z.anim.update(dt);
        return;
    }
    // Fora da área carregada o zumbi fica congelado (não cai pelo mundo vazio).
    if (!world_.columnReady(static_cast<int>(std::floor(z.body.position.x)), static_cast<int>(std::floor(z.body.position.z)))) return;
    z.cooldown = std::max(-1.0f, z.cooldown - dt);
    z.stun = std::max(0.0f, z.stun - dt);
    z.frenzy = std::max(0.0f, z.frenzy - dt);
    z.itemHidden = std::max(0.0f, z.itemHidden - dt);
    z.abilityTimer -= dt;
    if (z.burn > 0) {
        z.burn = std::max(0.0f, z.burn - dt);
        z.health -= z.burnDps * dt;
        z.barTimer = std::max(z.barTimer, 0.5f);
        if (rng_.chance(dt * 14.0f)) spawnBurst(z.body.position + Vec3(0, 1.0f * z.scale, 0), {1.0f, 0.55f, 0.15f, 1.0f}, 1, 1.2f, 0.25f, true, -2.0f);
        if (z.health <= 0) {
            killZombie(z, false);
            return;
        }
    }
    if (z.mutations & MUT_REGEN) {
        z.regenDelay -= dt;
        if (z.regenDelay <= 0 && z.health < z.maxHealth) z.health = std::min(z.maxHealth, z.health + z.maxHealth * 0.04f * dt);
    }
    const float rage = ((z.mutations & MUT_RAGE) && z.health < z.maxHealth * 0.4f) ? 1.45f : 1.0f;

    Vec3 pos = z.body.position;
    Vec3 toPlayer = player_.position - pos;
    float dist = lengthXZ(toPlayer);
    float dy = toPlayer.y;

    if (!z.announced && dist < 30.0f) {
        z.announced = true;
        int bit = 1 << static_cast<int>(z.kind);
        if (!(seenKinds_ & bit)) {
            seenKinds_ |= bit;
            if (z.kind != ZombieKind::Walker) emit(EV_NEW_KIND, static_cast<int>(z.kind));
        }
        if (z.mutations) emit(EV_ELITE, static_cast<int>(z.kind), z.mutations);
    }

    z.perceptionTimer -= dt;
    if (z.perceptionTimer <= 0) {
        z.perceptionTimer = 0.25f + static_cast<float>(index % 5) * 0.02f;
        // Noite vermelha: o escuro só atrapalha o jogador (os zumbis enxergam como de dia) e a atenção cresce com `aggression`.
        float sight = (horizonte::daylight(worldTime_) < 0.5f && !config_.redSky) ? 14.0f : 21.0f;
        if (isRanged(z.kind)) sight += 6.0f;
        if (crouching_) sight *= 0.6f;  // agachado: furtivo
        sight *= config_.aggression;
        const float notice = (crouching_ ? 3.5f : 6.5f) * config_.aggression;
        z.canSee = dist < sight + 10.0f && clearLineOfSight(pos + Vec3(0, 1.5f * z.scale, 0), player_.position + Vec3(0, 1.5f, 0));
        if (!z.alerted && !dead_ && ((z.canSee && dist < sight) || dist < notice)) {
            alertZombie(z);
            emitSound(SND_GROAN, pos, 0.8f);
            z.groanTimer = rng_.range(4.0f, 8.0f);
            if (z.kind == ZombieKind::Screamer) z.abilityTimer = 0;
        }
        if (z.alerted) {
            if (z.canSee || dist < 12.0f) z.lostSight = 0;
            else z.lostSight += 0.25f;
            if (z.lostSight > 10.0f * config_.aggression && dist > 40.0f && z.nest < 0) z.alerted = false;
        }
    }

    Vec3 desired;
    float speed = 0;
    const Vec3 flat = dist > 1e-3f ? Vec3(toPlayer.x, 0, toPlayer.z) / dist : Vec3(0, 0, 1);
    const Vec3 side(-flat.z, 0, flat.x);
    if (z.stun > 0) {
        // atordoado: só física
    } else if (z.action != ZAction::None) {
        zombieAbility(z, dist, dt);
        if (z.state == ZState::Dead) return;
        if (z.action == ZAction::Dash) {
            desired = toPlayer;
            speed = z.speed * 3.2f;
        }
    } else if (z.state == ZState::Attack) {
        z.windup -= dt;
        if (!z.attackHit && z.windup <= 0) {
            z.attackHit = true;
            bool knife = z.kind == ZombieKind::Knife;
            emitSound(knife ? SND_KNIFE : SND_ZOMBIE_ATTACK, pos, 0.7f);
            if (dist < 1.95f * std::max(1.0f, z.scale) && std::fabs(dy) < 1.8f && !dead_) {
                damagePlayer(z.damage, pos);
                if (knife) {
                    if (bleeding_ <= 0) emit(EV_STATUS, 2);
                    bleeding_ = std::max(bleeding_, 4.0f);
                }
                if (z.mutations & MUT_TOXIC) {
                    if (poisoned_ <= 0) emit(EV_STATUS, 3);
                    poisoned_ = std::max(poisoned_, 4.0f);
                }
            }
        }
        float recover = z.kind == ZombieKind::Knife ? -0.18f : -0.35f;
        if (z.windup <= recover) {
            z.state = ZState::Chase;
            z.cooldown = z.kind == ZombieKind::Knife ? 0.35f : 0.75f;
        }
    } else if (z.alerted && !dead_ && !victory_) {
        z.state = ZState::Chase;
        const bool ranged = isRanged(z.kind);
        const float meleeRange = 1.35f + 0.2f * z.scale;
        const bool level = std::fabs(dy) < 1.8f;
        if (ranged && z.abilityTimer <= 0 && z.canSee && dist > 4.5f && dist < 26.0f) {
            z.action = ZAction::Throw;
            z.actionTime = 0;
            z.actionDone = false;
            z.windup = z.kind == ZombieKind::KnifeThrower ? 0.45f : 0.7f;
        } else if (z.kind == ZombieKind::Brute && dist < 3.4f && level && z.abilityTimer <= 0) {
            z.action = ZAction::Slam;
            z.actionTime = 0;
            z.actionDone = false;
            emitSound(SND_GROAN, pos, 1.0f);
        } else if (z.kind == ZombieKind::Knife && dist > 3.5f && dist < 8.0f && z.abilityTimer <= 0 && z.canSee) {
            z.action = ZAction::Dash;
            z.actionTime = 0;
            z.actionDone = false;
        } else if (z.kind == ZombieKind::Exploder && dist < 2.6f && level) {
            z.action = ZAction::Fuse;
            z.actionTime = 0;
            z.actionDone = false;
            emitSound(SND_GROAN, pos, 1.0f);
        } else if (z.kind == ZombieKind::Screamer && z.abilityTimer <= 0) {
            z.action = ZAction::Scream;
            z.actionTime = 0;
            z.actionDone = false;
        } else if (z.kind == ZombieKind::Runner && dist > 3.0f && dist < 6.0f && z.abilityTimer <= 0 && z.canSee && z.body.onGround) {
            z.body.velocity += flat * 7.5f + Vec3(0, 4.5f, 0);  // bote
            z.abilityTimer = 3.0f;
        } else if (dist < meleeRange && std::fabs(dy) < 1.6f && z.cooldown <= 0 && z.kind != ZombieKind::Exploder) {
            z.state = ZState::Attack;
            z.windup = z.kind == ZombieKind::Knife ? 0.25f : 0.42f;
            z.attackHit = false;
            if (model) z.anim.play(model->clipIndex("attack-melee-right"), false, 0.08f, z.kind == ZombieKind::Knife ? 1.7f : 1.2f);
        } else {
            z.strafeTimer -= dt;
            if (z.strafeTimer <= 0) {
                z.strafe = -z.strafe;
                z.strafeTimer = rng_.range(1.5f, 3.5f);
            }
            if (ranged && z.canSee && dist < 7.0f) {
                desired = -flat + side * (z.strafe * 0.6f);  // recua para manter distância
                speed = z.speed * 0.85f;
            } else if (ranged && z.canSee && dist < 18.0f) {
                desired = side * z.strafe + flat * 0.15f;     // circula o jogador
                speed = z.speed * 0.6f;
            } else {
                Vec3 target;
                bool direct = (z.canSee && dist < 10.0f) || dist < 2.5f;
                if (!direct && nav_.nextStep(pos, target)) desired = target - pos;
                else desired = toPlayer;
                speed = z.speed;
            }
            speed *= rage * (z.frenzy > 0 ? 1.35f : 1.0f);
        }
    } else {
        z.state = ZState::Wander;
        z.wanderTimer -= dt;
        if (z.wanderTimer <= 0 || distXZ(pos, z.wanderTarget) < 0.8f) {
            Vec3 base = z.home;
            if (missionTime_ > 50.0f && index % 2 == 0 && z.nest < 0) base = player_.position + Vec3(rng_.range(-20, 20), 0, rng_.range(-20, 20));
            z.wanderTarget = base + Vec3(rng_.range(-9, 9), 0, rng_.range(-9, 9));
            z.wanderTimer = rng_.range(4.0f, 8.0f);
        }
        desired = z.wanderTarget - pos;
        speed = lengthXZ(desired) > 0.6f ? 1.0f : 0.0f;
        if (speed == 0) z.state = ZState::Idle;
    }

    // Separação entre zumbis.
    for (size_t j = 0; j < zombies_.size(); ++j) {
        if (j == index || zombies_[j].state == ZState::Dead) continue;
        Vec3 d = pos - zombies_[j].body.position;
        float dd = lengthXZ(d);
        if (dd < 0.9f && dd > 1e-3f) desired += Vec3(d.x, 0, d.z) / dd * ((0.9f - dd) * 3.0f);
    }
    if (z.sidestepTimer > 0) {
        z.sidestepTimer -= dt;
        desired = desired * 0.3f + z.sidestepDir * 2.0f;
    }

    Vec3 wish;
    if (speed > 0 && lengthXZ(desired) > 1e-3f) wish = normalize(Vec3(desired.x, 0, desired.z)) * (speed * (z.body.inWater ? 0.6f : 1.0f));
    float accel = z.body.onGround ? (z.action == ZAction::Dash ? 22.0f : 10.0f) : 3.0f;
    z.body.velocity.x = lerpf(z.body.velocity.x, wish.x, damp(accel, dt));
    z.body.velocity.z = lerpf(z.body.velocity.z, wish.z, damp(accel, dt));
    if (z.body.hitWall && z.body.onGround && z.alerted && speed > 0) z.body.velocity.y = 7.6f;
    if (z.body.inWater) {
        z.body.velocity.y -= 9.0f * dt;
        if (world_.isWaterAt(z.body.position + Vec3(0, 1.3f, 0))) z.body.velocity.y += 17.0f * dt;
        z.body.velocity.y = clampf(z.body.velocity.y, -3.0f, 5.0f);
        z.burn = 0;
    } else {
        z.body.velocity.y = std::max(-40.0f, z.body.velocity.y - kGravity * dt);
    }
    moveBody(world_, z.body, z.body.velocity * dt, kStepHeight);
    z.body.stepOffset = 0;
    if (z.body.position.y < 1.0f) {
        z.body.position = z.home;
        z.body.velocity = {};
    }

    if (speed > 0.5f) {
        float moved = distXZ(z.body.position, z.lastPos);
        if (moved < speed * dt * 0.25f) z.stuckTimer += dt;
        else z.stuckTimer = std::max(0.0f, z.stuckTimer - dt);
        if (z.stuckTimer > 0.8f) {
            z.stuckTimer = 0;
            z.sidestepTimer = 0.7f;
            Vec3 d = normalize(Vec3(desired.x, 0, desired.z));
            z.sidestepDir = rng_.chance(0.5f) ? Vec3(-d.z, 0, d.x) : Vec3(d.z, 0, -d.x);
        }
    }
    z.lastPos = z.body.position;

    float horizontal = lengthXZ(z.body.velocity);
    float targetYaw = z.yaw;
    bool facePlayer = z.state == ZState::Attack || z.action != ZAction::None || (z.alerted && (dist < 3.0f || (isRanged(z.kind) && z.canSee && dist < 26.0f)));
    if (facePlayer) targetYaw = yawFromDirection(toPlayer.x, toPlayer.z);
    else if (horizontal > 0.3f) targetYaw = yawFromDirection(z.body.velocity.x, z.body.velocity.z);
    if (z.action == ZAction::Fuse) targetYaw += std::sin(time_ * 40.0f) * 0.25f;
    z.yaw = lerpAngle(z.yaw, targetYaw, damp(9.0f, dt));

    if (model) {
        if (z.state == ZState::Attack) {
            if (z.anim.finished(*model)) z.anim.play(model->clipIndex("idle"), true, 0.1f);
        } else if (horizontal > 2.7f) {
            z.anim.play(model->clipIndex("sprint"), true, 0.2f, horizontal / 5.5f);
        } else if (horizontal > 0.3f) {
            z.anim.play(model->clipIndex("walk"), true, 0.2f, std::max(0.5f, horizontal / 2.6f));
        } else {
            z.anim.play(model->clipIndex("idle"), true, 0.25f);
        }
        z.anim.update(dt);
    }
    if (z.alerted) {
        z.groanTimer -= dt;
        if (z.groanTimer <= 0) {
            z.groanTimer = rng_.range(4.0f, 9.0f);
            emitSound(SND_GROAN, z.body.position, 0.65f);
        }
    }
}

void Game::zombieAbility(Zombie& z, float dist, float dt) {
    z.actionTime += dt;
    const Vec3 pos = z.body.position;
    const float dy = player_.position.y - pos.y;
    switch (z.action) {
        case ZAction::Throw:
            if (!z.actionDone && z.actionTime >= z.windup) {
                z.actionDone = true;
                if (!dead_) throwProjectile(z, projectileFor(z.kind));
                z.itemHidden = 0.6f;
            }
            if (z.actionTime >= z.windup + 0.35f) {
                z.action = ZAction::None;
                z.abilityTimer = rng_.range(2.4f, 4.0f) * (z.kind == ZombieKind::KnifeThrower ? 0.7f : 1.0f) * (z.frenzy > 0 ? 0.7f : 1.0f);
            }
            break;
        case ZAction::Slam:
            if (!z.actionDone && z.actionTime >= 0.75f) {
                z.actionDone = true;
                spawnBurst(pos + Vec3(0, 0.2f, 0), {0.62f, 0.55f, 0.45f, 0.9f}, 26, 6.0f, 0.3f, false, 8.0f);
                emitSound(SND_ROCK, pos, 1.0f);
                float d = length(player_.position - pos);
                if (d < 3.8f) shake_ = std::max(shake_, 0.8f);
                if (d < 3.6f && std::fabs(dy) < 2.0f && !dead_) {
                    damagePlayer(z.damage * 1.2f, pos);
                    Vec3 away = d > 0.1f ? normalize(Vec3(player_.position.x - pos.x, 0, player_.position.z - pos.z)) : Vec3(1, 0, 0);
                    player_.velocity += away * 9.0f + Vec3(0, 6.0f, 0);
                }
            }
            if (z.actionTime >= 1.25f) {
                z.action = ZAction::None;
                z.abilityTimer = 4.5f;
                z.cooldown = 0.5f;
            }
            break;
        case ZAction::Dash:
            if (!z.actionDone && dist < 1.5f && std::fabs(dy) < 1.8f && !dead_) {
                z.actionDone = true;
                damagePlayer(z.damage * 0.9f, pos);
                if (bleeding_ <= 0) emit(EV_STATUS, 2);
                bleeding_ = std::max(bleeding_, 4.5f);
                emitSound(SND_KNIFE, pos, 0.9f);
            }
            if (z.actionTime >= 0.38f) {
                z.action = ZAction::None;
                z.abilityTimer = 3.5f;
                z.cooldown = 0.3f;
            }
            break;
        case ZAction::Fuse:
            z.body.velocity.x *= 0.8f;
            z.body.velocity.z *= 0.8f;
            if (z.actionTime >= 0.85f) {
                z.exploded = true;
                explode(pos + Vec3(0, 0.9f, 0), 4.6f, z.damage * 3.2f, 150.0f, false);
                killZombie(z, false);
            }
            break;
        case ZAction::Scream:
            if (!z.actionDone && z.actionTime >= 0.5f) {
                z.actionDone = true;
                emitSound(SND_SCREAM, pos, 1.0f);
                shake_ = std::max(shake_, 0.3f);
                for (Zombie& o : zombies_) {
                    if (o.state == ZState::Dead || distXZ(o.body.position, pos) > 38.0f) continue;
                    alertZombie(o);
                    o.lostSight = 0;
                    o.frenzy = 8.0f;
                }
                int alive = 0;
                for (const Zombie& o : zombies_)
                    if (o.state != ZState::Dead && distXZ(o.body.position, player_.position) < 95.0f) ++alive;
                for (int i = 0; i < 2 && alive + i < config_.zombieCount + 5; ++i) {
                    Vec3 p;
                    if (findSpawnSpot(pos, 6.0f, 14.0f, false, p)) pendingSpawns_.push_back(p);
                }
                for (int k = 0; k < 18; ++k) {
                    float a = k * kTwoPi / 18.0f;
                    FxParticle p;
                    p.position = pos + Vec3(0, 1.6f * z.scale, 0);
                    p.velocity = Vec3(std::cos(a), 0.1f, std::sin(a)) * 7.0f;
                    p.color = {0.9f, 0.7f, 1.0f, 0.7f};
                    p.size = 0.25f;
                    p.life = p.maxLife = 0.5f;
                    p.additive = true;
                    particles_.push_back(p);
                }
            }
            if (z.actionTime >= 1.3f) {
                z.action = ZAction::None;
                z.abilityTimer = 14.0f;
            }
            break;
        case ZAction::None:
            break;
    }
}

void Game::throwProjectile(Zombie& z, ProjectileKind kind) {
    Vec3 hand = z.body.position + Vec3(0, zombieHeight(z) * 0.95f, 0) + forwardFromYaw(z.yaw) * 0.45f + rightFromYaw(z.yaw) * 0.3f;
    Vec3 target = player_.position + Vec3(0, kind == ProjectileKind::Knife ? 1.1f : 0.0f, 0);
    float dist = distXZ(hand, target);
    float t;
    switch (kind) {
        case ProjectileKind::Rock: t = clampf(dist / 15.0f, 0.45f, 1.5f); break;
        case ProjectileKind::Molotov: t = clampf(dist / 13.0f, 0.55f, 1.7f); break;
        case ProjectileKind::Acid: t = clampf(dist / 16.0f, 0.4f, 1.3f); break;
        default: t = std::max(0.12f, dist / 24.0f); break;
    }
    // Mira à frente do jogador (antecipa o movimento) com erro que diminui nas fases altas.
    target.x += player_.velocity.x * t * 0.65f;
    target.z += player_.velocity.z * t * 0.65f;
    float err = (kind == ProjectileKind::Knife ? 0.45f : 1.25f) * (1.0f - std::min(0.5f, config_.stage * 0.02f)) + dist * 0.03f;
    target.x += rng_.range(-err, err);
    target.z += rng_.range(-err, err);
    if (kind == ProjectileKind::Molotov || kind == ProjectileKind::Acid) {
        int tx = static_cast<int>(std::floor(target.x)), tz = static_cast<int>(std::floor(target.z));
        if (world_.columnReady(tx, tz)) target.y = static_cast<float>(world_.standHeight(tx, tz));
    }
    Projectile p;
    p.kind = kind;
    p.pos = hand;
    p.target = target;
    p.gravity = kind == ProjectileKind::Knife ? 4.0f : 22.0f;
    Vec3 d = target - hand;
    p.vel = {d.x / t, (d.y + 0.5f * p.gravity * t * t) / t, d.z / t};
    switch (kind) {
        case ProjectileKind::Rock: p.damage = z.damage * 1.1f; break;
        case ProjectileKind::Molotov: p.damage = z.damage * 0.6f; break;
        case ProjectileKind::Acid: p.damage = z.damage * 0.6f; break;
        case ProjectileKind::Knife: p.damage = z.damage * 0.9f; break;
        case ProjectileKind::Bullet: break;  // só o jogador dispara balas
        case ProjectileKind::Grenade: p.damage = z.damage; break;  // zumbis não usam granadas
        case ProjectileKind::Smoke: p.damage = 0.0f; break;
    }
    projectiles_.push_back(p);
    emitSound(kind == ProjectileKind::Acid ? SND_SPIT : SND_THROW, hand, 0.8f);
}

void Game::updateProjectiles(float dt) {
    for (Projectile& p : projectiles_) {
        p.life -= dt;
        p.spin += dt * 12.0f;
        Vec3 from = p.pos;
        p.vel.y -= p.gravity * dt;
        Vec3 to = from + p.vel * dt;
        if (p.kind == ProjectileKind::Bullet) {
            // Bala da escopeta: varre o trecho deste quadro como um raio (o SphereCast do template).
            Vec3 seg = to - from;
            float len = length(seg);
            Vec3 dir = len > 1e-5f ? seg / len : Vec3(0, 0, 1);
            ShotTrace hit = traceShot(from, dir, len, 0.0f);
            if (hit.kind != ShotTrace::None) {
                Vec3 at = from + dir * hit.t;
                // AreaOfEffect explode sem ferir quem atirou; DirectDamage acerta só o alvo.
                if (p.radius > 0) explode(at, p.radius, 0.0f, p.damage, false, p.friendly);
                else applyShot(hit, from, dir, p.weapon, p.damage);
                spawnBurst(at, {1.0f, 0.75f, 0.35f, 1.0f}, 6, 2.4f, 0.08f, true, 6.0f);
                p.life = -1;
            } else if (world_.isWaterAt(to)) {
                spawnBurst(to, {0.6f, 0.75f, 1.0f, 0.8f}, 6, 2.0f, 0.12f, false, 9.0f);
                p.life = -1;
            } else {
                p.pos = to;
                if (p.life <= 0 || p.pos.y < 0) p.life = -1;
            }
            continue;
        }
        bool hitPlayer = false;
        if (!dead_ && !p.friendly && p.kind != ProjectileKind::Grenade) {
            for (int s = 1; s <= 3; ++s) {
                Vec3 q = lerp(from, to, s / 3.0f);
                if (q.y > player_.position.y - 0.1f && q.y < player_.position.y + 1.85f && distXZ(q, player_.position) < 0.55f) {
                    hitPlayer = true;
                    to = q;
                    break;
                }
            }
        }
        bool hitWorld = false;
        Vec3 seg = to - from;
        float len = length(seg);
        RayHit wh;
        if (!hitPlayer && len > 1e-4f && world_.raycast(from, seg, len, wh)) {
            hitWorld = true;
            to = from + normalize(seg) * std::max(0.0f, wh.distance - 0.05f);
        }
        bool water = !hitPlayer && world_.isWaterAt(to);
        bool hitZombie = false;
        if (p.kind == ProjectileKind::Grenade || p.friendly) {
            for (const Zombie& z : zombies_) {
                if (z.state == ZState::Dead) continue;
                if (length(z.body.position + Vec3(0, 0.9f * z.scale, 0) - to) < 0.8f * z.scale + 0.3f) {
                    hitZombie = true;
                    break;
                }
            }
        }
        p.pos = to;
        if (p.kind == ProjectileKind::Acid && rng_.chance(dt * 30.0f) && particles_.size() < 820) {
            FxParticle t;
            t.position = p.pos;
            t.velocity = {0, -0.5f, 0};
            t.color = {0.5f, 1.0f, 0.25f, 0.8f};
            t.size = 0.2f;
            t.life = t.maxLife = 0.4f;
            t.additive = true;
            particles_.push_back(t);
        }
        if (hitPlayer || hitWorld || water || hitZombie || p.life <= 0 || p.pos.y < 0) {
            if (water && p.kind != ProjectileKind::Grenade) {
                spawnBurst(p.pos, {0.6f, 0.75f, 1.0f, 0.8f}, 10, 3.0f, 0.15f, false, 9.0f);
                emitSound(SND_SPLASH, p.pos, 0.5f);
            } else {
                impactProjectile(p, p.pos, hitPlayer);
            }
            p.life = -1;
        }
    }
    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(), [](const Projectile& p) { return p.life < 0; }), projectiles_.end());
}

void Game::impactProjectile(const Projectile& p, const Vec3& at, bool hitPlayer) {
    Vec3 back = at - normalize(p.vel);
    int gx = static_cast<int>(std::floor(at.x)), gz = static_cast<int>(std::floor(at.z));
    float groundY = world_.columnReady(gx, gz) ? static_cast<float>(world_.standHeight(gx, gz)) : at.y;
    if (groundY > at.y + 1.5f) groundY = std::floor(at.y);
    Vec3 ground(at.x, groundY, at.z);
    switch (p.kind) {
        case ProjectileKind::Rock:
            if (hitPlayer) {
                damagePlayer(p.damage, back);
                player_.velocity += normalize(Vec3(p.vel.x, 0, p.vel.z)) * 3.0f;
            }
            spawnBurst(at, {0.5f, 0.48f, 0.45f, 1.0f}, 10, 3.0f, 0.14f, false, 14.0f);
            emitSound(SND_ROCK, at, 0.8f);
            break;
        case ProjectileKind::Knife:
            if (hitPlayer) {
                damagePlayer(p.damage, back);
                if (bleeding_ <= 0) emit(EV_STATUS, 2);
                bleeding_ = std::max(bleeding_, 3.5f);
            }
            spawnBurst(at, {1.0f, 0.95f, 0.8f, 1.0f}, 5, 2.5f, 0.06f, true, 10.0f);
            emitSound(SND_KNIFE, at, 0.7f);
            break;
        case ProjectileKind::Molotov: {
            Hazard h;
            h.kind = 0;
            h.pos = ground;
            h.radius = p.friendly ? p.radius : 2.7f;
            h.life = h.maxLife = p.friendly ? 7.0f : 5.5f;
            h.damage = 5.0f * (1.0f + std::min(1.5f, config_.stage * 0.03f));
            h.friendly = p.friendly;
            h.zombieDps = p.friendly ? p.damage : 14.0f;
            if (!world_.isWaterAt(at)) hazards_.push_back(h);
            if (hitPlayer && !p.friendly) damagePlayer(p.damage, back);
            if (p.friendly) {
                // O molotov do jogador também queima na hora quem estiver no alcance.
                for (Zombie& z : zombies_) {
                    if (z.state == ZState::Dead || distXZ(z.body.position, ground) > h.radius) continue;
                    z.burn = std::max(z.burn, 3.0f);
                    z.burnDps = std::max(z.burnDps, h.zombieDps);
                    alertZombie(z);
                }
            }
            spawnBurst(at, {1.0f, 0.55f, 0.15f, 1.0f}, 30, 6.0f, 0.3f, true, -1.0f);
            spawnBurst(at, {0.7f, 0.9f, 0.6f, 0.8f}, 8, 4.0f, 0.08f, false, 12.0f);  // cacos de vidro
            addFlash(at + Vec3(0, 0.8f, 0), 12.0f, {1.0f, 0.55f, 0.2f}, 0.5f);
            emitSound(SND_FIRE, at, 1.0f);
            if (distXZ(at, player_.position) < 6.0f) shake_ = std::max(shake_, 0.3f);
            break;
        }
        case ProjectileKind::Grenade:
            explode(at + Vec3(0, 0.3f, 0), p.radius, 28.0f, p.damage, false, p.friendly);
            break;
        case ProjectileKind::Smoke: {
            SmokeCloud cloud;
            cloud.pos = ground;
            cloud.radius = 5.2f;
            cloud.life = cloud.maxLife = 11.0f;
            cloud.fxTimer = 0.0f;
            if (smokeClouds_.size() >= 8) smokeClouds_.erase(smokeClouds_.begin());
            smokeClouds_.push_back(cloud);
            emitSound(SND_THROW, at, 0.65f);
            break;
        }
        case ProjectileKind::Acid: {
            Hazard h;
            h.kind = 1;
            h.pos = ground;
            h.radius = 2.1f;
            h.life = h.maxLife = 6.0f;
            h.damage = 0;
            hazards_.push_back(h);
            if (hitPlayer) {
                damagePlayer(p.damage, back);
                if (poisoned_ <= 0) emit(EV_STATUS, 3);
                poisoned_ = std::max(poisoned_, 3.0f);
            }
            spawnBurst(at, {0.5f, 1.0f, 0.25f, 1.0f}, 18, 4.0f, 0.18f, true, 8.0f);
            emitSound(SND_SPIT, at, 0.8f);
            break;
        }
        case ProjectileKind::Bullet:
            break;  // tratada em updateProjectiles
    }
}

void Game::updateHazards(float dt) {
    for (Hazard& h : hazards_) {
        h.life -= dt;
        h.fxTimer -= dt;
        if (h.fxTimer <= 0 && particles_.size() < 860 && length(h.pos - camPos_) < 60.0f) {
            h.fxTimer = h.kind == 0 ? 0.025f : 0.08f;
            float a = rng_.range(0, kTwoPi), r = std::sqrt(rng_.uniform()) * h.radius;
            FxParticle p;
            p.position = h.pos + Vec3(std::cos(a) * r, 0.1f, std::sin(a) * r);
            if (h.kind == 0) {
                p.velocity = {rng_.range(-0.2f, 0.2f), rng_.range(1.5f, 3.0f), rng_.range(-0.2f, 0.2f)};
                p.color = {1.0f, rng_.range(0.35f, 0.7f), 0.12f, 1.0f};
                p.size = rng_.range(0.35f, 0.7f) * clampf(h.life / 1.0f, 0.3f, 1.0f);
                p.life = p.maxLife = rng_.range(0.35f, 0.7f);
            } else {
                p.velocity = {0, rng_.range(0.3f, 0.8f), 0};
                p.color = {0.55f, 1.0f, 0.3f, 0.8f};
                p.size = rng_.range(0.15f, 0.3f);
                p.life = p.maxLife = rng_.range(0.4f, 0.8f);
            }
            p.additive = true;
            particles_.push_back(p);
        }
        float d = distXZ(player_.position, h.pos);
        if (!h.friendly && d < h.radius && std::fabs(player_.position.y - h.pos.y) < 1.6f && !dead_) {
            if (h.kind == 0) {
                if (burning_ <= 0) emit(EV_STATUS, 1);
                burning_ = std::max(burning_, 2.0f);
                damageOverTime(h.damage * dt);
            } else {
                if (slowed_ <= 0) emit(EV_STATUS, 4);
                slowed_ = std::max(slowed_, 0.6f);
                poisoned_ = std::max(poisoned_, 1.5f);
            }
        }
        if (h.kind == 0) {
            for (Zombie& z : zombies_) {
                if (z.state == ZState::Dead || z.kind == ZombieKind::Molotov) continue;
                if (distXZ(z.body.position, h.pos) < h.radius && std::fabs(z.body.position.y - h.pos.y) < 1.6f) {
                    z.burn = std::max(z.burn, 1.5f);
                    z.burnDps = std::max(z.burnDps, h.zombieDps);
                }
            }
        }
    }
    hazards_.erase(std::remove_if(hazards_.begin(), hazards_.end(), [](const Hazard& h) { return h.life <= 0; }), hazards_.end());
}

void Game::explode(const Vec3& at, float radius, float playerDamage, float zombieDamage, bool fire, bool destroyTerrain) {
    explosions_.push_back({at, radius, playerDamage, zombieDamage, fire, destroyTerrain});
}

void Game::processExplosions() {
    int guard = 0;
    while (!explosions_.empty() && guard++ < 24) {
        Explosion e = explosions_.back();
        explosions_.pop_back();
        if (e.destroyTerrain && mode_ == Mode::Mission) {
            destroyTerrainSphere(world_,e.pos,e.radius,
                [this](int x,int y,int z){ return protectedTerrain(x,y,z); },
                [this](int x,int y,int z){ terrainEdited(x,y,z); });
        }
        spawnBurst(e.pos, {1.0f, 0.62f, 0.22f, 1.0f}, 44, 9.0f, 0.38f, true, 2.0f);
        spawnBurst(e.pos, {0.55f, 1.0f, 0.3f, 1.0f}, 16, 6.0f, 0.25f, true, 4.0f);
        spawnBurst(e.pos, {0.22f, 0.22f, 0.2f, 0.75f}, 22, 3.5f, 0.7f, false, -1.2f);
        addFlash(e.pos, e.radius * 4.0f, {1.0f, 0.65f, 0.3f}, 0.5f);
        emitSound(SND_EXPLOSION, e.pos, 1.0f);
        Vec3 chest = player_.position + Vec3(0, 1.0f, 0);
        float d = length(chest - e.pos);
        if (d < e.radius && !dead_) {
            float f = 1.0f - d / e.radius;
            damagePlayer(e.playerDamage * (0.35f + 0.65f * f), e.pos);
            Vec3 away = d > 0.1f ? normalize(chest - e.pos) : Vec3(0, 1, 0);
            player_.velocity += Vec3(away.x, 0, away.z) * (10.0f * f) + Vec3(0, 5.0f * f, 0);
            shake_ = 1.0f;
        } else if (d < e.radius * 3.0f) {
            shake_ = std::max(shake_, 0.6f * (1.0f - d / (e.radius * 3.0f)));
        }
        for (Zombie& z : zombies_) {
            if (z.state == ZState::Dead) continue;
            Vec3 c = z.body.position + Vec3(0, 0.9f * z.scale, 0);
            float dz = length(c - e.pos);
            if (dz >= e.radius) continue;
            float f = 1.0f - dz / e.radius;
            Vec3 away = dz > 0.1f ? normalize(c - e.pos) : Vec3(0, 1, 0);
            z.body.velocity += Vec3(away.x, 0, away.z) * (8.0f * f) + Vec3(0, 5.0f * f, 0);
            z.stun = std::max(z.stun, 0.6f);
            z.action = ZAction::None;
            damageZombie(z, e.zombieDamage * (0.4f + 0.6f * f), false, away, c);
        }
        if (e.fire) {
            Hazard h;
            h.kind = 0;
            h.pos = e.pos - Vec3(0, 0.9f, 0);
            h.radius = 2.4f;
            h.life = h.maxLife = 4.0f;
            h.damage = 5.0f;
            hazards_.push_back(h);
        }
    }
}

void Game::damageZombie(Zombie& z, float amount, bool headshot, const Vec3& dir, const Vec3& hitPoint) {
    if (z.state == ZState::Dead) return;
    if (z.mutations & MUT_ARMORED) amount *= headshot ? 0.8f : 0.5f;
    if (z.kind == ZombieKind::Brute && !headshot) amount *= 0.85f;
    z.health -= amount;
    z.hitFlash = 0.12f;
    alertZombie(z);
    z.lostSight = 0;
    z.barTimer = 2.5f;
    z.regenDelay = 3.0f;
    Vec3 push = normalize(Vec3(dir.x, 0, dir.z));
    bool heavy = z.kind == ZombieKind::Brute || (z.mutations & MUT_GIANT);
    z.body.velocity += push * (heavy ? 0.8f : 2.2f);
    labels_.push_back({hitPoint, static_cast<int>(std::lround(amount)), headshot ? 2 : 1, 0.8f});
    Vec4 blood = (z.mutations & MUT_ARMORED) && !headshot ? Vec4(0.75f, 0.75f, 0.8f, 1.0f) : Vec4(0.42f, 0.62f, 0.18f, 1.0f);
    spawnBurst(hitPoint, blood, headshot ? 9 : 5, 2.2f, 0.1f, false, 14.0f);
    emitSound(headshot ? SND_HEADSHOT : SND_HIT, hitPoint, headshot ? 0.9f : 0.6f);
    if (z.health <= 0) killZombie(z, headshot);
}

void Game::killZombie(Zombie& z, bool headshot) {
    if (z.state == ZState::Dead) return;
    z.health = 0;
    z.state = ZState::Dead;
    z.action = ZAction::None;
    z.deathTime = 0;
    z.barTimer = 0;
    if (z.asset) z.anim.play(z.asset->model.clipIndex("die"), false, 0.08f);
    ++kills_;
    if (headshot) ++headshots_;
    combo_ = comboTimer_ > 0 ? combo_ + 1 : 1;
    comboTimer_ = 3.0f;
    emit(EV_KILL, headshot ? 1 : 0, static_cast<int>(z.kind), combo_);
    emitSound(SND_ZOMBIE_DIE, z.body.position, 0.8f);

    Vec3 center = z.body.position + Vec3(0, 0.9f, 0);
    // O inchado explode ao morrer (mesmo abatido de longe) e o incendiário quebra o molotov.
    if (z.kind == ZombieKind::Exploder && !z.exploded) {
        z.exploded = true;
        explode(center, 3.8f, z.damage * 2.2f, 130.0f, false);
    }
    if (z.kind == ZombieKind::Molotov) {
        Hazard h;
        h.kind = 0;
        h.pos = z.body.position;
        h.radius = 2.0f;
        h.life = h.maxLife = 3.5f;
        h.damage = 4.0f;
        hazards_.push_back(h);
        emitSound(SND_FIRE, center, 0.7f);
    }

    rollZombieDrop(z);
    if (survival()) score_ += headshot ? 15 : 10;
    Vec3 drop = z.body.position + Vec3(0, 1.0f, 0);
    float healChance = z.mutations ? 0.3f : (health_ / maxHealth_ < 0.3f ? 0.08f : 0.02f);
    if (rng_.chance(healChance)) {
        Pickup p;
        p.type = 1;
        p.position = drop;
        p.velocity = {rng_.range(-1.5f, 1.5f), 4.5f, rng_.range(-1.5f, 1.5f)};
        pickups_.push_back(p);
    }
}

void Game::poseZombie(Pose& pose, const Zombie& z, const CharacterModel& m, float t) {
    int armR = m.nodeIndex("arm-right"), armL = m.nodeIndex("arm-left"), head = m.nodeIndex("head");
    auto setArm = [&](int arm, float pitch, float yaw, float roll) {
        if (arm >= 0) pose.r[arm] = Quat::axisAngle({0, 1, 0}, yaw) * Quat::axisAngle({0, 0, 1}, roll) * Quat::axisAngle({1, 0, 0}, pitch);
    };
    const float sway = std::sin(t * 4.0f) * 0.08f;
    // Postura base: braços estendidos (corpo a corpo) ou braço do item baixo (atiradores/facas).
    bool holding = isRanged(z.kind) || z.kind == ZombieKind::Knife;
    setArm(armL, -kPi * 0.5f + std::sin(t * 4.0f + 1.6f) * 0.08f, -0.12f, 0);
    if (holding) setArm(armR, -0.35f + sway, 0.1f, 0.08f);
    else setArm(armR, -kPi * 0.5f + sway, 0.12f, 0);
    if (head >= 0) pose.r[head] = Quat::axisAngle({0, 0, 1}, 0.16f) * Quat::axisAngle({1, 0, 0}, 0.12f);

    switch (z.action) {
        case ZAction::Throw: {
            float k = clampf(z.actionTime / std::max(0.05f, z.windup), 0, 1);
            if (!z.actionDone) setArm(armR, lerpf(-0.4f, -kPi * 1.15f, k), 0.15f, 0.1f);
            else setArm(armR, -kPi * 0.42f, 0.1f, 0);
            break;
        }
        case ZAction::Slam:
            if (!z.actionDone) {
                setArm(armR, -kPi * 0.95f, 0.2f, 0);
                setArm(armL, -kPi * 0.95f, -0.2f, 0);
            } else {
                setArm(armR, -kPi * 0.25f, 0.2f, 0);
                setArm(armL, -kPi * 0.25f, -0.2f, 0);
            }
            break;
        case ZAction::Scream:
            setArm(armR, -kPi * 0.3f, 0, -1.2f);
            setArm(armL, -kPi * 0.3f, 0, 1.2f);
            if (head >= 0) pose.r[head] = Quat::axisAngle({1, 0, 0}, -0.55f);
            break;
        case ZAction::Fuse:
            setArm(armR, -kPi * 0.4f + std::sin(t * 50.0f) * 0.3f, 0, -0.9f);
            setArm(armL, -kPi * 0.4f + std::sin(t * 47.0f) * 0.3f, 0, 0.9f);
            break;
        case ZAction::Dash:
            setArm(armR, -kPi * 0.6f, 0.2f, 0);
            setArm(armL, 0.4f, 0, 0.2f);
            break;
        case ZAction::None:
            if (z.state == ZState::Attack && z.kind == ZombieKind::Knife) {
                float k = clampf(1.0f - z.windup / 0.25f, 0, 1);
                setArm(armR, lerpf(-kPi * 0.95f, -kPi * 0.3f, k * k), 0.25f, 0);
            }
            break;
    }
}

void Game::addZombieVisuals(const Zombie& z, const CharacterDraw& draw) {
    const CharacterModel& m = z.asset->model;
    const int armR = m.nodeIndex("arm-right"), head = m.nodeIndex("head"), torso = m.nodeIndex("torso");
    auto part = [&](const Mat4& frame, const Vec3& c, const Vec3& size, const Vec4& color, float emissive) {
        BoxDraw b;
        b.world = frame * Mat4::translation(c) * Mat4::scale(size);
        b.color = color;
        b.emissive = emissive;
        scene_.boxes.push_back(b);
    };
    if (armR >= 0 && armR < static_cast<int>(draw.nodes.size())) {
        Mat4 hand = draw.world * draw.nodes[static_cast<size_t>(armR)] * Mat4::translation({0.0f, -0.85f, 0.05f}) * Mat4::rotationX(kPi * 0.5f);
        switch (z.kind) {
            case ZombieKind::Knife:
            case ZombieKind::KnifeThrower:
                if (z.itemHidden <= 0 || z.kind == ZombieKind::Knife) {
                    part(hand, {0, 0, 0.08f}, {0.09f, 0.09f, 0.24f}, {0.3f, 0.2f, 0.12f, 1}, 0);
                    part(hand, {0, 0.02f, 0.42f}, {0.03f, 0.13f, 0.46f}, {0.82f, 0.85f, 0.9f, 1}, 0.15f);
                }
                break;
            case ZombieKind::Thrower:
                if (z.itemHidden <= 0) part(hand, {0, 0, 0.14f}, {0.34f, 0.3f, 0.32f}, {0.5f, 0.48f, 0.45f, 1}, 0);
                break;
            case ZombieKind::Molotov:
                if (z.itemHidden <= 0) {
                    part(hand, {0, 0, 0.16f}, {0.15f, 0.15f, 0.28f}, {0.32f, 0.45f, 0.2f, 1}, 0.1f);
                    part(hand, {0, 0, 0.36f}, {0.07f, 0.07f, 0.14f}, {0.85f, 0.8f, 0.7f, 1}, 0);
                    Vec3 rag = hand.transformPoint({0, 0, 0.48f});
                    float f = 0.8f + 0.2f * std::sin(time_ * 19.0f + z.home.x);
                    scene_.particles.push_back({rag, {1.0f, 0.6f, 0.2f, 0.9f}, 0.42f * f, true});
                    scene_.particles.push_back({rag + Vec3(0, 0.12f, 0), {1.0f, 0.85f, 0.4f, 0.8f}, 0.22f * f, true});
                }
                break;
            default:
                break;
        }
    }
    if (z.kind == ZombieKind::Spitter && head >= 0 && head < static_cast<int>(draw.nodes.size())) {
        Vec3 mouth = (draw.world * draw.nodes[static_cast<size_t>(head)]).transformPoint({0, 0.2f, 0.45f});
        scene_.particles.push_back({mouth, {0.55f, 1.0f, 0.25f, 0.7f}, 0.25f + 0.08f * std::sin(time_ * 6.0f), true});
    }
    if (torso >= 0 && torso < static_cast<int>(draw.nodes.size())) {
        Vec3 mn, mx;
        bool hasBounds = nodeBounds(z.asset, torso, mn, mx);
        Mat4 frame = draw.world * draw.nodes[static_cast<size_t>(torso)];
        Vec3 c = hasBounds ? (mn + mx) * 0.5f : Vec3(0, 0.5f, 0);
        Vec3 size = hasBounds ? (mx - mn) : Vec3(0.8f, 0.8f, 0.5f);
        if (z.kind == ZombieKind::Exploder) {
            float pulse = 1.0f + 0.12f * std::sin(time_ * 6.0f + z.home.z);
            part(frame, c + Vec3(size.x * 0.3f, size.y * 0.1f, size.z * 0.5f), Vec3(0.2f, 0.2f, 0.2f) * pulse, {0.55f, 1.0f, 0.3f, 1}, 0.8f);
            part(frame, c + Vec3(-size.x * 0.25f, -size.y * 0.15f, size.z * 0.5f), Vec3(0.16f, 0.16f, 0.16f) * pulse, {0.55f, 1.0f, 0.3f, 1}, 0.8f);
            part(frame, c + Vec3(0.0f, size.y * 0.3f, -size.z * 0.5f), Vec3(0.22f, 0.22f, 0.22f) * pulse, {0.55f, 1.0f, 0.3f, 1}, 0.8f);
        }
        if (z.mutations & MUT_ARMORED) part(frame, c + Vec3(0, size.y * 0.05f, 0), Vec3(size.x * 1.1f, size.y * 0.72f, size.z * 1.16f), {0.42f, 0.44f, 0.48f, 1}, 0);
        if (z.kind == ZombieKind::Brute) {
            part(frame, c + Vec3(size.x * 0.55f, size.y * 0.45f, 0), Vec3(size.x * 0.4f, size.y * 0.18f, size.z * 0.9f), {0.28f, 0.24f, 0.2f, 1}, 0);
            part(frame, c + Vec3(-size.x * 0.55f, size.y * 0.45f, 0), Vec3(size.x * 0.4f, size.y * 0.18f, size.z * 0.9f), {0.28f, 0.24f, 0.2f, 1}, 0);
        }
    }
    if ((z.mutations & MUT_ARMORED) && head >= 0 && head < static_cast<int>(draw.nodes.size())) {
        Vec3 mn, mx;
        if (nodeBounds(z.asset, head, mn, mx)) {
            Vec3 size = mx - mn;
            Vec3 c = (mn + mx) * 0.5f + Vec3(0, size.y * 0.3f, 0);
            part(draw.world * draw.nodes[static_cast<size_t>(head)], c, Vec3(size.x * 1.12f, size.y * 0.45f, size.z * 1.12f), {0.38f, 0.4f, 0.44f, 1}, 0.05f);
        }
    }
    // Aura das variações elite (cor por mutação) orbitando os pés.
    if (z.mutations) {
        Vec4 color(1.0f, 0.85f, 0.3f, 0.6f);
        if (z.mutations & MUT_TOXIC) color = {0.45f, 1.0f, 0.3f, 0.6f};
        else if (z.mutations & MUT_RAGE) color = {1.0f, 0.3f, 0.25f, 0.6f};
        else if (z.mutations & MUT_SWIFT) color = {0.4f, 0.8f, 1.0f, 0.6f};
        else if (z.mutations & MUT_REGEN) color = {0.6f, 1.0f, 0.6f, 0.6f};
        for (int k = 0; k < 3; ++k) {
            float a = time_ * 2.5f + z.home.x + k * kTwoPi / 3.0f;
            Vec3 p = z.body.position + Vec3(std::cos(a) * 0.55f * z.scale, 0.25f + 0.5f * std::fmod(time_ * 0.7f + k * 0.33f, 1.0f), std::sin(a) * 0.55f * z.scale);
            scene_.particles.push_back({p, color, 0.28f, true});
        }
    }
}

}  // namespace zz
