// Autopiloto de depuração: dirige o jogador por uma rota fixa (início, ninhos, portal) com dt fixo, para medir
// desempenho de forma reproduzível. Só é ligado por MissionConfig::perfMode (extras de Intent em build debug).
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "../core/log.h"
#include "game.h"

namespace zz {

void Game::perfBegin() {
    auto& a = autopilot_;
    a = perf::AutopilotState{};
    a.mode = config_.perfMode;
    a.frames = config_.perfFrames > 0 ? config_.perfFrames : 7200;
    std::snprintf(a.label, sizeof(a.label), "auto%d", a.mode);
    if (!island_) return;
    a.route.push_back({island_->start().x, island_->start().z});
    for (int id : island_->nests()) {
        const Vec3 c = island_->poiAtCell(id).center;
        a.route.push_back({c.x, c.z});
    }
    if (island_->portalId() >= 0) {
        const Vec3 c = island_->poiAtCell(island_->portalId()).center;
        a.route.push_back({c.x, c.z});
    }
    if (a.route.size() < 2) return;
    a.leg = 1;
    a.lastPos = player_.position;
    a.active = true;
    noEnemies_ = a.mode == 1;
    prof_.startRun(a.label);
    ZZ_PERF_LOG("run begin label=%s frames=%d route=%zu seed=%u stage=%d radius=%d", a.label, a.frames, a.route.size(), config_.seed,
                config_.stage, streamRadius());
}

void Game::perfDrive(InputState& in) {
    auto& a = autopilot_;
    ++a.frame;
    invulnerable_ = std::max(invulnerable_, 1.0f);  // nada de morte durante o teste
    health_ = maxHealth_;
    // Chão existe em volta do jogador? (deve ser sempre verdadeiro)
    {
        const int px = static_cast<int>(std::floor(player_.position.x)), pz = static_cast<int>(std::floor(player_.position.z));
        bool ready = true;
        for (int dz = -16; dz <= 16 && ready; dz += 16)
            for (int dx = -16; dx <= 16; dx += 16)
                if (world_.columnInBounds(px + dx, pz + dz) && !world_.columnReady(px + dx, pz + dz)) {
                    ready = false;
                    break;
                }
        if (!ready) ++a.starved;
    }
    while (a.leg < a.route.size() && distXZ(player_.position, Vec3(a.route[a.leg].x, 0, a.route[a.leg].y)) < 6.0f) ++a.leg;
    if (a.leg >= a.route.size()) a.leg = 0;  // rota circular
    const Vec2 t = a.route[a.leg];
    const float want = yawFromDirection(t.x - player_.position.x, t.y - player_.position.z);
    camYaw_ = wrapAngle(camYaw_ + clampf(wrapAngle(want - camYaw_), -3.0f / 60.0f, 3.0f / 60.0f));  // vira a 3 rad/s
    camPitch_ = -0.12f;
    in.moveX = 0;
    in.moveY = 1;  // correr para a frente (pior caso de streaming)
    in.lookX = in.lookY = 0;
    in.fire = false;
    in.aim = false;
    in.aimStage = 0;
    if (a.frame % 120 == 0) {  // preso (água/penhasco)? pula o ponto
        if (distXZ(player_.position, a.lastPos) < 2.0f) ++a.leg;
        a.lastPos = player_.position;
    }
    if (a.frame >= a.frames) perfEnd();
}

void Game::perfEnd() {
    auto& a = autopilot_;
    // Espera o streaming assentar (até 120 chamadas) para que o hash do conteúdo seja comparável entre execuções.
    StreamStats st;
    for (int i = 0; i < 120; ++i) {
        st = world_.stream(player_.position.x, player_.position.z, streamRadius(), StreamBudget::bulk());
        if (st.pending == 0) break;
    }
    const unsigned long long hash = world_.residentHash(player_.position.x, player_.position.z, streamRadius());
    char extra[96];
    std::snprintf(extra, sizeof(extra), "starved=%d hash=0x%016llx", a.starved, hash);
    prof_.endRun(extra);
    a.active = false;
    noEnemies_ = false;
}

}  // namespace zz
