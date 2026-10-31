#include "game.h"
#include "battle_royale.h"
#include "../core/platform.h"
#include "../core/log.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
namespace zz {
using namespace proto;
namespace {
Vec3 direction(float y, float p) {
    float c = std::cos(p);
    return {std::sin(y) * c, std::sin(p), -std::cos(y) * c};
}
bool rayBox(const Vec3 &o, const Vec3 &d, const Vec3 &mn, const Vec3 &mx, float &outT) {
    float tmin = 0.f, tmax = 1e9f;
    for (int i = 0; i < 3; ++i) {
        float oi = (&o.x)[i], di = (&d.x)[i], bmin = (&mn.x)[i], bmax = (&mx.x)[i];
        if (std::fabs(di) < 1e-6f) {
            if (oi < bmin || oi > bmax)
                return false;
        } else {
            float t1 = (bmin - oi) / di, t2 = (bmax - oi) / di;
            if (t1 > t2)
                std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax)
                return false;
        }
    }
    outT = tmin;
    return true;
}
int weaponSoundId(int gun) {
    if (gun == br::KNIFE)
        return 18;
    if (gun == br::SHOTGUN)
        return 24;
    return 0;
}
Vec3 weaponTracerColor(int gun) {
    if (gun == br::SHOTGUN)
        return {1.f, .80f, .50f};
    if (gun == br::SNIPER)
        return {1.f, .95f, .75f};
    return {1.f, .90f, .60f};
}
} // namespace
Game::Game() {
    for (size_t i = 0; i < voxelItems_.size(); ++i)
        buildVoxelItem(VoxelItem(i), voxelItems_[i]);
}
void Game::onSurfaceCreated() {
    renderer_.onSurfaceCreated();
    renderer_.setTheme(theme_);
    if (battle_ && ready_) {
        loading_ = true;
        ready_ = false;
    }
}
void Game::onSurfaceChanged(int w, int h) { renderer_.onSurfaceChanged(w, h); }
CharacterAsset *Game::character(uint32_t look) {
    auto &a = characters_[look];
    if (!a) {
        a = std::make_unique<CharacterAsset>();
        buildBlockyCharacter(look, *a);
    }
    return a.get();
}
void Game::emit(int e, int a, int b, int c) {
    std::lock_guard<std::mutex> l(outMutex_);
    if (events_.size() < 4000)
        events_.insert(events_.end(), {e, a, b, c});
}
void Game::emitSoundAt(int sound, const Vec3 &pos, float volume) {
    Vec3 lp = visualPlayerPos();
    Vec3 d = pos - lp;
    float dist = length(d);
    if (dist > 65.f)
        return;
    float att = clampf(1.f - dist / 65.f, 0.f, 1.f);
    int vol = int(std::lround(clampf(volume * att * att, 0.f, 1.f) * 100.f));
    if (vol <= 1)
        return;
    Vec3 right = rightFromYaw(camYaw_);
    float pan = dist > .5f ? clampf(dot(d / dist, right), -1.f, 1.f) : 0.f;
    emit(EV_SFX, sound, vol, int(std::lround(pan * 100.f)));
}
Vec3 Game::visualPlayerPos() const {
    if (battle_ && !snapshot_.empty() && self_[11] == br::IN_PLANE && planePosInit_)
        return visualPlanePos_;
    return visualPlayer_.position + correction_ + Vec3(0, visualStep_ - visualPlayer_.stepOffset, 0);
}
void Game::enterLobby(uint32_t look, int screen) {
    lobbyScreen_ = screen;
    lobbyAnim_ = Animator();
    lobbyEmoteTimer_ = 3;
    lobbyEmoteCount_ = 0;
    aimBlend_ = crouchBlend_ = weaponKick_ = sprintFov_ = recoil_ = shake_ = visualStep_ = 0;
    fireCooldown_ = shotFeedback_ = muzzleTimer_ = hitMark_ = emptyClick_ = damageDirTimer_ = 0;
    targetLocked_ = firingNow_ = headshotMark_ = false;
    assistTarget_ = -1;
    throwAiming_ = false;
    battle_ = loading_ = ready_ = false;
    localId_ = -1;
    actors_.clear();
    snapshot_.clear();
    effects_.clear();
    walls_.clear();
    pending_.clear();
    outgoing_.clear();
    tracers_.clear();
    bullets_.clear();
    particles_.clear();
    labels_.clear();
    source_.reset();
    for (auto &a : characters_)
        renderer_.releaseCharacter(*a.second);
    characters_.clear();
    theme_ = lobbyTheme();
    renderer_.setTheme(theme_);
    layout_ = generateLobbyWorld(world_, theme_);
    world_.rebuildDirty(1 << 20);
    player_ = {};
    player_.position = layout_.start;
    visualPlayer_ = player_;
    look_ = look;
    lobbyAnim_.play(character(look)->model.clipIndex("idle"), true, 0);
    camYaw_ = 0;
    camPitch_ = -.12f;
    camPivotInit_ = false;
    lastFrame_ = 0;
    correction_ = {};
    setPaused(false);
    hud_.fill(0);
}
uint32_t Game::battleMapHash() const { return source_ ? source_->map.hash() : 0; }
bool Game::prepareBattle(int id, uint32_t look, uint32_t sequence, bool resume) {
    if (id < 0 || id >= 50 || sequence > 16777215)
        return false;
    if (resume && battle_ && source_ && localId_ == id) {
        sequence_ = std::max(sequence_, sequence);
        pending_.clear();
        outgoing_.clear();
        if (ready_)
            emit(100);
        else
            loading_ = true;
        return true;
    }
    try {
        auto source = std::make_unique<CompetitiveMapSource>();
        std::vector<uint8_t> bytes;
        if (!platform::readAsset("terra-escarlate.zzmap", bytes))
            throw std::runtime_error("Map asset missing");
        std::string serialized(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        bytes.clear();
        bytes.shrink_to_fit();
        std::istringstream in(std::move(serialized), std::ios::binary);
        if (!source->map.load(in))
            throw std::runtime_error("Map asset invalid");
        source_ = std::move(source);
        source_->transferToClient = true;
        world_.beginFinitePreload(br::kMapSize, br::kMapHeight, br::kMapSize, source_.get());
        mapImage_.resize(256 * 256);
        for (int z = 0; z < 256; ++z)
            for (int x = 0; x < 256; ++x)
                mapImage_[z * 256 + x] = 0xff000000 | source_->map.mapColor(x * 1440 / 256, z * 1440 / 256);
        battle_ = true;
        loading_ = true;
        ready_ = false;
        expandedReleased_ = false;
        localId_ = id;
        look_ = look;
        sequence_ = sequence;
        pending_.clear();
        outgoing_.clear();
        actors_.clear();
        snapshot_.clear();
        effects_.clear();
        walls_.clear();
        tracers_.clear();
        bullets_.clear();
        particles_.clear();
        labels_.clear();
        lastSnapshot_ = -1;
        self_.fill(0);
        self_[11] = 0;
        player_ = {};
        player_.position = {720, 220, 720};
        visualPlayer_ = player_;
        visualStep_ = 0;
        motion_ = {};
        ackMotion_ = {};
        correction_ = {};
        camPivotInit_ = false;
        planePosInit_ = false;
        zoneInit_ = false;
        visualZoneRadius_ = 1050.f;
        visualZoneCenter_ = {720, 0, 720};
        weaponInitialLoaded_.fill(false);
        planeVel_ = {};
        planeSnapAge_ = 0;
        inputTime_ = 0;
        dead_ = false;
        fireCooldown_ = shotFeedback_ = muzzleTimer_ = recoil_ = shake_ = hitMark_ = emptyClick_ = 0;
        damageDirTimer_ = 0;
        targetLocked_ = firingNow_ = headshotMark_ = false;
        assistTarget_ = -1;
        lastWeaponId_ = 0;
        localAmmo_ = 0;
        lastReloadState_ = 0;
        rackPlayed_ = true;
        theme_ = applySkyVariant(themeForStage(1), br::kMapSeed, false);
        renderer_.setTheme(theme_);
        return true;
    } catch (const std::exception &e) {
        LOGE("Battle loading failed: %s", e.what());
        emit(101);
        return false;
    }
}
bool Game::CollisionQuery::boxCollides(const Vec3 &a, const Vec3 &b) const {
    if (a.x < 1 || a.z < 1 || b.x > 1439 || b.z > 1439 || game.world_.boxCollides(a, b))
        return true;
    for (const auto &w : game.walls_) {
        if (w[6] <= 0 || w[7] <= 0 || b.y <= w[3] || a.y >= w[3] + 2.7f)
            continue;
        Vec3 mid = (a + b) * .5f;
        float hx = (b.x - a.x) * .5f, hz = (b.z - a.z) * .5f, c = std::cos(w[5]), s = std::sin(w[5]),
              dx = mid.x - w[2], dz = mid.z - w[4];
        if (std::abs(dx) > hx + 2.1f * std::abs(c) + .35f * std::abs(s) ||
            std::abs(dz) > hz + 2.1f * std::abs(s) + .35f * std::abs(c))
            continue;
        if (std::abs(dx * c - dz * s) <= 2.1f + hx * std::abs(c) + hz * std::abs(s) &&
            std::abs(dx * s + dz * c) <= .35f + hx * std::abs(s) + hz * std::abs(c))
            return true;
    }
    return false;
}
void Game::predict(const std::array<float, 12> &c) {
    if (!ready_ || (int(self_[3]) & 1) == 0 || snapshot_.empty() || snapshot_[1] == br::WAITING ||
        snapshot_[1] == br::FINISHED)
        return;
    auto q = CollisionQuery{*this};
    const int buttons = int(c[5]);
    if (predictedFlight_ == br::IN_PLANE) {
        if (buttons & br::JUMP) {
            predictedFlight_ = br::FREEFALL;
            if (planePosInit_)
                player_.position = visualPlanePos_;
            player_.velocity = {0, -6, 0};
        }
        return;
    }
    if (predictedFlight_ != br::LANDED) {
        float altitude =
            player_.position.y - source_->map.heightAt(int(player_.position.x), int(player_.position.z));
        mission::stepDescent(q, player_, predictedFlight_, c[1], c[2], c[3], buttons & br::PARACHUTE,
                             altitude, .05f);
        player_.stepOffset = 0;
        return;
    }
    mission::stepGroundMovement(q, player_, motion_, c[1], c[2], c[3],
                                (buttons & br::SPRINT) &&
                                    !(buttons & (br::FIRE | br::MELEE | br::HEAL | br::REVIVE)),
                                buttons & br::CROUCH, buttons & br::JUMP, int(self_[3]) & 2, .05f);
}
void Game::applyBattleSnapshot(const float *d, size_t n) {
    if (!battle_ || !br::validSnapshot(d, n) || d[2] < lastSnapshot_)
        return;
    const bool enteringFlight = (snapshot_.empty() || snapshot_[1] == br::WAITING) && d[1] == br::PLANE;
    const Vec3 snapPlane(d[13], d[14], d[15]);
    if (!snapshot_.empty() && d[1] == snapshot_[1]) {
        const float dtPhase = d[22] - snapshot_[22];
        const Vec3 travel(snapPlane.x - snapshot_[13], 0.f, snapPlane.z - snapshot_[15]);
        if (dtPhase > .01f && dtPhase < 2.f && lengthXZ(travel) > .01f) {
            const Vec3 measuredVel = travel / dtPhase;
            planeYaw_ = yawFromDirection(travel.x, travel.z);
            planeVel_ = lengthXZ(planeVel_) > 5.f ? lerp(planeVel_, measuredVel, .45f) : measuredVel;
        }
    }
    if (enteringFlight || !planePosInit_) {
        const Vec3 toCenter(720.f - snapPlane.x, 0.f, 720.f - snapPlane.z);
        const float maxComp = std::max(std::fabs(toCenter.x), std::fabs(toCenter.z));
        if (d[22] < 15.f && maxComp > 1.f) {
            const Vec3 halfSpan = toCenter * (680.f / maxComp);
            planeYaw_ = yawFromDirection(halfSpan.x, halfSpan.z);
            const float speed = lengthXZ(planeVel_) > 5.f ? lengthXZ(planeVel_) : (lengthXZ(halfSpan) * 2.f / 50.f);
            planeVel_ = normalize(halfSpan) * speed;
        }
        visualPlanePos_ = snapPlane;
        planePosInit_ = true;
    } else if (d[1] == br::PLANE && lengthXZ(snapPlane - visualPlanePos_) > 35.f) {
        visualPlanePos_ = snapPlane;
    }
    serverPlanePos_ = snapPlane;
    planeSnapAge_ = 0.f;
    if (enteringFlight) {
        camYaw_ = planeYaw_;
        camPitch_ = -.35f;
        camDistance_ = 25.f;
        camPivotInit_ = false;
    }
    lastSnapshot_ = d[2];
    snapshot_.assign(d, d + n);
    int count = int(d[16]);
    size_t wallAt = 24 + count * 32 + int(d[17]) * 7;
    walls_.resize(int(d[18]));
    for (size_t i = 0; i < walls_.size(); ++i)
        std::copy_n(d + wallAt + i * 10, 10, walls_[i].begin());
    std::map<int, bool> present;
    for (int i = 0; i < count; ++i) {
        const float *p = d + 24 + i * 32;
        int id = int(p[0]);
        present[id] = true;
        auto [it, added] = actors_.try_emplace(id);
        Actor &a = it->second;
        a.from = added ? Vec3(p[4], p[5], p[6]) : a.position;
        a.to = {p[4], p[5], p[6]};
        a.sample = time_;
        if (added) {
            a.position = a.to;
            a.muzzle = a.to + Vec3(0, 1.35f, 0);
        }
        const int previousGun = int(a.state[12]), previousShots = int(a.state[31]);
        const float previousHp = a.state[9];
        if (!added && p[22] > .01f && a.state[22] <= .01f && int(p[31]) == previousShots && a.meleeTime < 0)
            a.meleeTime = 0;
        if (!added && previousGun != int(p[12]))
            a.swapTime = .35f;
        if (!added && id != localId_ && p[9] < previousHp - .1f) {
            float dmg = previousHp - p[9];
            a.hitFlash = .18f;
            a.barTimer = 3.5f;
            Vec3 hitPos = a.position + Vec3(0, 1.2f, 0);
            spawnBurst(hitPos, {.9f, .18f, .15f, 1.f}, 8, 2.8f, .1f, false, 12.f);
            if (shotFeedback_ > 0 || hitMark_ > 0 || assistTarget_ == id) {
                bool head = dmg >= br::weaponSpec(int(self_[12])).damage * 1.35f;
                if (hitMark_ <= .02f) {
                    hitMark_ = .18f;
                    headshotMark_ = head;
                    emit(EV_SFX, head ? 3 : 2, 85, 0);
                }
                if (labels_.empty() || length(labels_.back().position - hitPos) > 1.2f)
                    labels_.push_back({hitPos + Vec3(0, .45f, 0), int(std::lround(dmg)), head ? 1 : 0, .85f});
            }
        }
        if (!added && previousShots != int(p[31])) {
            a.kick = 1;
            int gun = int(p[12]);
            if (gun == br::KNIFE) {
                a.meleeTime = 0;
                if (id != localId_)
                    emitSoundAt(18, a.position, .65f);
            } else if (id != localId_) {
                Vec3 dir = direction(p[7], p[8]);
                Vec3 origin = a.position + Vec3(0, (int(p[3]) & 16) ? 1.05f : 1.5f, 0);
                Vec3 muz = length(a.muzzle - a.position) < 3.f ? a.muzzle : origin + dir * .6f;
                spawnMuzzleFx(muz, dir, gun, false);
                fireVisualShot(origin, muz, dir, gun, id, false);
                emitSoundAt(weaponSoundId(gun), muz, .8f);
            }
        }
        std::copy_n(p, 32, a.state.begin());
        if (id != localId_)
            continue;
        Vec3 old = player_.position + correction_;
        float oldHp = self_[9];
        int oldShot = int(self_[31]);
        std::copy_n(p, 32, self_.begin());
        uint32_t ack = uint32_t(p[18]);
        while (!pending_.empty() && pending_.front().command[0] <= ack) {
            ackMotion_ = pending_.front().motionAfter;
            pending_.pop_front();
        }
        sequence_ = std::max(sequence_, ack);
        predictedFlight_ = int(p[11]);
        player_.position = (predictedFlight_ == br::IN_PLANE && planePosInit_) ? visualPlanePos_ : a.to;
        player_.velocity = {p[27], p[28], p[29]};
        player_.height = (int(p[3]) & 2) ? .55f : ((int(p[3]) & 16) ? 1.2f : 1.75f);
        CollisionQuery q{*this};
        player_.onGround = q.boxCollides(player_.position + Vec3(-.2999f, -.05f, -.2999f),
                                         player_.position + Vec3(.2999f, .001f, .2999f));
        player_.inWater = world_.isWaterAt(player_.position + Vec3(0, .5f, 0));
        motion_ = ackMotion_;
        for (auto &pending : pending_) {
            predict(pending.command);
            pending.motionAfter = motion_;
        }
        player_.stepOffset = 0;
        if (length(old - player_.position) > 3.f) {
            motion_ = {};
            correction_ = {};
            visualStep_ = 0;
        } else {
            correction_ = old - player_.position;
        }
        dead_ = (int(p[3]) & 1) == 0;
        if (oldHp > p[9]) {
            float lost = oldHp - p[9];
            emit(EV_PLAYER_HURT, int(lost));
            emit(EV_SFX, 6, 90, 0);
            shake_ = std::max(shake_, std::min(1.f, .25f + lost * .025f));
            Vec3 attackerPos = player_.position + forwardFromYaw(camYaw_);
            float bestScore = 1e9f;
            for (const auto &[aid, other] : actors_) {
                if (aid == localId_ || (int(other.state[3]) & 1) == 0 || other.state[1] == self_[1])
                    continue;
                float dist = length(other.position - player_.position);
                float score = dist - (other.state[22] > .01f ? 80.f : 0.f);
                if (score < bestScore) {
                    bestScore = score;
                    attackerPos = other.position;
                }
            }
            Vec3 dAtt = attackerPos - player_.position;
            if (lengthXZ(dAtt) > .1f) {
                damageDir_ = wrapAngle(yawFromDirection(dAtt.x, dAtt.z) - camYaw_);
                damageDirTimer_ = 1.2f;
            }
        }
        if (int(p[31]) != oldShot && shotFeedback_ <= 0) {
            weaponKick_ = 1;
            emit(EV_SFX, weaponSoundId(int(self_[12])), 75, 0);
        }
    }
    for (auto it = actors_.begin(); it != actors_.end();)
        if (!present.count(it->first))
            it = actors_.erase(it);
        else
            ++it;
}
void Game::applyBattleEffects(const float *data, size_t count) {
    if (!battle_ || count < 2 || count > 602 || data[1] < 0 || data[1] > 100 ||
        std::floor(data[1]) != data[1] || count != size_t(2 + int(data[1]) * 6))
        return;
    for (size_t i = 0; i < count; ++i)
        if (!std::isfinite(data[i]))
            return;
    for (size_t i = 2; i < count; i += 6)
        if (data[i] < 0 || data[i] > 49 || (data[i + 1] != 25 && data[i + 1] != 26) || data[i + 2] < 0 ||
            data[i + 2] > 1440 || data[i + 3] < -10 || data[i + 3] > 300 || data[i + 4] < 0 ||
            data[i + 4] > 1440 || data[i + 5] < 0 || data[i + 5] > 3.1f)
            return;
    if (!effects_.empty() && data[0] < effects_[0])
        return;
    effects_.assign(data, data + count);
}
bool Game::smokeObscures(const Vec3 &from, const Vec3 &to) const {
    if (snapshot_.size() < 24)
        return false;
    size_t at = 24 + size_t(int(snapshot_[16])) * 32 + size_t(int(snapshot_[17])) * 7 +
                size_t(int(snapshot_[18])) * 10 + size_t(int(snapshot_[19])) * 7;
    int count = int(snapshot_[20]);
    Vec3 seg = to - from;
    float segSq = dot(seg, seg);
    for (int i = 0; i < count && at + 4 < snapshot_.size(); ++i, at += 5) {
        if (snapshot_[at + 3] <= 0)
            continue;
        Vec3 center(snapshot_[at], snapshot_[at + 1] + 2.f, snapshot_[at + 2]);
        float t = segSq > 1e-5f ? clampf(dot(center - from, seg) / segSq, 0.f, 1.f) : 0.f;
        Vec3 delta = (from + seg * t) - center;
        float r = snapshot_[at + 4] * .92f;
        if (dot(delta, delta) < r * r)
            return true;
    }
    return false;
}
Game::ShotTrace Game::traceShot(const Vec3 &origin, const Vec3 &dir, float maxT, float minT,
                                int ignoreId) const {
    ShotTrace best;
    best.t = maxT;
    RayHit wh;
    if (world_.raycast(origin, dir, maxT, wh) && wh.distance > minT) {
        best.kind = ShotTrace::World;
        best.t = wh.distance;
        best.world = wh;
        maxT = wh.distance;
    }
    for (const auto &w : walls_) {
        if (w[6] <= 0 || w[7] <= 0)
            continue;
        float c = std::cos(-w[5]), s = std::sin(-w[5]);
        Vec3 rel = origin - Vec3(w[2], w[3], w[4]);
        Vec3 lo(rel.x * c - rel.z * s, rel.y, rel.x * s + rel.z * c);
        Vec3 ld(dir.x * c - dir.z * s, dir.y, dir.x * s + dir.z * c);
        float t = 0;
        if (rayBox(lo, ld, {-2.1f, 0.f, -.35f}, {2.1f, 2.7f, .35f}, t) && t > minT && t < maxT) {
            maxT = t;
            best.kind = ShotTrace::Wall;
            best.t = t;
        }
    }
    for (const auto &[id, a] : actors_) {
        if (id == ignoreId || (int(a.state[3]) & 1) == 0 || int(a.state[11]) == br::IN_PLANE)
            continue;
        if (ignoreId == localId_ && !snapshot_.empty() && a.state[1] == self_[1])
            continue;
        float h = (int(a.state[3]) & 2) ? .85f : ((int(a.state[3]) & 16) ? 1.25f : 1.75f);
        float hw = .34f;
        Vec3 p = a.position;
        float t = 0;
        if (rayBox(origin, dir, p + Vec3(-hw * .85f, h * .68f, -hw * .85f),
                   p + Vec3(hw * .85f, h + .08f, hw * .85f), t) &&
            t > minT && t < maxT && !smokeObscures(origin, p + Vec3(0, h * .55f, 0))) {
            maxT = t;
            best.kind = ShotTrace::ActorHit;
            best.t = t;
            best.actorId = id;
            best.head = true;
        }
        if (rayBox(origin, dir, p + Vec3(-hw, 0, -hw), p + Vec3(hw, h * .68f, hw), t) && t > minT &&
            t < maxT && !smokeObscures(origin, p + Vec3(0, h * .55f, 0))) {
            maxT = t;
            best.kind = ShotTrace::ActorHit;
            best.t = t;
            best.actorId = id;
            best.head = false;
        }
    }
    return best;
}
void Game::spawnBurst(const Vec3 &at, const Vec4 &color, int count, float speed, float size, bool additive,
                      float gravity) {
    for (int i = 0; i < count && particles_.size() < 900; ++i) {
        FxParticle p;
        p.position = at;
        Vec3 dir = normalize(Vec3(rng_.range(-1.f, 1.f), rng_.range(-.2f, 1.f), rng_.range(-1.f, 1.f)));
        p.velocity = dir * (speed * rng_.range(.4f, 1.f));
        float shade = rng_.range(.8f, 1.1f);
        p.color = {color.x * shade, color.y * shade, color.z * shade, color.w};
        p.size = size * rng_.range(.7f, 1.3f);
        p.life = p.maxLife = rng_.range(.3f, .65f);
        p.gravity = gravity;
        p.additive = additive;
        particles_.push_back(p);
    }
}
void Game::spawnMuzzleFx(const Vec3 &muzzle, const Vec3 &dir, int weapon, bool local) {
    if (local)
        muzzleTimer_ = weapon == br::SHOTGUN ? .07f : .05f;
    float scale = weapon == br::SHOTGUN ? 1.5f : 1.f;
    for (int i = 0; i < 2 && particles_.size() < 900; ++i) {
        FxParticle fx;
        fx.position = muzzle + dir * (.08f * i);
        fx.velocity = dir * 1.5f;
        fx.color = {1.f, .82f, .4f, 1.f};
        fx.size = (.32f - i * .1f) * scale;
        fx.life = fx.maxLife = weapon == br::SHOTGUN ? .07f : .05f;
        fx.additive = true;
        particles_.push_back(fx);
    }
    if (weapon == br::SHOTGUN)
        spawnBurst(muzzle, {.8f, .8f, .8f, .5f}, 6, 1.6f, .18f, false, -1.f);
}
void Game::fireVisualShot(const Vec3 &origin, const Vec3 &muzzle, const Vec3 &dir, int weapon, int shooterId,
                          bool local) {
    const auto spec = br::weaponSpec(weapon);
    const Vec3 tcol = weaponTracerColor(weapon);
    const float minT = local ? camDistance_ * .65f : .15f;
    if (weapon == br::SHOTGUN) {
        ShotTrace aim = traceShot(origin, dir, spec.range, minT, shooterId);
        Vec3 target = origin + dir * (aim.kind == ShotTrace::None ? spec.range : aim.t);
        Vec3 to = target - muzzle;
        Vec3 bdir = length(to) > .2f ? normalize(to) : dir;
        if (bullets_.size() < 64)
            bullets_.push_back({muzzle, bdir * 30.f, {tcol.x, tcol.y, tcol.z, 1.f}, spec.range / 30.f, weapon});
    }
    int hitActorId = -1;
    bool anyHead = false;
    float totalDamage = 0;
    Vec3 hitPoint{};
    int pellets = std::max(1, spec.pellets);
    const float spread = weapon == br::SHOTGUN ? .045f : 0.f;
    const float hsMul = weapon == br::SHOTGUN ? 1.5f : 1.8f;
    for (int p = 0; p < pellets; ++p) {
        Vec3 pdir = dir;
        if (spread > 0 && p > 0) {
            pdir = normalize(dir + Vec3(rng_.range(-spread, spread),
                                        rng_.range(-spread, spread),
                                        rng_.range(-spread, spread)));
        }
        ShotTrace hit = traceShot(origin, pdir, spec.range, minT, shooterId);
        float endT = hit.kind == ShotTrace::None ? spec.range : hit.t;
        Vec3 pt = origin + pdir * endT;
        if (weapon != br::SHOTGUN || p < 4) {
            if (tracers_.size() < 96)
                tracers_.push_back({muzzle, pt, {tcol.x, tcol.y, tcol.z, 1.f}, .06f});
        }
        if (hit.kind == ShotTrace::World) {
            Vec3 n(float(hit.world.nx), float(hit.world.ny), float(hit.world.nz));
            spawnBurst(pt + n * .06f, {.62f, .58f, .52f, 1.f}, 5, 2.6f, .08f, false, 14.f);
        } else if (hit.kind == ShotTrace::Wall) {
            spawnBurst(pt, {.55f, .88f, 1.f, 1.f}, 6, 2.8f, .09f, false, 12.f);
        } else if (hit.kind == ShotTrace::ActorHit) {
            auto it = actors_.find(hit.actorId);
            if (it != actors_.end()) {
                it->second.hitFlash = .18f;
                it->second.barTimer = 3.5f;
            }
            spawnBurst(pt, {.92f, .18f, .15f, 1.f}, 6, 2.8f, .09f, false, 12.f);
            hitActorId = hit.actorId;
            anyHead = anyHead || hit.head;
            totalDamage += spec.damage * (hit.head ? hsMul : 1.f);
            hitPoint = pt;
        }
    }
    if (local && hitActorId >= 0) {
        hitMark_ = .18f;
        headshotMark_ = anyHead;
        emit(EV_SFX, anyHead ? 3 : 2, 85, 0);
        if (labels_.size() < 32)
            labels_.push_back({hitPoint + Vec3(0, .35f, 0), int(std::lround(totalDamage)), anyHead ? 1 : 0, .85f});
    }
}
void Game::updateWeapon(float dt, const InputState &in) {
    if (!battle_ || !ready_ || snapshot_.empty())
        return;
    const int weapon = int(self_[12]);
    const auto spec = br::weaponSpec(weapon);
    const float interval = std::max(.08f, spec.interval);
    fireCooldown_ = std::max(-interval, fireCooldown_ - dt);
    shotFeedback_ = std::max(0.f, shotFeedback_ - dt);
    muzzleTimer_ = std::max(0.f, muzzleTimer_ - dt);
    weaponKick_ = std::max(0.f, weaponKick_ - dt * (weapon == br::SHOTGUN ? 4.f : 7.f));
    hitMark_ = std::max(0.f, hitMark_ - dt);
    emptyClick_ = std::max(0.f, emptyClick_ - dt);
    damageDirTimer_ = std::max(0.f, damageDirTimer_ - dt);

    int reserveAmmo = 0;
    if (weapon >= br::PISTOL && weapon <= br::SNIPER && snapshot_.size() >= 48) {
        const size_t invAt = snapshot_.size() - 48;
        for (int i = 0; i < 24; ++i)
            if (int(snapshot_[invAt + i * 2]) == spec.ammo)
                reserveAmmo += int(snapshot_[invAt + i * 2 + 1]);
        if (self_[13] > 0)
            weaponInitialLoaded_[weapon] = true;
    }
    const bool initialLoad =
        weapon >= br::PISTOL && weapon <= br::SNIPER && !weaponInitialLoaded_[weapon] && reserveAmmo > 0;

    if (weapon != lastWeaponId_) {
        lastWeaponId_ = weapon;
        localAmmo_ = (self_[13] <= 0 && initialLoad) ? float(std::min(spec.magazine, reserveAmmo)) : self_[13];
        fireCooldown_ = 0;
        if (weapon > 0)
            emit(EV_SFX, 25, 70, 0);
    } else if (localAmmo_ <= 0 && initialLoad) {
        localAmmo_ = float(std::min(spec.magazine, reserveAmmo));
    } else if ((!firingNow_ && !initialLoad) || (self_[14] > 0 && !initialLoad) || self_[13] > localAmmo_) {
        localAmmo_ = self_[13];
    }

    if (self_[14] > 0 && lastReloadState_ <= 0) {
        rackPlayed_ = false;
        emit(EV_SFX, 1, 85, 0);
    } else if (self_[14] > 0 && !rackPlayed_ && spec.reload > 0 && self_[14] <= spec.reload * .3f) {
        rackPlayed_ = true;
        emit(EV_SFX, 23, 85, 0);
    }
    lastReloadState_ = self_[14];

    // Aim assist & target lock against living enemy actors.
    assistTarget_ = -1;
    assistAngle_ = 10.f;
    float bestAngle = 10.f;
    int bestId = -1;
    const float assistRange = std::min(65.f, spec.range);
    for (const auto &[id, a] : actors_) {
        if (id == localId_ || (int(a.state[3]) & 1) == 0 || int(a.state[11]) == br::IN_PLANE)
            continue;
        if (a.state[1] == self_[1])
            continue;
        float h = (int(a.state[3]) & 2) ? .85f : ((int(a.state[3]) & 16) ? 1.25f : 1.75f);
        Vec3 chest = a.position + Vec3(0, h * .58f, 0);
        Vec3 v = chest - camPos_;
        float dist = length(v);
        if (dist > assistRange || dist < .5f)
            continue;
        float c = dot(v / dist, camForward_);
        if (c < .9f)
            continue;
        float angle = std::acos(clampf(c, -1.f, 1.f)) - std::atan2(.36f, dist);
        if (angle < bestAngle) {
            bestAngle = angle;
            bestId = id;
        }
    }
    if (bestId >= 0) {
        const auto &a = actors_.at(bestId);
        float h = (int(a.state[3]) & 2) ? .85f : ((int(a.state[3]) & 16) ? 1.25f : 1.75f);
        Vec3 chest = a.position + Vec3(0, h * .58f, 0);
        if (world_.lineOfSight(camPos_, chest) && !smokeObscures(camPos_, chest)) {
            assistTarget_ = bestId;
            assistAngle_ = std::max(0.f, bestAngle);
        }
    }
    float cone = aimAssist_ ? lerpf(.08f, .048f, aimBlend_) : 0.f;
    if (assistAngle_ > std::max(cone, .032f))
        assistTarget_ = -1;
    targetLocked_ = assistTarget_ >= 0 && actors_.count(assistTarget_) > 0;
    targetHp_ = targetLocked_ ? clampf(actors_.at(assistTarget_).state[9] / 100.f, 0.f, 1.f) : 0.f;

    const bool alive = !dead_ && (int(self_[3]) & 1) != 0 && (int(self_[3]) & 2) == 0 &&
                       predictedFlight_ == br::LANDED && br::clientInputActive(snapshot_[1]);
    const bool trigger = alive && in.fire && !throwAiming_;
    if (weapon == br::KNIFE) {
        firingNow_ = false;
        if (trigger && fireCooldown_ <= 0) {
            fireCooldown_ = spec.interval;
            shotFeedback_ = .12f;
            auto it = actors_.find(localId_);
            if (it != actors_.end())
                it->second.meleeTime = 0;
            emit(EV_SFX, 18, 75, 0);
            ShotTrace hit = traceShot(camPos_, camForward_, camDistance_ + 2.4f, camDistance_ * .5f, localId_);
            if (hit.kind == ShotTrace::ActorHit) {
                hitMark_ = .18f;
                headshotMark_ = false;
                shake_ = std::max(shake_, .2f);
                emit(EV_SFX, 2, 85, 0);
            }
        }
        return;
    }
    if (trigger && weapon > 0 && localAmmo_ <= 0 && self_[14] <= 0) {
        if (emptyClick_ <= 0) {
            emit(EV_SFX, 11, 80, 0);
            emptyClick_ = .35f;
        }
    }
    firingNow_ = trigger && weapon > 0 && localAmmo_ > 0 && (self_[14] <= 0 || initialLoad);
    int fired = 0;
    while (firingNow_ && fireCooldown_ <= 0 && fired < 2 && localAmmo_ > 0) {
        --localAmmo_;
        shotFeedback_ = .1f;
        fireCooldown_ += interval;
        ++fired;
        Vec3 dir = camForward_;
        if (aimAssist_ && assistTarget_ >= 0 && actors_.count(assistTarget_)) {
            const auto &t = actors_.at(assistTarget_);
            float h = (int(t.state[3]) & 2) ? .85f : ((int(t.state[3]) & 16) ? 1.25f : 1.75f);
            Vec3 chest = t.position + Vec3(0, h * .6f, 0);
            dir = normalize(lerp(dir, normalize(chest - camPos_), .6f));
        }
        Vec3 fallbackMuzzle = visualPlayerPos() + Vec3(0, crouching_ ? 1.05f : 1.42f, 0) + dir * .55f;
        Vec3 muz = length(muzzlePos_ - visualPlayerPos()) < 3.f ? muzzlePos_ : fallbackMuzzle;
        spawnMuzzleFx(muz, dir, weapon, true);
        fireVisualShot(camPos_, muz, dir, weapon, localId_, true);
        weaponKick_ = std::max(weaponKick_, weapon == br::SHOTGUN ? .9f : .5f);
        recoil_ = std::min(.14f, recoil_ + (weapon == br::SHOTGUN ? .035f : (weapon == br::SNIPER ? .04f : .012f)));
        if (weapon == br::SHOTGUN || weapon == br::SNIPER)
            shake_ = std::max(shake_, .22f);
        emit(EV_SFX, weaponSoundId(weapon), 82, 0);
    }
}
void Game::updateEffects(float dt) {
    for (auto &t : tracers_)
        t.life -= dt;
    tracers_.erase(std::remove_if(tracers_.begin(), tracers_.end(), [](const Tracer &t) { return t.life <= 0; }),
                   tracers_.end());
    for (auto &b : bullets_) {
        Vec3 step = b.vel * dt;
        RayHit hit;
        if (world_.raycast(b.pos, normalize(b.vel), length(step), hit)) {
            b.life = 0;
            continue;
        }
        b.pos += step;
        b.life -= dt;
    }
    bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Bullet &b) { return b.life <= 0; }),
                   bullets_.end());
    for (auto &p : particles_) {
        p.velocity.y -= p.gravity * dt;
        p.position += p.velocity * dt;
        p.life -= dt;
    }
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(), [](const FxParticle &p) { return p.life <= 0; }),
        particles_.end());
    for (auto &l : labels_) {
        l.position.y += dt * .85f;
        l.life -= dt;
    }
    labels_.erase(std::remove_if(labels_.begin(), labels_.end(), [](const FxLabel &l) { return l.life <= 0; }),
                  labels_.end());
}
void Game::tickInput() {
    if (!ready_ || paused_ || snapshot_.empty() || !br::clientInputActive(snapshot_[1]) ||
        sequence_ >= 16777215 || outgoing_.size() >= 4)
        return;
    InputState in;
    {
        std::lock_guard<std::mutex> l(inputMutex_);
        in = input_;
        input_.buttons = 0;
        input_.slot = -1;
        input_.target = -1;
        input_.dropItem = input_.dropCount = 0;
    }
    uint32_t buttons =
        in.buttons | (in.fire ? br::FIRE : 0) | (in.sprint ? br::SPRINT : 0) | (in.crouch ? br::CROUCH : 0);
    if (predictedFlight_ == br::LANDED) {
        buttons |= br::INTERACT;
        const int gun = int(self_[12]);
        int reserveAmmo = 0;
        if (gun >= br::PISTOL && gun <= br::SNIPER && snapshot_.size() >= 48) {
            const int ammoId = br::weaponSpec(gun).ammo;
            const size_t invAt = snapshot_.size() - 48;
            for (int i = 0; i < 24; ++i)
                if (int(snapshot_[invAt + i * 2]) == ammoId)
                    reserveAmmo += int(snapshot_[invAt + i * 2 + 1]);
            if (self_[13] <= 0 && self_[14] <= 0 && reserveAmmo > 0)
                buttons |= br::RELOAD;
        }
        if (in.slot < 0 && snapshot_.size() >= 48 &&
            (gun == 0 || ((buttons & br::FIRE) && gun >= br::PISTOL && gun <= br::SNIPER && self_[13] <= 0 &&
                          reserveAmmo <= 0))) {
            const size_t invAt = snapshot_.size() - 48;
            int fallbackSlot = -1, loadedSlot = -1;
            for (int i = 0; i < 24; ++i) {
                const int item = int(snapshot_[invAt + i * 2]);
                if (item >= br::PISTOL && item <= br::SNIPER && int(snapshot_[invAt + i * 2 + 1]) > 0) {
                    if (fallbackSlot < 0)
                        fallbackSlot = i;
                    const int aid = br::weaponSpec(item).ammo;
                    for (int j = 0; j < 24; ++j)
                        if (int(snapshot_[invAt + j * 2]) == aid && int(snapshot_[invAt + j * 2 + 1]) > 0)
                            loadedSlot = i;
                }
            }
            if (loadedSlot >= 0)
                in.slot = loadedSlot;
            else if (gun == 0 && fallbackSlot >= 0)
                in.slot = fallbackSlot;
        }
    }
    float cmdYaw = camYaw_, cmdPitch = camPitch_;
    if (predictedFlight_ == br::LANDED && ((buttons & br::FIRE) || in.aim)) {
        const auto spec = br::weaponSpec(int(self_[12]));
        Vec3 aimDir = camForward_;
        if (aimAssist_ && assistTarget_ >= 0 && actors_.count(assistTarget_)) {
            const auto &t = actors_.at(assistTarget_);
            float h = (int(t.state[3]) & 2) ? .85f : ((int(t.state[3]) & 16) ? 1.25f : 1.75f);
            Vec3 chest = t.position + Vec3(0, h * .6f, 0);
            aimDir = normalize(lerp(aimDir, normalize(chest - camPos_), .6f));
        }
        ShotTrace tr = traceShot(camPos_, aimDir, spec.range, camDistance_ * .65f, localId_);
        float hitT = tr.kind == ShotTrace::None ? spec.range : tr.t;
        Vec3 aimPoint = camPos_ + aimDir * hitT;
        Vec3 serverEye = player_.position + Vec3(0, (buttons & br::CROUCH) ? 1.05f : 1.5f, 0);
        Vec3 toTarget = aimPoint - serverEye;
        if (length(toTarget) > .25f) {
            cmdYaw = wrapAngle(yawFromDirection(toTarget.x, toTarget.z));
            cmdPitch = clampf(std::atan2(toTarget.y, std::max(1e-3f, lengthXZ(toTarget))), -1.3f, 1.3f);
        }
    }
    std::array<float, 12> command = {float(++sequence_), in.moveX,
                                     in.moveY,           cmdYaw,
                                     cmdPitch,           float(buttons),
                                     float(in.slot),     float(in.target),
                                     float(in.dropItem), float(in.dropCount),
                                     in.markX,           in.markZ};
    if (pending_.size() >= 32)
        pending_.pop_front();
    if (in.buttons & br::MELEE) {
        auto it = actors_.find(localId_);
        if (it != actors_.end())
            it->second.meleeTime = 0;
    }
    if (in.buttons & br::THROW_GRENADE) {
        auto it = actors_.find(localId_);
        if (it != actors_.end())
            it->second.throwTime = 0;
    }
    predict(command);
    if (player_.stepOffset != 0) {
        visualStep_ = clampf(visualStep_ - player_.stepOffset, -1.f, 1.f);
        player_.stepOffset = 0;
    }
    pending_.push_back({command, motion_});
    outgoing_.push_back(command);
}
int Game::pollBattleInput(float *out, int cap) {
    if (cap < 12 || outgoing_.empty())
        return 0;
    std::copy(outgoing_.front().begin(), outgoing_.front().end(), out);
    outgoing_.pop_front();
    return 12;
}
void Game::updateCamera(float dt, InputState &in) {
    if (!battle_) {
        Vec3 target = layout_.start + Vec3(-.1f, .92f, 0);
        Vec3 eye = target + Vec3(std::sin(time_ * .2f) * .07f, .3f + std::sin(time_ * .13f) * .02f, 2.7f);
        if (lobbyScreen_ == 2) {
            target = layout_.start + Vec3(-4, .62f, 0);
            eye = target + Vec3(0, .3f, 6.8f);
        }
        camPos_ = eye;
        camForward_ = normalize(target - eye);
        camYaw_ = yawFromDirection(camForward_.x, camForward_.z);
        view_ = Mat4::lookAt(eye, target, {0, 1, 0});
        proj_ =
            Mat4::perspective(.85f, float(renderer_.width()) / std::max(1, renderer_.height()), .08f, 200.f);
        return;
    }
    float aimFov = self_[12] == br::SNIPER ? (localAimStage_ >= 2 ? .22f : .36f)
                                           : (self_[12] == br::SHOTGUN ? .80f : .68f);
    float k = .0058f * sensitivity_ * lerpf(1, aimFov * .85f, aimBlend_);
    if (targetLocked_ && firingNow_)
        k *= .72f;
    if (!dead_) {
        camYaw_ = wrapAngle(camYaw_ + in.lookX * k);
        camPitch_ = clampf(camPitch_ + (invertY_ ? in.lookY : -in.lookY) * k, -1.15f, 1.f);
    }
    if (aimAssist_ && firingNow_ && assistTarget_ >= 0 && actors_.count(assistTarget_)) {
        const auto &t = actors_.at(assistTarget_);
        float h = (int(t.state[3]) & 2) ? .85f : ((int(t.state[3]) & 16) ? 1.25f : 1.75f);
        Vec3 chest = t.position + Vec3(0, h * .55f, 0);
        Vec3 to = chest - camPos_;
        float targetYaw = yawFromDirection(to.x, to.z);
        float targetPitch = std::atan2(to.y, std::max(1e-3f, lengthXZ(to)));
        float pull = clampf(dt * 2.6f, 0.f, 1.f) * (1.f - clampf(assistAngle_ / .1f, 0.f, 1.f));
        camYaw_ = wrapAngle(camYaw_ + wrapAngle(targetYaw - camYaw_) * pull * .5f);
        camPitch_ = clampf(camPitch_ + (targetPitch - camPitch_) * pull * .35f, -1.15f, 1.f);
    }
    if (in.shoulderSwaps % 2)
        shoulder_ = -shoulder_;
    shoulderBlend_ = lerpf(shoulderBlend_, shoulder_, damp(9, dt));
    aimBlend_ = lerpf(aimBlend_, in.aim && !dead_ ? 1.f : 0.f, damp(12, dt));
    crouchBlend_ = lerpf(crouchBlend_, crouching_ ? 1.f : 0.f, damp(12, dt));
    recoil_ = lerpf(recoil_, 0.f, damp(10, dt));
    shake_ = std::max(0.f, shake_ - dt * 1.8f);
    sprintFov_ = lerpf(sprintFov_, sprinting_ ? .07f : 0.f, damp(4, dt));
    fov_ = lerpf(1, aimFov, aimBlend_) + sprintFov_;
    const bool aboard = battle_ && !snapshot_.empty() && self_[11] == br::IN_PLANE;
    const bool inFlight = battle_ && !snapshot_.empty() && self_[11] != br::LANDED && !aboard;
    const bool inFreefall = inFlight && self_[11] == br::FREEFALL;
    const bool inChute = inFlight && self_[11] == br::CHUTE;

    const float horizontal = lengthXZ(visualPlayer_.velocity);
    if (visualPlayer_.onGround && horizontal > .5f && !dead_)
        bobPhase_ += dt * horizontal * 1.7f;
    float bob = std::sin(bobPhase_ * 2) * (sprinting_ ? .045f : .02f) * clampf(horizontal / 4, 0, 1) *
                (1 - aimBlend_);
    Vec3 renderPos = visualPlayerPos();
    float pivotHeight = 1.72f;
    if (inFreefall)
        pivotHeight = 1.15f;
    else if (inChute)
        pivotHeight = 2.40f;
    else
        pivotHeight = lerpf(1.72f, 1.2f, crouchBlend_) + bob;

    Vec3 target = renderPos + Vec3(0, pivotHeight, 0);
    if (aboard) {
        target = (planePosInit_ ? visualPlanePos_ : player_.position) + Vec3(0, 6, 0);
        camPivot_ = target;
        camPivotInit_ = true;
    } else {
        if (!camPivotInit_ || length(target - camPivot_) > 6) {
            camPivot_ = target;
            camPivotInit_ = true;
        }
        camPivot_.x = lerpf(camPivot_.x, target.x, damp(20, dt));
        camPivot_.z = lerpf(camPivot_.z, target.z, damp(20, dt));
        camPivot_.y = lerpf(camPivot_.y, target.y, damp(13, dt));
    }
    Vec3 right = rightFromYaw(camYaw_) * (shoulderBlend_ >= 0 ? 1.f : -1.f);
    float side = (battle_ && !aboard && !inFlight) ? lerpf(1, .78f, aimBlend_) * std::abs(shoulderBlend_) : 0;
    RayHit hit;
    if (!loading_ && world_.raycast(camPivot_, right, side + .25f, hit))
        side = std::max(0.f, hit.distance - .25f);
    Vec3 pivot = camPivot_ + right * side;
    float shakeAmp = shake_ * shake_ * .12f;
    float yaw = camYaw_ + std::sin(time_ * 47.f) * shakeAmp * .6f;
    float pitch = clampf(camPitch_ + recoil_ + std::sin(time_ * 39.f + 1.3f) * shakeAmp * .5f, -1.2f, 1.1f);
    camForward_ = direction(yaw, pitch);
    float want = aboard ? 25.f
                        : (dead_ ? 5.5f
                                 : (inChute ? 8.2f
                                            : (inFreefall ? 6.2f
                                                          : lerpf(lerpf(4.9f, 4.3f, crouchBlend_), 2.4f, aimBlend_))));
    if (!loading_ && world_.raycast(pivot, -camForward_, want + .3f, hit))
        want = std::max(.3f, hit.distance - .3f);
    camDistance_ = want < camDistance_ ? want : lerpf(camDistance_, want, damp(5, dt));
    camPos_ = pivot - camForward_ * camDistance_;
    view_ = Mat4::lookAt(camPos_, camPos_ + camForward_, {0, 1, 0});
    proj_ = Mat4::perspective(fov_, float(renderer_.width()) / std::max(1, renderer_.height()), .08f,
                              battle_ ? 2300.f : 180.f);
}
void Game::box(Vec3 p, Vec3 size, Vec4 color, float yaw) {
    scene_.boxes.push_back({Mat4::translation(p) * Mat4::rotationY(yaw) * Mat4::scale(size), color, 0});
}
void Game::buildScene() {
    gunHold_.valid = false;
    scene_.world = &world_;
    scene_.view = view_;
    scene_.proj = proj_;
    scene_.cameraPos = camPos_;
    scene_.cameraForward = camForward_;
    scene_.sky = theme_.sky;
    scene_.time = time_;
    scene_.focus = visualPlayer_.position;
    scene_.drawDistance = battle_ ? 2200 : 180;
    scene_.sky.fogStart = 1900;
    scene_.sky.fogEnd = 2200;
    scene_.upload.unlimited = loading_;
    scene_.characterCount = 0;
    if (scene_.characters.capacity() < std::max<size_t>(512, scene_.characters.size() + 64))
        scene_.characters.reserve(std::max<size_t>(512, scene_.characters.size() + 64));
    scene_.boxes.clear();
    scene_.beams.clear();
    scene_.particles.clear();
    scene_.shadows.clear();
    scene_.lights.clear();
    scene_.zoneAlpha = 0;
    scene_.zoneRadius = 0;
    scene_.muzzleFlash =
        muzzleTimer_ > 0 ? Vec4(muzzlePos_.x, muzzlePos_.y, muzzlePos_.z, 1.6f) : Vec4(0, -100, 0, 0);
    if (!battle_) {
        auto *asset = character(look_);
        Pose pose;
        lobbyAnim_.evaluate(asset->model, pose);
        ikAim_ = ikKick_ = ikDip_ = ikMove_ = 0;
        ikFalling_ = false;
        ikReload_ = -1;
        ikTwist_ = 1;
        if (previewWeapon_ != br::KNIFE)
            poseArmedBlend(pose, asset->model, -.3f, false, 0, 1, previewWeapon_);
        addCharacter(asset, layout_.start, kPi - (lobbyScreen_ == 2 ? .6f : 0.f), 1, pose);
        scene_.shadows.push_back({layout_.start, .55f, .32f});
        int arm = asset->model.nodeIndex("arm-right");
        if (arm >= 0)
            addWeaponModel(scene_.characters[scene_.characterCount - 1], arm, previewWeapon_, nullptr);

        // Cenário 3D do Lobby (hangar militar, pedestal octogonal luminoso, pet companheiro, expositor de armas e buggy)
        const Vec3 lp = layout_.start;
        // 1. Pedestal Octogonal Iluminado sob o jogador
        box(lp + Vec3(0, -.08f, 0), {2.8f, .16f, 2.8f}, {.18f, .20f, .25f, 1.f}, 0.f);
        box(lp + Vec3(0, -.06f, 0), {2.5f, .18f, 2.5f}, {.14f, .16f, .20f, 1.f}, kPi * .25f);
        box(lp + Vec3(0, -.03f, 0), {2.0f, .14f, 2.0f}, {.22f, .26f, .32f, 1.f}, 0.f);
        for (int k = 0; k < 16; ++k) {
            float a0 = k * kTwoPi / 16.f, a1 = (k + 1) * kTwoPi / 16.f;
            Vec3 b0 = lp + Vec3(std::cos(a0) * 1.32f, .04f, std::sin(a0) * 1.32f);
            Vec3 b1 = lp + Vec3(std::cos(a1) * 1.32f, .04f, std::sin(a1) * 1.32f);
            scene_.beams.push_back({b0, b1, {.98f, .65f, .10f, .95f}, .055f});
        }
        // Trilhos de luz âmbar no piso do hangar em direção à câmera
        scene_.beams.push_back({lp + Vec3(-.95f, .02f, 1.32f), lp + Vec3(-1.45f, .02f, 4.2f), {.98f, .55f, .08f, .9f}, .045f});
        scene_.beams.push_back({lp + Vec3(.95f, .02f, 1.32f), lp + Vec3(1.45f, .02f, 4.2f), {.98f, .55f, .08f, .9f}, .045f});

        // 2. Cachorrinho Voxel Companheiro com bandana vermelha
        const Vec3 dp = lp + Vec3(.70f, 0.f, .16f);
        scene_.shadows.push_back({dp, .28f, .18f});
        box(dp + Vec3(0, .24f, 0), {.32f, .26f, .44f}, {.88f, .72f, .48f, 1.f}, .18f);
        for (float fx : {-.11f, .11f}) {
            for (float fz : {-.14f, .14f}) {
                box(dp + Vec3(fx, .08f, fz), {.08f, .16f, .08f}, {.95f, .95f, .95f, 1.f}, .18f);
            }
        }
        box(dp + Vec3(0, .42f, .18f), {.28f, .26f, .26f}, {.88f, .72f, .48f, 1.f}, .18f);
        box(dp + Vec3(0, .36f, .28f), {.16f, .14f, .12f}, {.95f, .95f, .95f, 1.f}, .18f);
        box(dp + Vec3(0, .39f, .34f), {.06f, .05f, .04f}, {.12f, .12f, .12f, 1.f}, .18f);
        box(dp + Vec3(-.08f, .46f, .28f), {.04f, .06f, .04f}, {.10f, .10f, .10f, 1.f}, .18f);
        box(dp + Vec3(.08f, .46f, .28f), {.04f, .06f, .04f}, {.10f, .10f, .10f, 1.f}, .18f);
        box(dp + Vec3(-.15f, .48f, .16f), {.07f, .14f, .10f}, {.56f, .36f, .18f, 1.f}, .18f);
        box(dp + Vec3(.15f, .48f, .16f), {.07f, .14f, .10f}, {.56f, .36f, .18f, 1.f}, .18f);
        box(dp + Vec3(0, .32f, .14f), {.30f, .08f, .30f}, {.92f, .18f, .16f, 1.f}, .18f);
        box(dp + Vec3(0, .24f, .22f), {.12f, .10f, .07f}, {.92f, .18f, .16f, 1.f}, .18f);
        box(dp + Vec3(0, .34f, -.24f), {.06f, .16f, .06f}, {.88f, .72f, .48f, 1.f}, .35f);

        // 3. Expositor de Armas Iluminado com borda neon azul (fundo à direita)
        const Vec3 wr = lp + Vec3(2.75f, 0.f, -1.95f);
        const float wrYaw = -.32f;
        box(wr + Vec3(0, 1.25f, 0), {2.1f, 1.8f, .18f}, {.14f, .17f, .22f, 1.f}, wrYaw);
        box(wr + Vec3(0, 1.25f, .02f), {1.95f, 1.65f, .06f}, {.08f, .10f, .14f, 1.f}, wrYaw);
        Vec3 rRight = rightFromYaw(wrYaw);
        Vec3 rUp(0, 1.f, 0);
        Vec3 cTL = wr + Vec3(0, 1.25f, .08f) - rRight * .98f + rUp * .82f;
        Vec3 cTR = wr + Vec3(0, 1.25f, .08f) + rRight * .98f + rUp * .82f;
        Vec3 cBR = wr + Vec3(0, 1.25f, .08f) + rRight * .98f - rUp * .82f;
        Vec3 cBL = wr + Vec3(0, 1.25f, .08f) - rRight * .98f - rUp * .82f;
        scene_.beams.push_back({cTL, cTR, {.22f, .78f, 1.f, .95f}, .045f});
        scene_.beams.push_back({cTR, cBR, {.22f, .78f, 1.f, .95f}, .045f});
        scene_.beams.push_back({cBR, cBL, {.22f, .78f, 1.f, .95f}, .045f});
        scene_.beams.push_back({cBL, cTL, {.22f, .78f, 1.f, .95f}, .045f});
        // 3 armas expostas no rack (fuzil vermelho, fuzil dourado, sniper roxo)
        box(wr + Vec3(0, 1.62f, .12f), {1.25f, .24f, .08f}, {.92f, .18f, .24f, 1.f}, wrYaw);
        box(wr + Vec3(0, 1.25f, .12f), {1.35f, .22f, .08f}, {.96f, .76f, .18f, 1.f}, wrYaw);
        box(wr + Vec3(0, .88f, .12f), {1.45f, .20f, .08f}, {.72f, .24f, .92f, 1.f}, wrYaw);

        // 4. Buggy de Combate Militar na plataforma (fundo à esquerda)
        const Vec3 bg = lp + Vec3(-2.85f, 0.f, -1.65f);
        const float bgYaw = .30f;
        box(bg + Vec3(0, .18f, 0), {2.1f, .18f, 2.5f}, {.16f, .18f, .22f, 1.f}, bgYaw);
        for (float tx : {-.82f, .82f}) {
            for (float tz : {-.88f, .88f}) {
                box(bg + Vec3(tx, .42f, tz), {.38f, .52f, .38f}, {.12f, .12f, .12f, 1.f}, bgYaw);
            }
        }
        box(bg + Vec3(0, .66f, 0), {1.55f, .42f, 2.1f}, {.86f, .82f, .74f, 1.f}, bgYaw);
        box(bg + Vec3(0, 1.12f, -.08f), {1.25f, .52f, 1.15f}, {.20f, .22f, .26f, 1.f}, bgYaw);
        box(bg + Vec3(-.42f, .68f, 1.05f), {.16f, .12f, .10f}, {1.f, .85f, .32f, 1.f}, bgYaw);
        box(bg + Vec3(.42f, .68f, 1.05f), {.16f, .12f, .10f}, {1.f, .85f, .32f, 1.f}, bgYaw);
        return;
    }
    if (loading_)
        return;
    for (auto &[id, a] : actors_) {
        if (a.deathTime > 3.2f || int(a.state[11]) == br::IN_PLANE)
            continue;
        bool local = id == localId_;
        Vec3 pos = local ? visualPlayerPos() : a.position;
        if (length(pos - camPos_) > 500)
            continue;
        uint32_t look = uint32_t(a.state[2]) | (uint32_t(a.state[30]) << 16);
        auto *asset = character(look);
        Pose pose;
        a.anim.evaluate(asset->model, pose);
        poseActor(a, local, pose);
        pos.y -= .12f * a.presentation.crouchBlend;
        float flash = a.hitFlash > 0 ? .55f : 0.f;
        float actorPitch = a.presentation.freefallPitch;
        if (int(a.state[11]) == br::FREEFALL) {
            actorPitch += std::cos(a.presentation.flightTime * 12.f) * 0.025f;
        } else if (int(a.state[11]) == br::CHUTE) {
            actorPitch += std::sin(a.presentation.flightTime * 2.2f) * 0.025f;
        }
        addCharacter(asset, pos, a.presentation.yaw, 1, pose, {1, 1, 1}, flash, {1, 1, 1}, a.presentation.lean,
                     actorPitch);
        scene_.shadows.push_back({pos, .55f, .32f});
        int gun = int(a.state[12]);
        bool alive = (int(a.state[3]) & 1) != 0;
        bool utility = local && (throwAiming_ || a.throwTime >= 0);
        a.muzzle = pos + Vec3(0, 1.35f, 0) + forwardFromYaw(a.presentation.yaw) * .5f;
        if ((gun > 0 || utility || a.meleeTime >= 0) && alive) {
            const auto &actor = scene_.characters[scene_.characterCount - 1];
            int arm = asset->model.nodeIndex("arm-right"), torso = asset->model.nodeIndex("torso");
            if (utility) {
                gunHold_.valid = false;
                if (gun > 0 && torso >= 0)
                    addWeaponOnBack(actor, torso, gun);
                if (throwAiming_ || a.throwTime < .18f) {
                    int hand = asset->model.nodeIndex("RightHand");
                    if (hand >= 0)
                        addVoxelItem(VoxelItem::Grenade,
                                     actor.world * actor.nodes[size_t(hand)] *
                                         Mat4::translation(Vec3(.3f, -1.6f, 0) * kBlockyPixel) *
                                         Mat4::rotationX(kPi * .5f));
                }
            } else if (gun == br::KNIFE || a.meleeTime >= 0) {
                gunHold_.valid = false;
                int hand = asset->model.nodeIndex("RightHand");
                if (hand >= 0)
                    addVoxelItem(VoxelItem::Knife, actor.world * actor.nodes[size_t(hand)] *
                                                       Mat4::translation(Vec3(.3f, -1.6f, 0) * kBlockyPixel) *
                                                       Mat4::rotationX(kPi * .5f));
            } else if ((a.presentation.armedBlend < .5f || int(a.state[11]) != br::LANDED) && torso >= 0) {
                gunHold_.valid = false;
                addWeaponOnBack(actor, torso, gun);
            } else if (arm >= 0) {
                addWeaponModel(actor, arm, gun, local ? &muzzlePos_ : &a.muzzle);
                if (local)
                    a.muzzle = muzzlePos_;
            }
        } else {
            gunHold_.valid = false;
        }
        if (int(a.state[11]) == br::FREEFALL) {
            // Em queda livre, renderiza o equipamento de paraquedas (rig container) no dorso do herói
            const auto &actor = scene_.characters[scene_.characterCount - 1];
            int torso = asset->model.nodeIndex("torso");
            Mat4 torsoWorld = torso >= 0 ? (actor.world * actor.nodes[size_t(torso)]) : actor.world;
            scene_.boxes.push_back({torsoWorld * Mat4::translation({0.f, .12f, -.24f}) * Mat4::scale({.38f, .48f, .18f}),
                                    {.18f, .22f, .28f, 1.f}, 0.f});
            scene_.boxes.push_back({torsoWorld * Mat4::translation({0.f, .24f, -.26f}) * Mat4::scale({.32f, .14f, .20f}),
                                    {.95f, .68f, .15f, 1.f}, 0.f});
        } else if (int(a.state[11]) == br::CHUTE) {
            const float deployTime = a.presentation.chuteDeployTime;
            // Sequência de abertura: expansão progressiva do tecido e subida do velame em 0.55s
            const float deployT = clampf(deployTime / 0.55f, 0.f, 1.f);
            const float canopyDeploy = smoothstepf(0.05f, 1.f, deployT);
            const float canopyHeight = lerpf(1.5f, 4.22f, canopyDeploy);
            const float canopyScaleX = lerpf(0.2f, 1.f, canopyDeploy);
            const float canopyScaleZ = lerpf(0.2f, 1.f, canopyDeploy);

            const float yaw = -a.presentation.yaw;
            const Vec3 fwd = forwardFromYaw(a.presentation.yaw), right = rightFromYaw(a.presentation.yaw), up(0, 1, 0);
            auto chutePart = [&](float lx, float ly, float lz, Vec3 sz, Vec4 col, float roll = 0.f, float pitch = 0.f) {
                Vec3 wp = pos + right * lx + up * ly + fwd * lz;
                Mat4 m = Mat4::translation(wp) * Mat4::rotationY(yaw) * Mat4::rotationZ(roll) *
                         Mat4::rotationX(pitch) * Mat4::scale(sz);
                scene_.boxes.push_back({m, col, 0.f});
            };

            // Mochila tática e alças no tronco
            chutePart(0.f, 1.24f, -0.22f, {.42f, .50f, .20f}, {.18f, .22f, .28f, 1.f});
            chutePart(0.f, 1.45f, -0.24f, {.36f, .14f, .22f}, {.95f, .68f, .15f, 1.f});
            chutePart(-.15f, 1.38f, -.04f, {.08f, .34f, .26f}, {.14f, .17f, .22f, 1.f});
            chutePart(.15f, 1.38f, -.04f, {.08f, .34f, .26f}, {.14f, .17f, .22f, 1.f});

            if (deployT > 0.05f) {
                // Velame arqueado de 7 células (ram-air) com estabilizadores laterais
                for (int i = -3; i <= 3; ++i) {
                    float lx = float(i) * (.64f * canopyScaleX);
                    float ly = canopyHeight - .085f * float(i * i) * canopyScaleX;
                    float cRoll = -float(i) * .11f + a.presentation.lean * .5f;
                    Vec4 col = i == 0 ? Vec4{.98f, .74f, .15f, 1.f}
                                      : (std::abs(i) == 1 ? Vec4{.17f, .21f, .28f, 1.f}
                                                          : (std::abs(i) == 2 ? Vec4{.94f, .46f, .12f, 1.f}
                                                                              : Vec4{.14f, .17f, .23f, 1.f}));
                    chutePart(lx, ly, 0.f, {.66f * canopyScaleX, .18f * canopyDeploy, 2.38f * canopyScaleZ}, col, cRoll, .06f);
                    chutePart(lx, ly - .06f * canopyDeploy, 1.14f * canopyScaleZ, {.58f * canopyScaleX, .13f * canopyDeploy, .18f * canopyScaleZ}, {.08f, .10f, .14f, 1.f}, cRoll, .06f);
                    chutePart(lx + (i < 0 ? -.31f : .31f) * canopyScaleX, ly - .12f * canopyDeploy, 0.f, {.06f * canopyScaleX, .26f * canopyDeploy, 2.26f * canopyScaleZ}, {.10f, .13f, .18f, 1.f}, cRoll, .06f);
                }
                chutePart(-2.26f * canopyScaleX, canopyHeight - 1.02f, -.05f, {.10f * canopyScaleX, .70f * canopyDeploy, 2.12f * canopyScaleZ}, {.94f, .46f, .12f, 1.f}, .22f, .06f);
                chutePart(2.26f * canopyScaleX, canopyHeight - 1.02f, -.05f, {.10f * canopyScaleX, .70f * canopyDeploy, 2.12f * canopyScaleZ}, {.94f, .46f, .12f, 1.f}, -.22f, .06f);

                // Manoplas de direção e 8 linhas de suspensão (tirantes frontais e traseiros)
                chutePart(-.26f, 1.72f, .08f, {.12f, .06f, .12f}, {.96f, .72f, .14f, 1.f});
                chutePart(.26f, 1.72f, .08f, {.12f, .06f, .12f}, {.96f, .72f, .14f, 1.f});
                for (int i : {-3, -1, 1, 3}) {
                    float lx = float(i) * (.64f * canopyScaleX);
                    float ly = (canopyHeight - .12f) - .085f * float(i * i) * canopyScaleX;
                    Vec3 riser = pos + up * 1.56f + right * (i < 0 ? -.24f : .24f);
                    Vec3 canopyFront = pos + right * lx + up * ly + fwd * (.85f * canopyScaleZ);
                    Vec3 canopyRear = pos + right * lx + up * (ly - .08f) - fwd * (.85f * canopyScaleZ);
                    scene_.beams.push_back({canopyFront, riser, {.86f, .88f, .92f, .95f * canopyDeploy}, .024f});
                    scene_.beams.push_back({canopyRear, riser, {.78f, .80f, .85f, .95f * canopyDeploy}, .024f});
                }
            }
        }
    }
    for (const auto &b : bullets_) {
        Vec3 d = normalize(b.vel);
        float yaw = std::atan2(d.x, d.z);
        float pitch = -std::asin(clampf(d.y, -1.f, 1.f));
        BoxDraw bd;
        bd.world =
            Mat4::translation(b.pos) * Mat4::rotationY(yaw) * Mat4::rotationX(pitch) * Mat4::scale({.09f, .09f, .42f});
        bd.color = b.color;
        bd.emissive = 1.f;
        scene_.boxes.push_back(bd);
        scene_.particles.push_back({b.pos - d * .35f, {1.f, .75f, .35f, .8f}, .3f, true});
        if (scene_.lights.size() < 8)
            scene_.lights.push_back({b.pos, 4.f, Vec3(1.f, .7f, .35f)});
    }
    for (const auto &p : particles_) {
        float a = clampf(p.life / std::max(.01f, p.maxLife), 0.f, 1.f);
        scene_.particles.push_back({p.position,
                                    {p.color.x, p.color.y, p.color.z, p.color.w * a},
                                    p.size * (p.additive ? 1.f : .8f + a * .2f),
                                    p.additive});
    }
    for (const auto &t : tracers_)
        scene_.beams.push_back(
            {t.from, t.to, {t.color.x, t.color.y, t.color.z, std::min(1.f, t.life * 18.f)}, .045f});
    if (throwAiming_) {
        Vec3 p = visualPlayerPos() + Vec3(0, 1.5f, 0), v = direction(camYaw_, camPitch_ - .25f) * 16;
        for (int i = 0; i < 60; ++i) {
            v.y -= 18 * .05f;
            Vec3 next = p + v * .05f;
            RayHit hit;
            if (world_.raycast(p, normalize(next - p), length(next - p), hit)) {
                box(p + normalize(next - p) * hit.distance, {.22f, .08f, .22f}, {1, .82f, .25f, .8f});
                break;
            }
            if (i % 2 == 0)
                scene_.beams.push_back({p, next, {1, .85f, .4f, .8f}, .025f});
            p = next;
        }
    }
    if (snapshot_.empty())
        return;
    if (effects_.size() >= 2 && lastSnapshot_ - effects_[0] < .35f)
        for (size_t i = 2; i + 5 < effects_.size(); i += 6) {
            Vec3 p(effects_[i + 2], effects_[i + 3], effects_[i + 4]);
            addVoxelItem(effects_[i + 1] == 26 ? VoxelItem::Smoke : VoxelItem::Grenade,
                         Mat4::translation(p) * Mat4::rotationZ(time_ * 6));
        }
    if (snapshot_[1] == br::PLANE) {
        Vec3 p = planePosInit_ ? visualPlanePos_ : Vec3(snapshot_[13], snapshot_[14], snapshot_[15]);
        const float yaw = -planeYaw_;
        const Vec3 fwd = forwardFromYaw(planeYaw_), right = rightFromYaw(planeYaw_), up(0, 1, 0);
        auto planePart = [&](float lx, float ly, float lz, Vec3 sz, Vec4 col, float pitch = 0.f, float roll = 0.f, float em = 0.f) {
            Vec3 wp = p + right * lx + up * ly + fwd * lz;
            Mat4 m = Mat4::translation(wp) * Mat4::rotationY(yaw) * Mat4::rotationZ(roll) *
                     Mat4::rotationX(pitch) * Mat4::scale(sz);
            scene_.boxes.push_back({m, col, em});
        };
        // Fuselagem principal tática (estilo C-130 / Dropship militar)
        planePart(0.f, 0.f, 0.f, {3.8f, 2.8f, 15.2f}, {.31f, .36f, .43f, 1.f});
        planePart(0.f, 1.58f, -.2f, {2.6f, .55f, 13.4f}, {.23f, .27f, .33f, 1.f});
        planePart(0.f, -1.48f, .4f, {2.8f, .35f, 12.6f}, {.20f, .23f, .28f, 1.f});
        // Sponsons laterais do trem de pouso e faixas táticas douradas
        planePart(-2.05f, -.72f, -.4f, {.68f, 1.05f, 6.4f}, {.22f, .26f, .31f, 1.f});
        planePart(2.05f, -.72f, -.4f, {.68f, 1.05f, 6.4f}, {.22f, .26f, .31f, 1.f});
        planePart(-1.94f, .22f, .2f, {.08f, .36f, 14.4f}, {.94f, .68f, .15f, 1.f});
        planePart(1.94f, .22f, .2f, {.08f, .36f, 14.4f}, {.94f, .68f, .15f, 1.f});
        // Nariz, radome e cabine iluminada (cockpit)
        planePart(0.f, -.22f, 8.5f, {2.9f, 1.85f, 2.4f}, {.29f, .34f, .41f, 1.f});
        planePart(0.f, -.48f, 10.0f, {1.75f, 1.10f, 1.2f}, {.14f, .16f, .20f, 1.f});
        planePart(0.f, .82f, 7.55f, {2.75f, .95f, 2.2f}, {.22f, .72f, .96f, 1.f}, .32f, 0.f, .48f);
        planePart(0.f, 1.38f, 6.85f, {2.55f, .38f, 2.1f}, {.26f, .31f, .38f, 1.f});
        // Rampa traseira aberta de salto com iluminação interna laranja
        planePart(0.f, .48f, -8.7f, {3.2f, 2.0f, 3.2f}, {.28f, .33f, .40f, 1.f});
        planePart(0.f, .05f, -7.65f, {2.9f, 1.85f, .4f}, {.06f, .08f, .11f, 1.f});
        planePart(0.f, .92f, -7.35f, {2.1f, .22f, .7f}, {1.f, .52f, .14f, 1.f}, 0.f, 0.f, 1.f);
        planePart(0.f, -1.18f, -8.8f, {2.85f, .22f, 2.9f}, {.24f, .28f, .34f, 1.f}, -.28f);
        // Asas altas principais, flaps, winglets e luzes de navegação
        planePart(0.f, 1.36f, .8f, {24.2f, .42f, 3.6f}, {.35f, .40f, .48f, 1.f});
        planePart(0.f, 1.36f, 2.75f, {23.6f, .28f, .52f}, {.23f, .27f, .33f, 1.f});
        planePart(0.f, 1.25f, -1.25f, {21.2f, .22f, .72f}, {.26f, .30f, .36f, 1.f});
        planePart(-12.15f, 1.88f, .6f, {.28f, 1.35f, 2.4f}, {.94f, .58f, .14f, 1.f}, 0.f, -.18f);
        planePart(12.15f, 1.88f, .6f, {.28f, 1.35f, 2.4f}, {.94f, .58f, .14f, 1.f}, 0.f, .18f);
        planePart(-12.35f, 1.45f, 1.5f, {.28f, .28f, .45f}, {1.f, .16f, .14f, 1.f}, 0.f, 0.f, 1.f);
        planePart(12.35f, 1.45f, 1.5f, {.28f, .28f, .45f}, {.16f, 1.f, .36f, 1.f}, 0.f, 0.f, 1.f);
        // 4 motores turboélice com hélices giratórias
        int engIdx = 0;
        for (float ex : {-8.4f, -4.5f, 4.5f, 8.4f}) {
            planePart(ex, .72f, 1.95f, {1.18f, 1.18f, 3.3f}, {.24f, .28f, .34f, 1.f});
            planePart(ex, .72f, 3.72f, {.52f, .52f, .55f}, {.92f, .66f, .15f, 1.f});
            float spin = time_ * 28.f + float(engIdx++) * .85f;
            planePart(ex, .72f, 3.92f, {2.65f, .20f, .08f}, {.13f, .15f, .18f, 1.f}, 0.f, spin);
            planePart(ex, .72f, 3.92f, {2.65f, .20f, .08f}, {.13f, .15f, .18f, 1.f}, 0.f, spin + kPi * .5f);
        }
        // Cauda vertical alta (empenagem) e estabilizadores horizontais
        planePart(0.f, 3.35f, -9.2f, {.48f, 3.9f, 3.5f}, {.31f, .36f, .43f, 1.f});
        planePart(0.f, 5.15f, -9.4f, {.52f, .55f, 2.8f}, {.94f, .68f, .15f, 1.f});
        planePart(0.f, 1.65f, -9.6f, {8.8f, .32f, 2.3f}, {.33f, .38f, .45f, 1.f});
    }
    size_t at = 24 + int(snapshot_[16]) * 32;
    for (int i = 0; i < int(snapshot_[17]); ++i, at += 7) {
        Vec3 p(snapshot_[at + 3], snapshot_[at + 4] + .2f, snapshot_[at + 5]);
        if (length(p - camPos_) < 80)
            box(p, {.35f, .25f, .45f}, {.9f, .7f, .2f, 1}, time_ * .5f);
    }
    for (auto &w : walls_)
        box({w[2], w[3] + 1.35f, w[4]}, {4.2f, 2.7f, .7f}, {.45f, .85f, 1, 1}, w[5]);
    at += int(snapshot_[18]) * 10;
    for (int i = 0; i < int(snapshot_[19]); ++i, at += 7) {
        Vec3 p(snapshot_[at + 1], snapshot_[at + 2], snapshot_[at + 3]);
        box(p + Vec3(0, .7f, 0), {1.5f, 1.4f, 1.5f}, {.65f, .25f, .1f, 1});
        if (snapshot_[at + 4] == 0) {
            box(p + Vec3(0, 4.3f, 0), {3.6f, .35f, 3.6f}, {.95f, .68f, .18f, 1});
            box(p + Vec3(0, 4.6f, 0), {2.2f, .35f, 2.2f}, {.22f, .26f, .34f, 1});
            for (float sx : {-1.4f, 1.4f})
                for (float sz : {-1.4f, 1.4f})
                    scene_.beams.push_back({p + Vec3(sx, 4.2f, sz), p + Vec3(sx * .35f, 1.4f, sz * .35f), {.85f, .85f, .88f, .9f}, .025f});
        }
        else if (length(p - camPos_) < 300)
            for (int k = 0; k < 8; ++k)
                scene_.particles.push_back(
                    {p + Vec3(std::sin(k + time_) * .6f, k * .7f, 0), {.9f, .1f, .1f, .5f}, 1.4f, false});
    }
    for (int i = 0; i < int(snapshot_[20]); ++i, at += 5) {
        Vec3 p(snapshot_[at], snapshot_[at + 1], snapshot_[at + 2]);
        if (length(p - camPos_) < 150)
            for (int k = 0; k < 24; ++k) {
                float a = k * 2.4f;
                scene_.particles.push_back({p + Vec3(std::cos(a) * 3, 1 + (k % 4) * .8f, std::sin(a) * 3),
                                            {.7f, .72f, .74f, .9f},
                                            5,
                                            false});
            }
    }
    const int phase = int(snapshot_[1]);
    const int stage = int(snapshot_[11]);
    const bool shrinking = snapshot_[12] > .5f;
    const float phaseTime = snapshot_[22];
    const float timer = snapshot_[10];
    const bool zoneRevealed =
        stage > 0 || shrinking || phase == br::ACTIVE ||
        (phase == br::PLANE && int(self_[11]) == br::LANDED && phaseTime >= 30.f);
    if (zoneRevealed) {
        const float fade = 1.f;
        const float zx = snapshot_[4], zz = snapshot_[5], zr = snapshot_[6];
        float targetX = zx, targetZ = zz, targetR = zr;
        if (stage == 0 && !shrinking) {
            const float totalWait = std::max(30.f, phaseTime + timer);
            const float waitProg = phase == br::ACTIVE ? clampf(phaseTime / totalWait, 0.f, 1.f) : 0.f;
            targetX = 720.f;
            targetZ = 720.f;
            targetR = lerpf(1050.f, zr, waitProg);
        }
        if (!zoneInit_) {
            visualZoneCenter_ = {targetX, 0.f, targetZ};
            visualZoneRadius_ = targetR;
            zoneInit_ = true;
        }
        const float wallX = visualZoneCenter_.x;
        const float wallZ = visualZoneCenter_.z;
        const float wallR = visualZoneRadius_;
        scene_.zoneCenter = {wallX, 0.f, wallZ};
        scene_.zoneRadius = wallR;
        scene_.zoneAlpha = .88f * fade;
        const Vec3 playerPos = visualPlayerPos();
        for (int k = 0; k < 160; ++k) {
            const float a = k * kTwoPi / 160.f, b = (k + 1) * kTwoPi / 160.f;
            Vec3 p(wallX + std::cos(a) * wallR, playerPos.y, wallZ + std::sin(a) * wallR);
            Vec3 q(wallX + std::cos(b) * wallR, playerPos.y, wallZ + std::sin(b) * wallR);
            const float dist = lengthXZ(p - playerPos);
            if (dist < 650.f) {
                const float gy0 = float(source_->map.heightAt(int(clampf(p.x, 1.f, 1438.f)), int(clampf(p.z, 1.f, 1438.f)))) + .25f;
                const float gy1 = float(source_->map.heightAt(int(clampf(q.x, 1.f, 1438.f)), int(clampf(q.z, 1.f, 1438.f)))) + .25f;
                scene_.beams.push_back({Vec3(p.x, gy0, p.z), Vec3(q.x, gy1, q.z), {.35f, .92f, 1.f, .95f * fade}, .38f});
                for (int y = 1; y <= 5; ++y) {
                    const float hOff = float(y) * 5.5f + std::sin(time_ * 2.4f + float(y)) * .35f;
                    scene_.beams.push_back(
                        {Vec3(p.x, gy0 + hOff, p.z), Vec3(q.x, gy1 + hOff, q.z), {.22f, .72f, 1.f, .75f * fade}, .22f});
                }
            }
        }
    }
}
bool Game::project(const Vec3 &p, float &sx, float &sy) const {
    Vec4 clip = (proj_ * view_).transform({p.x, p.y, p.z, 1.f});
    if (clip.w <= .05f)
        return false;
    float nx = clip.x / clip.w, ny = clip.y / clip.w;
    sx = nx * .5f + .5f;
    sy = 1.f - (ny * .5f + .5f);
    return std::fabs(nx) <= 1.05f && std::fabs(ny) <= 1.05f;
}
void Game::updateHud() {
    std::lock_guard<std::mutex> l(outMutex_);
    hud_.fill(0);
    const int gun = int(self_[12]);
    const auto spec = br::weaponSpec(gun);
    hud_[H_HEALTH] = self_[9];
    hud_[H_MAX_HEALTH] = 100;
    hud_[H_AMMO] = std::max(0.f, localAmmo_);
    hud_[H_MAG] = float(spec.magazine);
    hud_[H_RELOAD] =
        self_[14] > 0 ? clampf(1.f - self_[14] / std::max(.01f, spec.reload), 0.f, 1.f) : -1.f;
    hud_[H_KILLS] = self_[10];
    Vec3 rpos = visualPlayerPos();
    hud_[H_PLAYER_X] = rpos.x;
    hud_[H_PLAYER_Y] = rpos.y;
    hud_[H_PLAYER_Z] = rpos.z;
    hud_[H_FLIGHT] = float(battle_ ? predictedFlight_ : 3);
    hud_[H_PHASE] = float(battle_ && !snapshot_.empty() ? int(snapshot_[1]) : 0);
    hud_[H_CAM_YAW] = camYaw_;
    hud_[H_WORLD_SIZE] = 1440;
    hud_[H_AIM] = aimBlend_;
    const bool grey = self_[14] > 0 || (fireCooldown_ > 0 && shotFeedback_ <= 0);
    hud_[H_WEAPON_READY] = grey ? 0.f : 1.f;
    hud_[H_CROUCH] = crouching_;
    hud_[H_SPRINTING] = sprinting_;
    {
        std::lock_guard<std::mutex> inputLock(inputMutex_);
        hud_[H_SPRINT_LOCK] = input_.sprint;
    }
    hud_[H_FOV] = fov_;
    hud_[H_WEAPON] = self_[12];
    hud_[H_INV_ENABLED] = 1;
    hud_[H_WEAPON_SLOT] = self_[26];
    hud_[H_ELAPSED] = lastSnapshot_;
    hud_[H_MAG_MUL] = 1;
    hud_[H_TARGET_LOCKED] = targetLocked_ ? 1.f : 0.f;
    hud_[H_TARGET_HP] = targetHp_;
    hud_[H_HITMARK] = clampf(hitMark_ / .18f, 0.f, 1.f);
    hud_[H_HEADSHOT_MARK] = headshotMark_ ? 1.f : 0.f;
    hud_[H_DAMAGE_DIR] = damageDir_;
    hud_[H_DAMAGE_DIR_T] = damageDirTimer_;
    if (!snapshot_.empty()) {
        hud_[H_ALIVE] = snapshot_[21];
        size_t at = snapshot_.size() - 48;
        for (int i = 0; i < 48; ++i)
            hud_[H_INV + i] = snapshot_[at + i];
    }
    int n = 0;
    for (const auto &lb : labels_) {
        if (n >= 24)
            break;
        float lx = 0, ly = 0;
        if (!project(lb.position, lx, ly))
            continue;
        hud_[H_LABELS + n * 5] = lx;
        hud_[H_LABELS + n * 5 + 1] = ly;
        hud_[H_LABELS + n * 5 + 2] = float(lb.kind);
        hud_[H_LABELS + n * 5 + 3] = float(lb.value);
        hud_[H_LABELS + n * 5 + 4] = clampf(lb.life / .5f, 0.f, 1.f);
        ++n;
    }
    hud_[H_LABEL_COUNT] = float(n);
    n = 0;
    for (const auto &[id, a] : actors_) {
        if (n >= 16 || id == localId_ || (int(a.state[3]) & 1) == 0 || int(a.state[11]) == br::IN_PLANE)
            continue;
        if (a.barTimer <= 0 && id != assistTarget_)
            continue;
        if (length(a.position - camPos_) > 55.f)
            continue;
        float h = (int(a.state[3]) & 2) ? .85f : ((int(a.state[3]) & 16) ? 1.25f : 1.75f);
        float bx = 0, by = 0;
        if (!project(a.position + Vec3(0, h + .3f, 0), bx, by))
            continue;
        int o = H_BARS + n * 5;
        hud_[o] = bx;
        hud_[o + 1] = by;
        hud_[o + 2] = clampf(a.state[9] / 100.f, 0.f, 1.f);
        hud_[o + 3] = a.barTimer > 0 ? clampf(a.barTimer / .4f, 0.f, 1.f) : .85f;
        hud_[o + 4] = 0.f;
        ++n;
    }
    hud_[H_BAR_COUNT] = float(n);
    hud_[H_PERF_PENDING] = loading_ ? (1 - world_.finitePreloadProgress()) * 100 : 0;
    hud_[H_FPS] = fps_;
}
void Game::frame(double now) {
    float dt = lastFrame_ ? clampf(float(now - lastFrame_), 0, .1f) : .016f;
    lastFrame_ = now;
    if (dt > .001f)
        fps_ = lerpf(fps_, 1.f / dt, damp(2, dt));
    time_ += dt;
    InputState in;
    {
        std::lock_guard<std::mutex> l(inputMutex_);
        in = input_;
        in.fire = in.fire || (in.buttons & br::FIRE) != 0;
        input_.lookX = input_.lookY = 0;
        input_.shoulderSwaps = 0;
    }
    crouching_ = in.crouch;
    throwAiming_ = in.throwAim;
    localAimStage_ = in.aimStage;
    if (loading_) {
        try {
            world_.preloadFinite(32, 24);
            if (world_.finitePreloadReady() && !expandedReleased_) {
                source_->map.releaseExpandedForClient();
                expandedReleased_ = true;
            }
        } catch (const std::exception &e) {
            LOGE("Preload: %s", e.what());
            loading_ = false;
            emit(101);
        }
    }
    if (!battle_ || (loading_ && world_.finitePreloadReady()))
        world_.rebuildDirty(24);
    if (ready_) {
        inputTime_ += dt;
        while (inputTime_ >= .05f) {
            inputTime_ -= .05f;
            tickInput();
        }
    }
    // Smooth sub-tick visual prediction at display refresh rate (60/120 Hz).
    if (battle_ && !snapshot_.empty() && snapshot_[1] == br::PLANE && planePosInit_) {
        planeSnapAge_ = std::min(planeSnapAge_ + dt, .45f);
        visualPlanePos_ += planeVel_ * dt;
        const Vec3 expectedPlane = serverPlanePos_ + planeVel_ * planeSnapAge_;
        visualPlanePos_ = lerp(visualPlanePos_, expectedPlane, damp(7.f, dt));
        visualPlanePos_.y = serverPlanePos_.y;
        if (predictedFlight_ == br::IN_PLANE) {
            player_.position = visualPlanePos_;
            correction_ = {};
        }
    }
    if (battle_ && !snapshot_.empty() && zoneInit_) {
        const int phase = int(snapshot_[1]), stage = int(snapshot_[11]);
        const bool shrinking = snapshot_[12] > .5f;
        float targetX = snapshot_[4], targetZ = snapshot_[5], targetR = snapshot_[6];
        if (stage == 0 && !shrinking) {
            const float totalWait = std::max(30.f, snapshot_[22] + snapshot_[10]);
            const float waitProg = phase == br::ACTIVE ? clampf(snapshot_[22] / totalWait, 0.f, 1.f) : 0.f;
            targetX = 720.f;
            targetZ = 720.f;
            targetR = lerpf(1050.f, snapshot_[6], waitProg);
        }
        visualZoneCenter_ = lerp(visualZoneCenter_, {targetX, 0.f, targetZ}, damp(6.f, dt));
        visualZoneRadius_ = lerpf(visualZoneRadius_, targetR, damp(6.f, dt));
    }
    visualPlayer_ = player_;
    visualPlayer_.stepOffset = 0;
    if (ready_ && !paused_ && !snapshot_.empty() && br::clientInputActive(snapshot_[1]) &&
        (int(self_[3]) & 1) != 0 && inputTime_ > 1e-4f) {
        CollisionQuery q{*this};
        if (predictedFlight_ == br::LANDED) {
            auto visMotion = motion_;
            bool wantSprint = in.sprint && !in.crouch && !in.fire && !in.aim && !in.throwAim;
            mission::stepGroundMovement(q, visualPlayer_, visMotion, in.moveX, in.moveY, camYaw_, wantSprint,
                                        in.crouch, false, int(self_[3]) & 2, inputTime_);
        } else if (predictedFlight_ != br::IN_PLANE) {
            int visFlight = predictedFlight_;
            float altitude = visualPlayer_.position.y -
                             source_->map.heightAt(int(visualPlayer_.position.x), int(visualPlayer_.position.z));
            mission::stepDescent(q, visualPlayer_, visFlight, in.moveX, in.moveY, camYaw_, false, altitude,
                                 inputTime_);
            visualPlayer_.stepOffset = 0;
        }
    }
    sprinting_ = in.sprint && !crouching_ && lengthXZ(visualPlayer_.velocity) > 3.5f && !in.fire && !in.aim &&
                 !throwAiming_;
    correction_ = correction_ * std::exp(-14.f * dt);
    visualStep_ = lerpf(visualStep_, 0.f, damp(16.f, dt));
    for (auto &[id, a] : actors_) {
        if (id == localId_)
            a.position = visualPlayerPos();
        else
            a.position = lerp(a.from, a.to, clampf((time_ - a.sample) / .1f, 0, 1));
        updateActor(a, id == localId_, in, dt);
    }
    if (!battle_) {
        auto *asset = character(look_);
        lobbyAnim_.update(dt);
        if (lobbyAnim_.finished(asset->model))
            lobbyAnim_.play(asset->model.clipIndex("idle"), true, .25f);
    }
    updateCamera(dt, in);
    updateWeapon(dt, in);
    updateEffects(dt);
    buildScene();
    renderer_.render(scene_);
    if (loading_ && world_.finitePreloadReady() && renderer_.worldGpuReady(world_)) {
        ready_ = true;
        loading_ = false;
        emit(100);
    }
    updateHud();
#ifndef NDEBUG
    if (battle_ && ready_ && !snapshot_.empty() && time_ - lastTelemetry_ > 2) {
        lastTelemetry_ = time_;
        LOGI("BR state phase=%.0f seq=%u flight=%d pos=%.2f,%.2f,%.2f hp=%.1f gun=%.0f ammo=%.0f",
             snapshot_[1], sequence_, predictedFlight_, player_.position.x, player_.position.y,
             player_.position.z, self_[9], self_[12], self_[13]);
    }
#endif
}
void Game::setPaused(bool b) {
    paused_ = b;
    std::lock_guard<std::mutex> l(inputMutex_);
    input_ = {};
}
void Game::setSettings(float s, bool a, bool y) {
    sensitivity_ = clampf(s, .3f, 3);
    aimAssist_ = a;
    invertY_ = y;
}
void Game::setGraphics(int q) {
    quality_ = std::max(0, std::min(2, q));
    scene_.lodNearDistance = quality_ == 0 ? 80 : 140;
    scene_.lodFarDistance = quality_ == 0 ? 280 : 420;
}
void Game::setMove(float x, float y) {
    std::lock_guard<std::mutex> l(inputMutex_);
    input_.moveX = clampf(x, -1, 1);
    input_.moveY = clampf(y, -1, 1);
    if (y < -.5f && -y > std::fabs(x))
        input_.sprint = false;
}
void Game::addLook(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    std::lock_guard<std::mutex> l(inputMutex_);
    input_.lookX += clampf(x, -300, 300);
    input_.lookY += clampf(y, -300, 300);
}
void Game::setFire(bool b) {
    std::lock_guard<std::mutex> l(inputMutex_);
    input_.fire = b;
    if (b)
        input_.buttons |= br::FIRE;
}
void Game::setAim(bool b) {
    std::lock_guard<std::mutex> l(inputMutex_);
    input_.aim = b;
    input_.aimStage = b ? 1 : 0;
}
void Game::setAimStage(int n) {
    std::lock_guard<std::mutex> l(inputMutex_);
    input_.aimStage = std::max(0, std::min(2, n));
    input_.aim = input_.aimStage > 0;
}
void Game::action(int bits, int target, float x, float z) {
    std::lock_guard<std::mutex> l(inputMutex_);
    input_.buttons |= uint32_t(bits) & 32767;
    input_.target = std::max(-1, std::min(49, target));
    input_.markX = clampf(x, 0, 1440);
    input_.markZ = clampf(z, 0, 1440);
}
void Game::pressJump() { action(br::JUMP, -1, 0, 0); }
void Game::pressReload() { action(br::RELOAD, -1, 0, 0); }
void Game::toggleCrouch() {
    std::lock_guard<std::mutex> l(inputMutex_);
    input_.crouch = !input_.crouch;
    if (input_.crouch)
        input_.sprint = false;
}
void Game::toggleSprint() {
    std::lock_guard<std::mutex> l(inputMutex_);
    input_.sprint = !input_.sprint;
    if (input_.sprint)
        input_.crouch = false;
}
void Game::pressMelee() { action(br::MELEE, -1, 0, 0); }
void Game::pressGrenade() { action(br::THROW_GRENADE, -1, 0, 0); }
void Game::swapShoulder() {
    std::lock_guard<std::mutex> l(inputMutex_);
    ++input_.shoulderSwaps;
}
void Game::tapSlot(int n) {
    if (n < 0 || n >= 24)
        return;
    std::lock_guard<std::mutex> l(inputMutex_);
    input_.slot = n;
}
void Game::useSlot(int n) { tapSlot(n); }
void Game::dropFromPlayer(int id, int n) {
    if (!br::validItem(id) || n < 1 || n > 60)
        return;
    std::lock_guard<std::mutex> l(inputMutex_);
    input_.dropItem = id;
    input_.dropCount = n;
    input_.buttons |= br::DROP;
}
void Game::setThrowAim(bool held, bool cancel) {
    std::lock_guard<std::mutex> l(inputMutex_);
    if (!held && input_.throwAim && !cancel)
        input_.buttons |= br::THROW_GRENADE;
    input_.throwAim = held;
}
void Game::copyHud(float *out, int n) {
    std::lock_guard<std::mutex> l(outMutex_);
    std::copy_n(hud_.begin(), std::min(n, int(hud_.size())), out);
}
int Game::drainEvents(int *out, int n) {
    std::lock_guard<std::mutex> l(outMutex_);
    n = std::min(n, int(events_.size()));
    n -= n % 4;
    std::copy_n(events_.begin(), n, out);
    events_.erase(events_.begin(), events_.begin() + n);
    return n;
}
void Game::copyMinimap(uint32_t *out, int n) {
    std::copy_n(mapImage_.begin(), std::min(n, int(mapImage_.size())), out);
}
} // namespace zz
