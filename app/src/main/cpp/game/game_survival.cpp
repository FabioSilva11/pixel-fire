// Modo sobrevivência: ilha aleatória sem objetivos e zumbis chegando em ondas (estilo Plants vs Zombies):
// contagem regressiva, ondas cada vez maiores vindo de 1 a 3 frentes, uma GRANDE HORDA a cada 5 ondas,
// intervalo com recompensa entre elas. A pontuação vai para o ranking no Kotlin.
#include <algorithm>
#include <cmath>

#include "game.h"

namespace zz {

using namespace proto;

namespace {
constexpr float kFirstWaveDelay = 25.0f;
constexpr float kIntermission = 15.0f;
constexpr int kMaxAliveCap = 26;
}  // namespace

void Game::startSurvivalState() {
    wave_ = 0;
    waveState_ = 0;
    waveTimer_ = survival() ? kFirstWaveDelay : 0.0f;
    waveTotal_ = 0;
    waveSpawned_ = 0;
    waveKilledBase_ = 0;
    waveSpawnTimer_ = 0;
    waveBig_ = false;
    frontCount_ = 1;
    for (float& a : frontAngles_) a = 0;
    survivalTime_ = 0;
    score_ = 0;
    if (survival()) {
        config_.stage = 1;
        emit(EV_WAVE, 1, 0, 0);
    }
}

void Game::startWave() {
    ++wave_;
    waveState_ = 1;
    waveTimer_ = 0;
    waveBig_ = wave_ % 5 == 0;
    // Cada onda traz mais zumbis; as grandes hordas quase dobram.
    waveTotal_ = std::min(140, 6 + wave_ * 3 + (waveBig_ ? 8 + wave_ * 2 : 0));
    waveSpawned_ = 0;
    waveKilledBase_ = kills_;
    waveSpawnTimer_ = 0.5f;
    // Novos tipos de zumbi e mutações aparecem conforme as ondas avançam.
    config_.stage = std::max(1, wave_);
    frontCount_ = std::min(3, 1 + wave_ / 4 + (waveBig_ ? 1 : 0));
    float base = rng_.range(0, kTwoPi);
    for (int i = 0; i < 3; ++i) frontAngles_[i] = wrapAngle(base + i * (kTwoPi / 3.0f) + rng_.range(-0.4f, 0.4f));
    emit(EV_WAVE, wave_, waveBig_ ? 1 : 0, 1);
    emitSound(SND_WAVE, camPos_, 1.0f);
}

void Game::spawnWaveZombie() {
    const Vec3 pp = player_.position;
    Vec3 pos;
    bool found = false;
    int front = rng_.rangeInt(0, frontCount_ - 1);
    for (int attempt = 0; attempt < 10 && !found; ++attempt) {
        float ang = frontAngles_[front] + rng_.range(-0.35f, 0.35f);
        float dist = rng_.range(36.0f, 52.0f);
        Vec3 dir = forwardFromYaw(ang);
        int x = static_cast<int>(std::floor(pp.x + dir.x * dist));
        int z = static_cast<int>(std::floor(pp.z + dir.z * dist));
        if (!world_.columnMeshed(x, z)) continue;
        int g = world_.groundHeight(x, z), s = world_.standHeight(x, z);
        if (g <= layout_.seaLevel + 1 || s - g > 1) continue;
        if (isWaterBlock(world_.get(x, s, z)) || world_.solid(x, s + 1, z)) continue;
        pos = {x + 0.5f, static_cast<float>(s), z + 0.5f};
        found = true;
    }
    if (!found) found = findSpawnSpot(pp, 24.0f, 70.0f, false, pos);
    ++waveSpawned_;  // mesmo sem lugar (jogador no mar): a onda não pode travar
    if (!found) {
        --waveKilledBase_;  // conta como abatido para a barra de progresso
        return;
    }
    // Nas grandes hordas um a cada cinco é elite.
    bool elite = waveBig_ && waveSpawned_ % 5 == 0;
    spawnZombie(pos, true, -1, elite);
    Zombie& z = zombies_.back();
    const float w = static_cast<float>(wave_ - 1);
    float hp = 1.0f + 0.13f * w + 0.004f * w * w;
    z.maxHealth *= hp;
    z.health = z.maxHealth;
    z.damage *= 1.0f + 0.07f * w;
    z.speed *= std::min(1.45f, 1.0f + 0.025f * w);
    z.home = pp;
    spawnBurst(pos + Vec3(0, 0.6f, 0), {0.45f, 0.25f, 0.2f, 0.8f}, 10, 2.5f, 0.2f, false, 6.0f);
}

void Game::updateWaves(float dt) {
    // Limpeza: corpos somem e zumbis muito para trás são trazidos de volta para a frente da horda.
    size_t before = zombies_.size();
    zombies_.erase(std::remove_if(zombies_.begin(), zombies_.end(), [](const Zombie& z) { return z.state == ZState::Dead && z.deathTime > 4.0f; }), zombies_.end());
    if (zombies_.size() != before) assistTarget_ = -1;
    int alive = 0;
    for (Zombie& z : zombies_) {
        if (z.state == ZState::Dead) continue;
        ++alive;
        if (distXZ(z.body.position, player_.position) > 110.0f) {
            Vec3 p;
            if (findSpawnSpot(player_.position, 34.0f, 50.0f, true, p)) {
                z.body.position = p;
                z.body.velocity = {};
                z.alerted = true;
                z.state = ZState::Chase;
            }
        }
    }
    if (dead_) return;

    if (waveState_ != 1) {
        waveTimer_ -= dt;
        if (waveTimer_ <= 0) startWave();
        return;
    }
    waveTimer_ += dt;
    waveSpawnTimer_ -= dt;
    int maxAlive = std::min(kMaxAliveCap, 8 + wave_ * 2);
    if (waveSpawned_ < waveTotal_ && waveSpawnTimer_ <= 0 && alive < maxAlive) {
        // Grande horda: no fim da onda os zumbis chegam em rajadas.
        bool surge = waveBig_ && waveSpawned_ > waveTotal_ * 0.6f;
        int burst = surge ? 3 : rng_.rangeInt(1, 2);
        for (int i = 0; i < burst && waveSpawned_ < waveTotal_; ++i) spawnWaveZombie();
        waveSpawnTimer_ = std::max(0.35f, 2.2f - wave_ * 0.08f) * (waveBig_ ? 0.6f : 1.0f);
        if (surge) emitSound(SND_GROAN, camPos_, 0.7f);
    }
    if (waveSpawned_ >= waveTotal_ && alive == 0 && pendingSpawns_.empty()) {
        // Onda vencida: bônus, um pouco de vida e uma caixa de itens perto do jogador.
        waveState_ = 2;
        waveTimer_ = kIntermission;
        score_ += 100 * wave_ + (waveBig_ ? 250 : 0);
        health_ = std::min(maxHealth_, health_ + maxHealth_ * 0.08f);
        emit(EV_WAVE, wave_, waveBig_ ? 1 : 0, 2);
        emitSound(SND_CHEST, player_.position, 1.0f);
        static const int kReward[] = {IT_BANDAGE, IT_GRENADE, IT_GRENADE, IT_MOLOTOV, IT_SMOKE, IT_ADRENALINE, IT_FIRE_AMMO, IT_ARMOR, IT_ANTIDOTE};
        Vec3 at = player_.position + forwardFromYaw(camYaw_) * 2.5f + Vec3(0, 1.5f, 0);
        dropItem(at, kReward[rng_.rangeInt(0, 7)], 1);
        if (waveBig_) {
            dropItem(at, IT_MEDKIT, 1);
            dropItem(at, IT_GRENADE, 2);
        }
    }
}

}  // namespace zz
