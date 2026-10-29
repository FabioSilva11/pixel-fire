// Contratos de expedição (etapa 4): céu/visão, reserva de munição, contagem de alertas e HUD do contrato.
#include <algorithm>
#include <cmath>

#include "../core/log.h"
#include "game.h"

namespace zz {

using namespace proto;

namespace {
constexpr int kMaxReservePct = 900;  // teto da reserva: 9 pentes
}  // namespace

// Roda em startMission ANTES de renderer_.setTheme()/generateBlockTextures(). Só mexe no céu/névoa:
// o terreno da ilha não lê theme.sky, então a mesma semente gera a mesma ilha em qualquer contrato.
Theme applyContractTheme(Theme t, const MissionConfig& c) {
    if (c.redSky) {
        t.sky.zenith = {0.16f, 0.015f, 0.03f};
        t.sky.horizon = {0.55f, 0.07f, 0.05f};
        t.sky.fog = {0.34f, 0.04f, 0.04f};
        t.sky.sunColor = {1.0f, 0.22f, 0.15f};
        t.sky.ambient = {0.42f, 0.17f, 0.17f};
        t.sky.sunIntensity = 0.40f;
        t.sky.night = 1.0f;  // estrelas, lanterna do jogador, +3 zumbis e mais Corredores (já existentes)
    }
    if (c.visionMul > 0.0f && c.visionMul != 1.0f) {
        t.sky.fogEnd = std::max(34.0f, t.sky.fogEnd * c.visionMul);
        t.sky.fogStart = std::min(t.sky.fogStart, t.sky.fogEnd * 0.3f);
    }
    return t;
}

void Game::startContractState() {
    alerts_ = 0;
    noAmmoTimer_ = 0;
    contractLogTimer_ = 0;
    bombs_ = 0;
    cargo_ = Cargo{};
    const bool limited = mode_ == Mode::Mission && !survival() && config_.ammoClips > 0.0f;
    reservePct_ = limited ? std::min(kMaxReservePct, static_cast<int>(std::lround(config_.ammoClips * 100.0f))) : -1;
    LOGI("Contrato %d: reserva=%d%% visao=%.2f agressao=%.2f guardas=%.2f ceuVermelho=%d bomba=%.2f carga=%d",
         config_.contractId, reservePct_, config_.visionMul, config_.aggression, config_.guardMul,
         config_.redSky ? 1 : 0, config_.nestBomb, config_.cargo ? 1 : 0);
    placeCargo();
}

// 4D: o ponto da carga depende só de (semente, ilha): varre os pontos de interesse comuns, filtra pela janela de
// distância e começa a busca num índice derivado da semente. O baú escolhido já foi validado pela decoração do POI.
void Game::placeCargo() {
    cargo_ = Cargo{};
    if (!config_.cargo || mode_ != Mode::Mission || survival() || !island_) return;
    std::vector<Vec3> objectives;  // ninhos e portal: a carga fica longe da rota
    for (const Nest& n : nests_) objectives.push_back(n.core);
    if (island_->portalId() >= 0) objectives.push_back(layout_.portalBase);
    for (int tier = 0; tier < 2; ++tier) {
        std::vector<const IslandPoi*> sites;
        for (int id = 0; id < kIslandCells * kIslandCells; ++id) {
            const IslandPoi& p = island_->poiAtCell(id);
            if (!p.exists || p.role != ROLE_NONE) continue;
            float nearest = 1e9f;
            for (const Vec3& o : objectives) nearest = std::min(nearest, distXZ(p.center, o));
            if (contractrules::cargoSiteOk(distXZ(p.center, layout_.start), nearest, tier)) sites.push_back(&p);
        }
        if (sites.empty()) continue;
        const int count = static_cast<int>(sites.size());
        const int first = contractrules::cargoStartIndex(config_.seed, count);
        for (int k = 0; k < count; ++k) {
            const IslandPoi& p = *sites[static_cast<size_t>((first + k) % count)];
            const PoiStamp& st = island_->stamp(p);
            if (st.decor.chests.empty()) continue;
            const int pick = contractrules::cargoStartIndex(config_.seed ^ 0x5A17u, static_cast<int>(st.decor.chests.size()));
            cargo_.pos = st.decor.chests[static_cast<size_t>(pick)];
            cargo_.placed = true;
            LOGI("Carga: POI %d (tipo %d) a %.0f m da largada, plano %c, %d candidatos", p.id, p.kind,
                 distXZ(cargo_.pos, layout_.start), tier == 0 ? 'A' : 'B', count);
            return;
        }
    }
    LOGW("Carga: nenhum ponto valido nesta ilha; o contrato segue sem carga");
}

void Game::revealCargo() {
    if (!cargo_.placed || cargo_.revealed || cargo_.collected) return;
    cargo_.revealed = true;
    emit(EV_CONTRACT, 4);
    LOGI("Carga revelada a %.0f m do jogador", distXZ(cargo_.pos, player_.position));
}

void Game::updateContract(float dt) {
    noAmmoTimer_ = std::max(0.0f, noAmmoTimer_ - dt);
    if (cargo_.placed && !cargo_.collected && !dead_ && !victory_) {
        // Coleta por proximidade (revelada ou não: quem esbarra na caixa a leva).
        const Vec3 d = cargo_.pos + Vec3(0, 0.5f, 0) - (player_.position + Vec3(0, 0.6f, 0));
        if (length(d) < contractrules::kCargoPickupM) {
            cargo_.collected = true;
            cargo_.revealed = true;
            emit(EV_CONTRACT, 3);
            emitSound(SND_CHEST, cargo_.pos, 1.0f);
            spawnBurst(cargo_.pos + Vec3(0, 0.8f, 0), {1.0f, 0.62f, 0.15f, 1.0f}, 30, 4.5f, 0.15f, true, 4.0f);
            spawnBurst(cargo_.pos + Vec3(0, 0.8f, 0), {1.0f, 0.9f, 0.4f, 1.0f}, 12, 3.0f, 0.12f, true, 4.0f);
            LOGI("Carga coletada t=%.1f", missionTime_);
        }
    }
    if (config_.contractId == 0) return;
    contractLogTimer_ -= dt;
    if (contractLogTimer_ <= 0.0f) {  // diagnóstico por logcat
        contractLogTimer_ = 5.0f;
        LOGI("Contrato %d: t=%.0fs alertas=%d reserva=%d%% pente=%d zumbis=%zu", config_.contractId, missionTime_, alerts_, reservePct_, ammo_, zombies_.size());
    }
}

// Único ponto que marca um zumbi como alertado por percepção, tiro, grito ou dano.
// Zumbis gerados já alertados (ninhos, ondas, chamados do Gritador) não passam por aqui e não contam.
void Game::alertZombie(Zombie& z) {
    if (z.alerted) return;
    z.alerted = true;
    z.lostSight = 0;
    if (!z.counted && z.nest < 0 && mode_ == Mode::Mission && !survival()) {
        z.counted = true;
        ++alerts_;
        if (config_.contractId != 0 && alerts_ <= 40) LOGI("Alerta #%d (tipo %d) a %.1f m", alerts_, static_cast<int>(z.kind), distXZ(z.body.position, player_.position));
    }
}

bool Game::canReloadNow(int magazine) const {
    return contractrules::canReload(ammo_, magazine, ammoLimited() ? reservePct_ : -1);
}

void Game::completeReload(int magazine) {
    const int limit = ammoLimited() ? reservePct_ : -1;
    const contractrules::ReloadResult r = contractrules::reload(ammo_, magazine, limit);
    ammo_ = r.ammo;
    if (limit < 0) return;
    const int before = reservePct_;
    reservePct_ = r.reservePct;
    // Aviso de reserva no fim (o pente na mão ainda está cheio); "sem munição" de verdade vem de notifyOutOfAmmo.
    if (reservePct_ == 0 && before > 0) emit(EV_CONTRACT, 2);
    else if (reservePct_ > 0 && reservePct_ <= 100 && before > 100) emit(EV_CONTRACT, 2);
}

void Game::notifyOutOfAmmo() {
    if (noAmmoTimer_ > 0.0f) return;
    noAmmoTimer_ = 4.0f;
    emit(EV_CONTRACT, 1);
}

void Game::grantReserve(int pct) {
    if (reservePct_ < 0 || pct <= 0) return;
    reservePct_ = std::min(kMaxReservePct, reservePct_ + pct);
}

void Game::publishContractHud(std::array<float, H_SIZE>& h) const {
    h[H_CT_ID] = static_cast<float>(config_.contractId);
    h[H_CT_ALERTS] = static_cast<float>(alerts_);
    h[H_CT_BOMBS] = static_cast<float>(bombs_);
    if (cargo_.placed) {
        const bool shown = cargo_.revealed && !cargo_.collected;
        h[H_CT_CARGO_STATE] = cargo_.collected ? 3.0f : (cargo_.revealed ? 2.0f : 1.0f);
        if (shown) {
            float sx = 0, sy = 0;
            const bool on = project(cargo_.pos + Vec3(0, 1.2f, 0), sx, sy);
            h[H_CT_CARGO_X] = cargo_.pos.x;
            h[H_CT_CARGO_Z] = cargo_.pos.z;
            h[H_CT_CARGO_SX] = sx;
            h[H_CT_CARGO_SY] = sy;
            h[H_CT_CARGO_ON] = on ? 1.0f : 0.0f;
        } else if (!cargo_.collected) {
            // Oculta: só a dica de distância (m, arredondada a 100) viaja em H_CT_CARGO_SX; posição NÃO é publicada.
            h[H_CT_CARGO_SX] = static_cast<float>(contractrules::cargoHintMeters(distXZ(cargo_.pos, player_.position)));
        }
    }
    const bool limited = reservePct_ >= 0;
    h[H_CT_RESERVE_CLIPS] = limited ? reservePct_ / 100.0f : -1.0f;
    h[H_CT_RESERVE_ROUNDS] = limited ? static_cast<float>(contractrules::reserveRounds(reservePct_, magazineFor(weaponId()))) : -1.0f;
}

}  // namespace zz
