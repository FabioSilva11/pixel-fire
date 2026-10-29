// Missão na ilha em streaming: carregamento, população de zumbis, pontos de interesse, ninhos,
// baús, suprimentos por anúncio e minimapas.
#include <algorithm>
#include <cmath>

#include "../core/log.h"
#include "../world/block_textures.h"
#include "game.h"

namespace zz {

using namespace proto;

void Game::startMission(const MissionConfig& config) {
    // Tempos de cada etapa da carga (log "ZZPerf load").
    double loadMark = perf::nowMs();
    const double loadStart = loadMark;
    auto lap = [&]() {
        const double now = perf::nowMs();
        const double d = now - loadMark;
        loadMark = now;
        return static_cast<int>(d + 0.5);
    };
    prof_.skipFrames(3);
    uploadUnlimitedFrames_ = 3;
    streamStats_ = StreamStats{};
    prewarmTimer_ = 0;
    config_ = config;
    mode_ = Mode::Mission;
    paused_ = false;
    theme_ = applyContractTheme(applySkyVariant(themeForStage(config.stage), config.seed, config.stage >= 3), config);
    renderer_.setTheme(theme_);
    {
        std::vector<uint8_t> layers = generateBlockTextures(theme_);
        layerColors_.resize(T_COUNT);
        for (int i = 0; i < T_COUNT; ++i) layerColors_[i] = averageLayerColor(layers, i);
    }
    rng_.reseed(config.seed * 2654435761u + 17u);
    const int loadTex = lap();

    // A ilha nova só substitui a antiga depois que o mundo deixa de apontar para ela.
    // Na sobrevivência a ilha é livre: sem ninhos nem portal, só as ondas de zumbis.
    auto island = std::make_unique<IslandGenerator>(theme_, config.stage, config.seed, config.mode != 1);
    world_.resetStreaming(kIslandSize, kIslandHeight, kIslandSize, island.get());
    island_ = std::move(island);
    fluids_ = {};
    worldTime_ = 0;
    terrainRevision_ = world_.revision();
    layout_ = WorldLayout{};
    layout_.seaLevel = island_->seaLevel();
    layout_.start = island_->start();
    layout_.startYaw = island_->startYaw();
    const int loadIsland = lap();

    // Gera toda a área visível inicial antes de soltar o jogador (modo bulk: sem orçamento de tempo).
    int loadGen = 0, loadMesh = 0;
    for (int i = 0; i < 500; ++i) {
        const StreamStats ss = world_.stream(layout_.start.x, layout_.start.z, streamRadius(), StreamBudget::bulk());
        loadGen += ss.generated;
        loadMesh += ss.meshed;
        if (ss.pending == 0) break;
    }
    const int loadStream = lap();
    layout_.start.y = static_cast<float>(world_.standHeight(static_cast<int>(std::floor(layout_.start.x)), static_cast<int>(std::floor(layout_.start.z))));

    // Objetivos: ninhos de infecção e o portal de extração.
    nests_.clear();
    for (int id : island_->nests()) {
        const IslandPoi& p = island_->poiAtCell(id);
        const PoiStamp& st = island_->stamp(p);
        Nest n;
        n.poi = id;
        n.core = st.nestCore;
        n.maxHealth = n.health = 320.0f + config.stage * 36.0f;
        n.spawnTimer = 2.0f;
        nests_.push_back(n);
    }
    if (island_->portalId() >= 0) {
        const PoiStamp& st = island_->stamp(island_->poiAtCell(island_->portalId()));
        layout_.portalBase = st.portalBase;
        layout_.portalYaw = st.portalYaw;
    }
    const int loadNests = lap();
    navX_ = navZ_ = -1;
    navTimer_ = 0;

    resetPlayerState(layout_.start, layout_.startYaw, config.playerHealth * std::max(1.0f, config.boostHealth), config.magazine);
    playerAsset_ = character(config.skin);
    playerAnim_ = Animator();
    if (playerAsset_) playerAnim_.play(playerAsset_->model.clipIndex("idle"), true, 0);

    loadZombieAssets();
    const int loadChars = lap();
    zombies_.clear();
    pendingSpawns_.clear();
    chests_.clear();
    scatterMapChests();
    visitedPois_.clear();
    announcedPois_.clear();
    supply_ = SupplyDrop{};
    suppliesLeft_ = config.mode == 1 ? 4 : 2;
    supplyPeriod_ = config.mode == 1 ? 120.0f : 150.0f;
    supplyCooldown_ = config.mode == 1 ? 45.0f : 75.0f;
    startSurvivalState();
    startContractState();
    spawnTimer_ = 2.5f;
    poiTimer_ = 0;
    {
        const IslandPoi& sp = island_->poi(static_cast<int>(layout_.start.x) / kIslandCell, static_cast<int>(layout_.start.z) / kIslandCell);
        if (sp.exists) {
            visitedPois_.insert(sp.id);
            announcedPois_.insert(sp.id);
        }
    }
    {
        std::lock_guard<std::mutex> lock(outMutex_);
        events_.clear();
        result_.fill(0);
    }
    lastTime_ = -1;
    lap();
    buildBigMap();
    const int loadBigMap = lap();
    rebuildMinimap();
    const int loadMinimap = lap();
    ZZ_PERF_LOG("load total=%d tex=%d island=%d stream=%d gen=%d mesh=%d nests=%d chars=%d bigmap=%d minimap=%d ms",
                static_cast<int>(perf::nowMs() - loadStart + 0.5), loadTex, loadIsland, loadStream, loadGen, loadMesh, loadNests,
                loadChars, loadBigMap, loadMinimap);
    autopilot_.active = false;
    noEnemies_ = false;
    if (config.perfMode != 0 && config.mode != 1) perfBegin();
    emit(EV_MISSION_READY, config.mode == 1 ? -2 : -1);
    LOGI("Missão %d: ilha %d, %zu chunks, %zu ninhos, rota %.0f m", config.stage, static_cast<int>(config.seed), world_.chunks().size(), nests_.size(), island_->routeLength());
}

namespace {
constexpr float kTargetFrameMs = 16.7f;  // teto de ~60 fps do GameSurfaceView
constexpr float kStreamSafetyMs = 3.0f;  // swap + trabalho Kotlin pós-quadro
constexpr float kStreamMinMs = 1.0f, kStreamMaxMs = 5.0f;
}  // namespace

// Raio de mundo efetivo: nenhum chunk além de (névoa + 10 + 12) é desenhado (Renderer::drawChunks), então não vale
// gerar nem malhar além disso. Em névoa curta (Terra Devastada, noite) o raio cai de 8 para 7 sem mudança visual.
int Game::streamRadius() const {
    const float fogEnd = std::min(theme_.sky.fogEnd, viewRadius_ * 16.0f - 6.0f);  // igual a buildScene
    const int r = static_cast<int>(std::ceil((fogEnd + 22.0f) / 16.0f - 0.5f));
    return std::max(4, std::min(viewRadius_, r));
}

// Orçamento de streaming por TEMPO (1-5 ms), adaptado ao resto do quadro. Faixa urgente em volta do jogador ignora o
// orçamento (chão sob os pés sempre existe); piso de 1 geração + 1 malha garante progresso.
void Game::streamWorld() {
    if (!world_.streaming()) return;
    ZZ_TRACE_SCOPE("ZZ streamWorld");
    perf::Scoped timer(prof_, perf::Sec::Stream);
    StreamBudget b;
#ifdef ZZ_STREAM_LEGACY  // chave de emergência: -DZZ_STREAM_LEGACY volta ao limite antigo (2 gerações + 3 malhas)
    const bool behind = streamStats_.pending > 24;
    b = StreamBudget::legacy(behind ? 4 : 2, behind ? 6 : 3);
#else
    b.maxMs = clampf(kTargetFrameMs - prof_.otherMs() - kStreamSafetyMs, kStreamMinMs, kStreamMaxMs);
    if (renderer_.pendingUploads() > 16) b.maxMesh = 1;  // contrapressão: GPU atrasada, não acumule malhas na CPU
#endif
    streamStats_ = world_.stream(player_.position.x, player_.position.z, streamRadius(), b);
    prof_.count(perf::Cnt::Generated, streamStats_.generated);
    prof_.count(perf::Cnt::Meshed, streamStats_.meshed);
    prof_.count(perf::Cnt::Unloaded, streamStats_.unloaded);
}

bool Game::findSpawnSpot(const Vec3& around, float minDist, float maxDist, bool behindCamera, Vec3& out) {
    for (int attempt = 0; attempt < 14; ++attempt) {
        float ang = behindCamera ? camYaw_ + kPi + rng_.range(-2.0f, 2.0f) : rng_.range(0, kTwoPi);
        float dist = rng_.range(minDist, maxDist);
        Vec3 dir = forwardFromYaw(ang);
        int x = static_cast<int>(std::floor(around.x + dir.x * dist));
        int z = static_cast<int>(std::floor(around.z + dir.z * dist));
        if (!world_.columnMeshed(x, z)) continue;
        int g = world_.groundHeight(x, z), s = world_.standHeight(x, z);
        if (g <= layout_.seaLevel + 1 || s - g > 1) continue;
        if (isWaterBlock(world_.get(x, s, z)) || world_.solid(x, s + 1, z)) continue;
        out = {x + 0.5f, static_cast<float>(s), z + 0.5f};
        return true;
    }
    return false;
}

void Game::updateSpawner(float dt) {
    spawnTimer_ -= dt;
    if (spawnTimer_ > 0) return;
    spawnTimer_ = 1.0f;
    const Vec3 pp = player_.position;
    // Descarta zumbis mortos e os que ficaram muito para trás.
    size_t before = zombies_.size();
    zombies_.erase(std::remove_if(zombies_.begin(), zombies_.end(), [&](const Zombie& z) {
        float d = distXZ(z.body.position, pp);
        if (z.state == ZState::Dead) return z.deathTime > 4.0f;
        if (d > 175.0f) return true;
        return d > 125.0f && !z.alerted && z.nest < 0;
    }), zombies_.end());
    if (zombies_.size() != before) assistTarget_ = -1;
    if (dead_ || victory_) return;

    int nearAlive = 0;
    for (const Zombie& z : zombies_)
        if (z.state != ZState::Dead && distXZ(z.body.position, pp) < 95.0f) ++nearAlive;
    int target = config_.zombieCount + (theme_.sky.night > 0.5f ? 3 : 0) + std::min(6, static_cast<int>(missionTime_ / 120.0f));
    if (nearAlive >= target) return;
    Vec3 base;
    if (!findSpawnSpot(pp, 42.0f, 78.0f, rng_.chance(0.7f), base)) return;
    int group = rng_.rangeInt(1, std::min(4, 1 + config_.stage / 3));
    for (int i = 0; i < group && nearAlive < target + 1; ++i) {
        Vec3 p = base;
        Vec3 q;
        if (i > 0 && findSpawnSpot(base, 1.5f, 5.0f, false, q)) p = q;
        spawnZombie(p, false);
        ++nearAlive;
    }
}

void Game::updatePois(float dt) {
    // Pré-aquece o molde de um POI por vez (0,25 s) antes de o jogador chegar: o molde é função pura de (POI, semente),
    // então o conteúdo não muda, mas o primeiro chunk do POI deixa de pagar o custo inteiro dentro do quadro.
    prewarmTimer_ -= dt;
    if (prewarmTimer_ <= 0 && island_) {
        prewarmTimer_ = 0.25f;
        island_->poisNear(player_.position, 230.0f, prewarmScratch_);
        for (const IslandPoi* p : prewarmScratch_) {
            if (island_->hasStamp(*p)) continue;
            ZZ_TRACE_SCOPE("ZZ prewarm");
            island_->stamp(*p);
            break;
        }
    }
    poiTimer_ -= dt;
    if (poiTimer_ > 0) return;
    poiTimer_ = 0.5f;
    island_->poisNear(player_.position, 92.0f, poiScratch_);
    for (const IslandPoi* p : poiScratch_) {
        float d = distXZ(p->center, player_.position);
        if (!announcedPois_.count(p->id) && d < p->radius + 5.0f) {
            announcedPois_.insert(p->id);
            emit(EV_POI, p->role == ROLE_NEST ? K_NEST : (p->role == ROLE_PORTAL ? K_PORTAL : p->kind));
        }
        if (visitedPois_.count(p->id)) continue;
        if (!world_.columnMeshed(static_cast<int>(p->center.x), static_cast<int>(p->center.z))) continue;
        visitedPois_.insert(p->id);
        if (p->role == ROLE_START) continue;
        Rng prng(hash2(p->cellX, p->cellZ, config_.seed + 99u));
        int guards;
        const bool guarded = !survival();
        if (p->role == ROLE_NEST) guards = 4 + std::min(4, config_.stage / 3);
        else if (p->role == ROLE_PORTAL) guards = 3 + std::min(3, config_.stage / 4);
        else guards = prng.rangeInt(2, 4) + std::min(2, config_.stage / 5);
        const int keep = contractrules::scaledGuards(guards, config_.guardMul);  // contrato: menos guardas
        for (int i = 0; i < (guarded ? guards : 0); ++i) {
            float ang = prng.range(0, kTwoPi), dist = prng.range(1.5f, p->radius + 2.0f);
            if (i >= keep) continue;  // depois dos sorteios: o prng segue igual e os baús do POI não mudam entre contratos
            int x = static_cast<int>(std::floor(p->center.x + std::cos(ang) * dist));
            int z = static_cast<int>(std::floor(p->center.z + std::sin(ang) * dist));
            if (!world_.columnReady(x, z) || world_.solid(x, world_.standHeight(x, z) + 1, z)) continue;
            if (world_.standHeight(x, z) - world_.groundHeight(x, z) > 1) continue;  // nada de zumbi em cima do telhado
            const size_t before = zombies_.size();
            spawnZombie({x + 0.5f, static_cast<float>(world_.standHeight(x, z)), z + 0.5f}, false, -1, p->role != ROLE_NONE && i == 0);
            if (zombies_.size() > before) zombies_.back().home = p->center;  // noEnemies_ não insere nada
        }
        if (p->role == ROLE_NONE) {
            const PoiStamp& st = island_->stamp(*p);
            for (const Vec3& c : st.decor.chests) {
                if (p->kind != K_TOWER && prng.chance(0.3f)) continue;
                Chest ch;
                ch.pos = c;
                ch.diamonds = prng.rangeInt(6, 14) + config_.stage / 2 + (p->kind == K_TOWER ? 8 : 0);
                if (cargo_.placed && length(c - cargo_.pos) < 0.5f) continue;  // a carga ocupa este lugar (depois dos sorteios: o prng segue igual)
                chests_.push_back(ch);
            }
        }
    }
}

void Game::updateNests(float dt) {
    for (size_t i = 0; i < nests_.size(); ++i) {
        Nest& n = nests_[i];
        n.hitFlash = std::max(0.0f, n.hitFlash - dt);
        if (n.destroyed) continue;
        float d = distXZ(n.core, player_.position);
        if (d > 70.0f) continue;
        if (rng_.chance(dt * 10.0f) && particles_.size() < 820) {
            FxParticle p;
            p.position = n.core + Vec3(rng_.range(-1.5f, 1.5f), rng_.range(-1.5f, 0.5f), rng_.range(-1.5f, 1.5f));
            p.velocity = {0, rng_.range(0.6f, 1.6f), 0};
            if (config_.nestBomb > 0.0f) p.color = rng_.chance(0.5f) ? Vec4(1.0f, 0.5f, 0.1f, 0.9f) : Vec4(1.0f, 0.85f, 0.25f, 0.9f);  // ninho instável
            else p.color = rng_.chance(0.5f) ? Vec4(0.75f, 0.3f, 1.0f, 0.9f) : Vec4(0.4f, 1.0f, 0.35f, 0.9f);
            p.size = rng_.range(0.12f, 0.28f);
            p.life = p.maxLife = rng_.range(0.8f, 1.6f);
            p.additive = true;
            particles_.push_back(p);
        }
        if (d > 55.0f || dead_ || victory_) continue;
        n.awake = true;
        n.spawnTimer -= dt;
        int fromNest = 0;
        for (const Zombie& z : zombies_)
            if (z.nest == static_cast<int>(i) && z.state != ZState::Dead) ++fromNest;
        constexpr int kMaxNestMobs = 7;
        if (n.spawnTimer <= 0 && fromNest < kMaxNestMobs) {
            Vec3 pos;
            if (findSpawnSpot(n.core, 2.0f, 7.0f, false, pos)) {
                n.spawnTimer = std::max(1.5f, 2.8f - std::min(config_.stage, 30) * 0.04f);
                // Ninho instável (4C): parte dos filhotes é Inchado. Só consome o rng_ quando o contrato está ativo.
                const bool bomb = config_.nestBomb > 0.0f && rng_.chance(config_.nestBomb);
                const size_t before = zombies_.size();
                spawnZombie(pos, true, static_cast<int>(i), false, bomb ? static_cast<int>(ZombieKind::Exploder) : -1);
                if (zombies_.size() > before) ++n.spawnCount;  // noEnemies_ não insere nada
                if (bomb && zombies_.size() > before) {
                    ++bombs_;
                    if (bombs_ == 1) emit(EV_CONTRACT, 5, static_cast<int>(i));  // aviso só no primeiro: o chip do HUD conta o resto
                    LOGI("Ninho instavel #%d gerou Inchado (total %d, filhote %d do ninho)", static_cast<int>(i), bombs_, n.spawnCount);
                }
                spawnBurst(pos + Vec3(0, 0.5f, 0), bomb ? Vec4(1.0f, 0.5f, 0.1f, 1.0f) : Vec4(0.7f, 0.3f, 1.0f, 1.0f), 14, 3.0f, 0.16f, true, -1.0f);
                emitSound(SND_GROAN, pos, 0.8f);
            } else {
                n.spawnTimer = 0.5f;
            }
        }
    }
}

bool Game::damageNestRay(const Vec3& origin, const Vec3& dir, float maxT, float& tHit, int& nestIndex) {
    nestIndex = -1;
    float best = maxT;
    for (size_t i = 0; i < nests_.size(); ++i) {
        const Nest& n = nests_[i];
        if (n.destroyed) continue;
        Vec3 mn = n.core - Vec3(0.85f, 0.85f, 0.85f), mx = n.core + Vec3(0.85f, 0.85f, 0.85f);
        float tmin = 0, tmax = best;
        bool hit = true;
        for (int a = 0; a < 3 && hit; ++a) {
            if (std::fabs(dir[a]) < 1e-7f) {
                if (origin[a] < mn[a] || origin[a] > mx[a]) hit = false;
                continue;
            }
            float inv = 1.0f / dir[a];
            float t1 = (mn[a] - origin[a]) * inv, t2 = (mx[a] - origin[a]) * inv;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) hit = false;
        }
        if (hit && tmin < best) {
            best = tmin;
            nestIndex = static_cast<int>(i);
        }
    }
    tHit = best;
    return nestIndex >= 0;
}

void Game::damageNest(int index, float amount, const Vec3& hitPoint) {
    Nest& n = nests_[static_cast<size_t>(index)];
    if (n.destroyed) return;
    n.health -= amount;
    n.hitFlash = 0.1f;
    labels_.push_back({hitPoint, static_cast<int>(std::lround(amount)), 1, 0.8f});
    spawnBurst(hitPoint, {0.7f, 0.25f, 0.9f, 1.0f}, 5, 2.4f, 0.12f, true, 6.0f);
    emitSound(SND_HIT, hitPoint, 0.6f);
    alertNear(n.core, 35.0f);
    if (n.health > 0) return;
    n.health = 0;
    n.destroyed = true;
    spawnBurst(n.core, {0.8f, 0.3f, 1.0f, 1.0f}, 60, 9.0f, 0.3f, true, 2.0f);
    spawnBurst(n.core, {0.4f, 1.0f, 0.35f, 1.0f}, 40, 6.0f, 0.25f, true, 3.0f);
    addFlash(n.core, 22.0f, {0.85f, 0.45f, 1.0f}, 0.8f);
    shake_ = std::max(shake_, 0.7f);
    emitSound(SND_EXPLOSION, n.core, 1.0f);
    Pickup p;
    p.type = 0;
    p.crystalIndex = index;
    p.position = n.core;
    p.velocity = {rng_.range(-0.6f, 0.6f), 4.0f, rng_.range(-0.6f, 0.6f)};
    pickups_.push_back(p);
    crystalsSpawnedMask_ |= 1 << index;
    LOGI("OBJ ninho_destruido idx=%d total=%zu t=%.1f", index, nests_.size(), missionTime_);
    emit(EV_NEST_DESTROYED, index);
    revealCargo();
}

// Baús espalhados pelo mapa, fora dos pontos de interesse: cada célula da ilha tem uma pequena chance de
// ter um. Sorteio com semente própria, então não mexe no prng dos POIs (contratos seguem iguais).
void Game::scatterMapChests() {
    constexpr float kMapChestChance = 0.025f;  // ~1 em 40 células
    Rng mapRng(config_.seed * 2654435761u + 911u);
    for (int cz = 0; cz < kIslandCells; ++cz) {
        for (int cx = 0; cx < kIslandCells; ++cx) {
            if (!mapRng.chance(kMapChestChance)) continue;
            const float x = cx * kIslandCell + mapRng.range(16.0f, kIslandCell - 16.0f);
            const float z = cz * kIslandCell + mapRng.range(16.0f, kIslandCell - 16.0f);
            if (distXZ({x, 0, z}, layout_.start) < 90.0f) continue;  // longe do ponto de partida
            if (island_->poi(cx, cz).exists) continue;
            const ColumnInfo col = island_->column(static_cast<int>(x), static_cast<int>(z));
            if (col.water || col.path || col.bridge || col.nearPoi || col.river > 0.5f) continue;
            Chest ch;
            ch.pos = {x + 0.5f, static_cast<float>(col.top), z + 0.5f};
            ch.diamonds = mapRng.rangeInt(3, 8) + config_.stage / 3;
            chests_.push_back(ch);
        }
    }
}

void Game::updateChests(float dt) {
    for (Chest& c : chests_) {
        if (c.opened) {
            c.openAnim += dt;
            continue;
        }
        if (dead_ || length(c.pos + Vec3(0, 0.5f, 0) - (player_.position + Vec3(0, 0.6f, 0))) > 1.7f) continue;
        c.opened = true;
        ++chestsOpened_;
        chestDiamonds_ += c.diamonds;
        // Contrato de munição escassa: o baú repõe a reserva em vez de encher o pente.
        if (ammoLimited()) grantReserve(config_.chestAmmoPct);
        else ammo_ = std::max(ammo_, magazineFor(weaponId()));
        // Itens do baú: um ou dois consumíveis e, raramente, uma arma que o jogador ainda não tem.
        bool grenade = rng_.chance(0.35f);
        if (grenade) {
            int left = addItem(IT_GRENADE, 1);
            if (left > 0) dropItem(c.pos + Vec3(0, 1.0f, 0), IT_GRENADE, left);
        }
        // Cura é rara no baú: kit médico só às vezes e no máximo uma bandagem.
        static const int kChestLoot[] = {IT_BANDAGE, IT_MOLOTOV, IT_GRENADE, IT_SMOKE, IT_ADRENALINE, IT_ANTIDOTE, IT_FIRE_AMMO, IT_ARMOR, IT_MOLOTOV};
        int extra = rng_.chance(0.25f) ? 2 : 1;
        for (int k = 0; k < extra; ++k) dropItem(c.pos + Vec3(0, 1.0f, 0), kChestLoot[rng_.rangeInt(0, 7)], 1);
        if (rng_.chance(0.08f * config_.luck)) dropItem(c.pos + Vec3(0, 1.0f, 0), IT_MEDKIT, 1);
        if (rng_.chance(0.05f * config_.luck)) {
            // A única arma além do fuzil inicial é a escopeta do template.
            if (countItem(IT_SHOTGUN) == 0) dropItem(c.pos + Vec3(0, 1.2f, 0), IT_SHOTGUN, 1);
        }
        emit(EV_CHEST, c.diamonds, grenade ? 1 : 0);
        emitSound(SND_CHEST, c.pos, 1.0f);
        spawnBurst(c.pos + Vec3(0, 0.8f, 0), {1.0f, 0.85f, 0.3f, 1.0f}, 24, 4.0f, 0.14f, true, 4.0f);
        spawnBurst(c.pos + Vec3(0, 0.8f, 0), {0.35f, 0.95f, 1.0f, 1.0f}, 10, 3.0f, 0.12f, true, 4.0f);
        if (rng_.chance(0.15f)) {
            Pickup h;
            h.type = 1;
            h.position = c.pos + Vec3(0, 1.0f, 0);
            h.velocity = {rng_.range(-1.0f, 1.0f), 4.0f, rng_.range(-1.0f, 1.0f)};
            pickups_.push_back(h);
        }
    }
}

void Game::updateSupply(float dt, const InputState& in) {
    if (suppliesLeft_ > 0 && !supply_.active) supplyCooldown_ = std::max(0.0f, supplyCooldown_ - dt);
    if (in.supplyRequests > 0 && suppliesLeft_ > 0 && !supply_.active && !dead_ && !victory_) {
        --suppliesLeft_;
        supplyCooldown_ = supplyPeriod_;
        Vec3 target = player_.position + forwardFromYaw(camYaw_) * 7.0f;
        supply_.active = true;
        supply_.landed = false;
        supply_.age = 0;
        supply_.pos = {target.x, player_.position.y + 32.0f, target.z};
        emit(EV_SUPPLY, 0);
        emitSound(SND_THROW, player_.position, 0.6f);
    }
    if (!supply_.active) return;
    supply_.age += dt;
    supply_.pos.y -= 6.0f * dt;
    if (rng_.chance(dt * 20.0f) && particles_.size() < 820) {
        FxParticle smoke;
        smoke.position = supply_.pos - Vec3(0, 0.4f, 0);
        smoke.velocity = {rng_.range(-0.2f, 0.2f), 0.5f, rng_.range(-0.2f, 0.2f)};
        smoke.color = {0.5f, 1.0f, 0.45f, 0.5f};
        smoke.size = rng_.range(0.5f, 0.8f);
        smoke.life = smoke.maxLife = 1.5f;
        particles_.push_back(smoke);
    }
    int gx = static_cast<int>(std::floor(supply_.pos.x)), gz = static_cast<int>(std::floor(supply_.pos.z));
    float ground = world_.columnReady(gx, gz) ? static_cast<float>(world_.standHeight(gx, gz)) : player_.position.y;
    if (supply_.pos.y <= ground + 0.5f) {
        supply_.active = false;
        Pickup p;
        p.type = 2;
        p.position = {supply_.pos.x, ground + 0.5f, supply_.pos.z};
        pickups_.push_back(p);
        spawnBurst(p.position, {0.75f, 0.68f, 0.55f, 0.9f}, 16, 3.0f, 0.25f, false, 6.0f);
        emitSound(SND_ROCK, p.position, 0.8f);
    }
}

void Game::rebuildMinimap() {
    const int n = kMinimapSize;
    minimap_.assign(static_cast<size_t>(n) * n, 0xFF0B1426u);
    minimapCX_ = static_cast<int>(std::floor(player_.position.x));
    minimapCZ_ = static_cast<int>(std::floor(player_.position.z));
    const int x0 = minimapCX_ - n / 2, z0 = minimapCZ_ - n / 2;
    const int sea = layout_.seaLevel;
    static std::vector<int> prevTops;
    prevTops.assign(static_cast<size_t>(n), sea);
    for (int j = 0; j < n; ++j) {
        int leftTop = sea;
        for (int i = 0; i < n; ++i) {
            int x = x0 + i, z = z0 + j;
            if (!world_.columnReady(x, z)) {
                prevTops[static_cast<size_t>(i)] = sea;
                continue;
            }
            int top = world_.topBlockY(x, z);
            uint8_t b = top >= 0 ? world_.get(x, top, z) : B_AIR;
            uint32_t c = blockInfo(b).mapColor;
            float shade = 0.82f + (top - sea) * 0.012f;
            int ref = (prevTops[static_cast<size_t>(i)] + leftTop) / 2;
            shade += top > ref ? 0.1f : (top < ref ? -0.1f : 0.0f);
            if (isWaterBlock(b)) {
                int depth = sea - world_.groundHeight(x, z);
                shade = clampf(1.0f - depth * 0.05f, 0.55f, 1.0f);
            }
            if (b == B_PATH) c = 0xC4A068;
            shade = clampf(shade, 0.45f, 1.3f);
            uint32_t r = static_cast<uint32_t>(clampf(((c >> 16) & 0xFF) * shade, 0, 255));
            uint32_t g = static_cast<uint32_t>(clampf(((c >> 8) & 0xFF) * shade, 0, 255));
            uint32_t bl = static_cast<uint32_t>(clampf((c & 0xFF) * shade, 0, 255));
            minimap_[static_cast<size_t>(j) * n + i] = 0xFF000000u | (r << 16) | (g << 8) | bl;
            prevTops[static_cast<size_t>(i)] = top;
            leftTop = top;
        }
    }
    minimapTimer_ = 5.0f;
    emit(EV_MINIMAP, minimapCX_, minimapCZ_);
}

void Game::buildBigMap() {
    const int s = kBigMapSize;
    bigMap_.assign(static_cast<size_t>(s) * s, 0xFF000000u);
    const float step = static_cast<float>(kIslandSize) / s;
    for (int j = 0; j < s; ++j)
        for (int i = 0; i < s; ++i) {
            int x = static_cast<int>((i + 0.5f) * step), z = static_cast<int>((j + 0.5f) * step);
            bigMap_[static_cast<size_t>(j) * s + i] = 0xFF000000u | island_->mapColor(x, z);
        }
}

}  // namespace zz
