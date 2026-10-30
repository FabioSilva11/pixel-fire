#include "battle_royale.h"
#include <algorithm>
#include <set>
#include <limits>

namespace zz::br {
namespace {
float distanceXZ(const Vec3 &a, const Vec3 &b) {
    const float x = a.x - b.x, z = a.z - b.z;
    return std::sqrt(x * x + z * z);
}
bool containsWall(const IceWall &w, const Vec3 &p, float padding = 0) {
    Vec3 d = p - w.pos;
    float c = std::cos(w.yaw), s = std::sin(w.yaw);
    return std::abs(d.x * c - d.z * s) <= 2.1f + padding && std::abs(d.x * s + d.z * c) <= .35f + padding &&
           d.y >= -padding && d.y <= 2.7f + padding;
}
Vec3 direction(float yaw, float pitch) {
    return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
}
} // namespace

BattleRoyale::BattleRoyale(const Config &c) : config_(c), rng_(c.seed) {
    config_.teamSize = c.teamSize == 2 || c.teamSize == 4 ? c.teamSize : 1;
    const auto duration = [](float v, float fallback, float low, float high) {
        return std::isfinite(v) ? clampf(v, low, high) : fallback;
    };
    config_.waitSeconds = duration(c.waitSeconds, 15, 0, 120);
    config_.planeSeconds = duration(c.planeSeconds, 50, .1f, 120);
    config_.zoneWait = duration(c.zoneWait, 55, .1f, 300);
    config_.zoneShrink = duration(c.zoneShrink, 35, .1f, 180);
    config_.bleedSeconds = duration(c.bleedSeconds, 30, 1, 120);
    config_.reviveSeconds = duration(c.reviveSeconds, 5, 1, 30);
    config_.reconnectSeconds = duration(c.reconnectSeconds, 30, 1, 120);
    const bool east = rng_.chance(.5f);
    float lane = rng_.range(250, 1190);
    routeA_ = east ? Vec3(40, 190, lane) : Vec3(lane, 190, 40);
    routeB_ = east ? Vec3(1400, 190, 1440 - lane) : Vec3(1440 - lane, 190, 1400);
    zone_.timer = std::max(.1f, config_.zoneWait);
    chooseNextZone();
    // Each structure has original, deterministic nearby loot. No campaign items enter this inventory.
    for (const auto &b : map_.buildings()) {
        Vec3 pos = map_.doorPosition(b);
        // Outside the door, away from the wall. Structure rotation defines front direction.
        Vec3 outward = direction(float(b.rotation) * kPi / 180, 0) * -3.0f;
        pos += outward;
        pos.y = float(map_.heightAt(int(pos.x), int(pos.z)));
        const int gun = rng_.rangeInt(PISTOL, SNIPER);
        createLoot(pos, gun, 1);
        createLoot(pos + Vec3(1, 0, 0), weaponSpec(gun).ammo, 60);
        createLoot(pos + Vec3(-1, 0, 0), rng_.rangeInt(VEST, ADRENALINE), rng_.rangeInt(1, 2));
        // Interior loot encourages entering buildings.
        createLoot({float(b.x), float(b.floor), float(b.z)}, MEDKIT, 2);
    }
}
bool BattleRoyale::join(int id, int team, int look, bool bot) {
    if (id < 0 || id >= kMaxPlayers || team < 0 || team >= kMaxPlayers || players_[id].present ||
        phase_ != WAITING)
        return false;
    int members = 0;
    for (const auto &p : players_)
        if (p.present && p.team == team)
            ++members;
    if (members >= config_.teamSize)
        return false;
    auto &p = players_[id];
    p.present = true;
    p.id = id;
    p.team = team;
    p.look = look;
    p.bot = bot;
    p.body.position = routeA_;
    return true;
}
bool BattleRoyale::submitInput(int id, const InputCommand &in) {
    if (id < 0 || id >= kMaxPlayers || !players_[id].present || !players_[id].alive ||
        !players_[id].connected || ended())
        return false;
    if (!std::isfinite(in.moveX) || !std::isfinite(in.moveZ) || !std::isfinite(in.yaw) ||
        !std::isfinite(in.pitch) || !std::isfinite(in.markX) || !std::isfinite(in.markZ) ||
        std::abs(in.moveX) > 1.001f || std::abs(in.moveZ) > 1.001f || std::abs(in.pitch) > 1.55f ||
        std::abs(in.yaw) > 10000 || in.buttons > 32767 || in.slot < -1 || in.slot >= 24 || in.target < -1 ||
        in.target >= kMaxPlayers || in.dropCount < 0 || in.dropCount > 10000)
        return false;
    auto &p = players_[id];
    if (in.sequence == 0 || in.sequence > 16777215 || in.sequence <= p.receivedSequence ||
        in.sequence - p.receivedSequence > 1024 || p.pendingInputs.size() >= 32)
        return false;
    if ((in.buttons & DROP) && (!validItem(in.dropItem) || in.dropCount <= 0))
        return false;
    p.receivedSequence = in.sequence;
    p.pendingInputs.push_back(in);
    return true;
}
void BattleRoyale::setConnected(int id, bool value) {
    if (id < 0 || id >= kMaxPlayers || !players_[id].present)
        return;
    auto &p = players_[id];
    p.connected = value;
    if (value)
        p.disconnected = 0;
    else {
        p.pendingInputs.clear();
        p.input.buttons = 0;
        p.input.moveX = p.input.moveZ = 0;
        p.sinceInput = 1;
    }
}
Vec3 BattleRoyale::planePosition() const {
    float t = clampf(phaseTime_ / std::max(.1f, config_.planeSeconds), 0, 1);
    return routeA_ + (routeB_ - routeA_) * t;
}
void BattleRoyale::createLoot(const Vec3 &p, int item, int count) {
    if (!validItem(item) || count <= 0 || count > 10000 || nextEntity_ > 16777215)
        return;
    LootEntity value{nextEntity_, item, count, p, false};
    for (auto &existing : loot_)
        if (existing.taken) {
            existing = value;
            ++nextEntity_;
            return;
        }
    if (loot_.size() >= 1800)
        return;
    loot_.push_back(value);
    ++nextEntity_;
}
bool BattleRoyale::lootRoom(int count) const {
    size_t free = 1800 - loot_.size();
    for (const auto &l : loot_)
        if (l.taken)
            ++free;
    return count > 0 && free >= size_t(count) && nextEntity_ + count <= 16777216;
}
void BattleRoyale::forfeit(int id) {
    if (id < 0 || id >= kMaxPlayers || !players_[id].present)
        return;
    auto &p = players_[id];
    p.connected = false;
    p.pendingInputs.clear();
    if (p.alive && phase_ != FINISHED)
        eliminate(id, -1);
}
bool BattleRoyale::isWaterAt(const Vec3 &p) const {
    const int x = int(std::floor(p.x)), y = int(std::floor(p.y)), z = int(std::floor(p.z));
    const uint8_t b = map_.blockAt(x, y, z);
    return isWaterBlock(b) && p.y - y < waterHeight(b, isWaterBlock(map_.blockAt(x, y + 1, z)));
}
bool BattleRoyale::boxCollides(const Vec3 &mn, const Vec3 &mx) const {
    if (mn.x < 1 || mn.z < 1 || mx.x > 1439 || mx.z > 1439)
        return true;
    for (int z = int(std::floor(mn.z)); z <= int(std::floor(mx.z)); ++z)
        for (int x = int(std::floor(mn.x)); x <= int(std::floor(mx.x)); ++x)
            for (int y = int(std::floor(mn.y)); y <= int(std::floor(mx.y)); ++y)
                if (isSolid(map_.blockAt(x, y, z)))
                    return true;
    for (const auto &w : walls_) {
        if (w.life <= 0 || w.health <= 0)
            continue;
        if (mx.y <= w.pos.y || mn.y >= w.pos.y + 2.7f)
            continue;
        const Vec3 mid = (mn + mx) * .5f;
        const float hx = (mx.x - mn.x) * .5f, hz = (mx.z - mn.z) * .5f;
        const float c = std::cos(w.yaw), s = std::sin(w.yaw), dx = mid.x - w.pos.x, dz = mid.z - w.pos.z;
        // Separating axes for the oriented ice rectangle and the query's axis-aligned box.
        if (std::abs(dx) > hx + 2.1f * std::abs(c) + .35f * std::abs(s) ||
            std::abs(dz) > hz + 2.1f * std::abs(s) + .35f * std::abs(c))
            continue;
        if (std::abs(dx * c - dz * s) <= 2.1f + hx * std::abs(c) + hz * std::abs(s) &&
            std::abs(dx * s + dz * c) <= .35f + hx * std::abs(s) + hz * std::abs(c))
            return true;
    }
    return false;
}
float BattleRoyale::obstruction(const Vec3 &from, const Vec3 &dir, float range, int *wallIndex) const {
    // Voxel DDA: no distance-dependent 0.1m sampling and no tunnelling through thin cover.
    int cell[3] = {int(std::floor(from.x)), int(std::floor(from.y)), int(std::floor(from.z))};
    int sign[3];
    float next[3], delta[3];
    for (int i = 0; i < 3; ++i) {
        sign[i] = dir[i] >= 0 ? 1 : -1;
        delta[i] = std::abs(dir[i]) < 1e-7f ? 1e20f : std::abs(1 / dir[i]);
        next[i] =
            std::abs(dir[i]) < 1e-7f ? 1e20f : ((sign[i] > 0 ? cell[i] + 1 : cell[i]) - from[i]) / dir[i];
    }
    float hit = range;
    for (float t = 0; t < range;) {
        if (isSolid(map_.blockAt(cell[0], cell[1], cell[2]))) {
            hit = t;
            break;
        }
        int a = next[0] < next[1] ? (next[0] < next[2] ? 0 : 2) : (next[1] < next[2] ? 1 : 2);
        t = next[a];
        next[a] += delta[a];
        cell[a] += sign[a];
        if (cell[1] > 250 && sign[1] > 0)
            break;
    }
    for (size_t i = 0; i < walls_.size(); ++i) {
        const auto &w = walls_[i];
        if (w.health <= 0 || w.life <= 0)
            continue;
        const float c = std::cos(w.yaw), s = std::sin(w.yaw);
        Vec3 p = from - w.pos;
        Vec3 o(p.x * c - p.z * s, p.y, p.x * s + p.z * c);
        Vec3 d(dir.x * c - dir.z * s, dir.y, dir.x * s + dir.z * c);
        float lo = 0, hi = hit;
        float mn[3] = {-2.1f, 0, -.35f}, mx[3] = {2.1f, 2.7f, .35f};
        for (int a = 0; a < 3 && lo <= hi; ++a) {
            if (std::abs(d[a]) < 1e-7f) {
                if (o[a] < mn[a] || o[a] > mx[a])
                    lo = hi + 1;
            } else {
                float a0 = (mn[a] - o[a]) / d[a], a1 = (mx[a] - o[a]) / d[a];
                if (a0 > a1)
                    std::swap(a0, a1);
                lo = std::max(lo, a0);
                hi = std::min(hi, a1);
            }
        }
        if (lo <= hi && lo < hit) {
            hit = lo;
            if (wallIndex)
                *wallIndex = int(i);
        }
    }
    return hit;
}
bool BattleRoyale::lineOfSight(const Vec3 &a, const Vec3 &b) const {
    const Vec3 d = b - a;
    const float len = length(d);
    return len < .01f || obstruction(a, d / len, len) >= len - .05f;
}
void BattleRoyale::chooseNextZone() {
    const float next = zone_.stage >= 7 ? 0 : zone_.radius * .66f;
    const float offset = std::sqrt(rng_.range(.25f, 1.0f)) * std::max(0.0f, zone_.radius - next);
    const float angle = rng_.range(0, kTwoPi);
    zone_.nextRadius = next;
    zone_.nextX = clampf(zone_.x + std::sin(angle) * offset, next, 1440.0f - next);
    zone_.nextZ = clampf(zone_.z + std::cos(angle) * offset, next, 1440.0f - next);
}
void BattleRoyale::advanceZone(float dt) {
    zone_.timer -= dt;
    if (zone_.shrinking) {
        const float remaining = std::max(dt, zone_.timer + dt), fraction = std::min(1.0f, dt / remaining);
        zone_.x = lerpf(zone_.x, zone_.nextX, fraction);
        zone_.z = lerpf(zone_.z, zone_.nextZ, fraction);
        zone_.radius = lerpf(zone_.radius, zone_.nextRadius, fraction);
    }
    if (zone_.timer <= 0) {
        if (zone_.shrinking) {
            zone_.x = zone_.nextX;
            zone_.z = zone_.nextZ;
            zone_.radius = zone_.nextRadius;
            ++zone_.stage;
            zone_.shrinking = false;
            zone_.timer = std::max(.1f, config_.zoneWait * .85f);
            chooseNextZone();
        } else {
            zone_.shrinking = true;
            zone_.timer = std::max(.1f, config_.zoneShrink);
        }
    }
    for (auto &p : players_)
        if (p.present && p.alive && p.flight == LANDED) {
            if (distanceXZ(p.body.position, {zone_.x, 0, zone_.z}) > zone_.radius)
                damage(p.id, dt * std::min(35.0f, 2.0f + zone_.stage * 3.0f), -1);
        }
}
int BattleRoyale::aliveTeams() const {
    std::set<int> teams;
    for (const auto &p : players_)
        if (p.present && p.alive)
            teams.insert(p.team);
    return int(teams.size());
}
void BattleRoyale::eliminate(int id, int attacker) {
    auto &p = players_[id];
    if (!p.alive)
        return;
    p.alive = false;
    p.down = false;
    p.health = 0;
    p.placement = 0;
    if (attacker >= 0 && attacker < kMaxPlayers && players_[attacker].present &&
        players_[attacker].team != p.team)
        ++players_[attacker].kills;
    if (p.equipped >= 0 && p.equipped < 24 && weapon(p.inventory.slots[p.equipped].id))
        p.clips[p.inventory.slots[p.equipped].id] = int(p.ammo);
    for (const auto &s : p.inventory.slots)
        if (s.count > 0)
            createLoot(p.body.position, s.id, s.count);
    for (int gun = PISTOL; gun <= SNIPER; ++gun)
        if (p.clips[gun] > 0)
            createLoot(p.body.position, weaponSpec(gun).ammo, p.clips[gun]);
    p.inventory = Inventory{};
    p.clips.fill(0);
    p.ammo = 0;
    p.equipped = -1;
    p.reload = 0;
    p.pendingInputs.clear();
    p.input.buttons = 0;
    bool standingAlly = false;
    for (const auto &ally : players_)
        if (ally.present && ally.alive && !ally.down && ally.team == p.team)
            standingAlly = true;
    if (!standingAlly)
        for (auto &ally : players_)
            if (ally.present && ally.alive && ally.down && ally.team == p.team)
                eliminate(ally.id, ally.attacker);
    bool teamAlive = false;
    for (const auto &ally : players_)
        if (ally.present && ally.alive && ally.team == p.team)
            teamAlive = true;
    if (!teamAlive) {
        const int rank = std::max(1, aliveTeams() + 1);
        for (auto &ally : players_)
            if (ally.present && ally.team == p.team)
                ally.placement = rank;
    }
}
void BattleRoyale::damage(int id, float amount, int attacker, bool head) {
    if (id < 0 || id >= kMaxPlayers || !std::isfinite(amount) || amount <= 0)
        return;
    auto &p = players_[id];
    if (!p.present || !p.alive || p.flight == IN_PLANE)
        return;
    if (attacker >= 0 && (attacker >= kMaxPlayers || !players_[attacker].present))
        return;
    if (attacker >= 0 && attacker != id && !config_.friendlyFire && players_[attacker].team == p.team)
        return;
    p.hurtThisTick = true;
    for (auto &q : players_)
        if (q.reviver == id) {
            q.revive = 0;
            q.reviver = -1;
            q.reviveThisTick = false;
        }
    p.heal = 0;
    p.revive = 0;
    p.reviver = -1;
    p.reviveThisTick = false;
    p.attacker = attacker;
    if (attacker >= 0) {
        int &equipment = head ? p.inventory.helmet : p.inventory.vest;
        const float absorb = std::min(float(equipment), amount * .45f);
        equipment = std::max(0, equipment - int(std::ceil(absorb)));
        amount -= absorb;
    }
    if (p.down) {
        p.bleed -= amount;
        if (p.bleed <= 0)
            eliminate(id, attacker);
        return;
    }
    p.health -= amount;
    if (p.health > 0)
        return;
    bool ally = false;
    for (const auto &q : players_)
        if (q.present && q.alive && !q.down && q.id != id && q.team == p.team)
            ally = true;
    if (config_.teamSize > 1 && ally) {
        p.down = true;
        p.health = 1;
        p.bleed = config_.bleedSeconds;
        p.revive = 0;
        p.movement.mantle.time = -1;
    } else
        eliminate(id, attacker);
}
void BattleRoyale::fire(PlayerState &p, int item) {
    const auto spec = weaponSpec(item);
    if (p.cooldown > 0 || p.reload > 0 || p.down || p.flight != LANDED)
        return;
    if (item != KNIFE && p.ammo < 1)
        return;
    if (item != KNIFE) {
        --p.ammo;
        ++p.shotCount;
    }
    p.cooldown = spec.interval;
    p.shot = .12f;
    const Vec3 origin = p.body.position + Vec3(0, p.input.buttons & CROUCH ? 1.05f : 1.5f, 0);
    for (int pellet = 0; pellet < spec.pellets; ++pellet) {
        const float spread = spec.pellets > 1 ? .055f : (item == SMG ? .012f : .003f);
        const Vec3 dir =
            direction(p.yaw + rng_.range(-spread, spread), p.pitch + rng_.range(-spread, spread));
        int wall = -1;
        float nearest = obstruction(origin, dir, spec.range, &wall);
        int target = -1;
        bool head = false;
        for (const auto &q : players_) {
            if (!q.present || !q.alive || q.id == p.id || (!config_.friendlyFire && q.team == p.team) ||
                q.flight == IN_PLANE)
                continue;
            float h = q.down ? .6f : ((q.input.buttons & CROUCH) ? 1.2f : 1.8f);
            float lo = 0, hi = nearest;
            Vec3 mn = q.body.position + Vec3(-.34f, 0, -.34f), mx = q.body.position + Vec3(.34f, h, .34f);
            for (int a = 0; a < 3 && lo <= hi; ++a) {
                if (std::abs(dir[a]) < 1e-7f) {
                    if (origin[a] < mn[a] || origin[a] > mx[a])
                        lo = hi + 1;
                } else {
                    float a0 = (mn[a] - origin[a]) / dir[a], a1 = (mx[a] - origin[a]) / dir[a];
                    if (a0 > a1)
                        std::swap(a0, a1);
                    lo = std::max(lo, a0);
                    hi = std::min(hi, a1);
                }
            }
            if (lo <= hi && lo < nearest) {
                target = q.id;
                nearest = lo;
                head = origin.y + dir.y * lo > q.body.position.y + h * .78f;
            }
        }
        if (target >= 0)
            damage(target, spec.damage * (head ? 1.7f : 1), p.id, head);
        else if (wall >= 0)
            walls_[wall].health -= spec.damage;
    }
}
void BattleRoyale::actions(PlayerState &p, float dt) {
    auto &in = p.input;
    auto &inv = p.inventory;
    if (in.slot >= 0 && in.slot < inv.capacity() && inv.slots[in.slot].count > 0) {
        const int item = inv.slots[in.slot].id;
        if (weapon(item) && p.equipped != in.slot) {
            if (p.equipped >= 0 && weapon(inv.slots[p.equipped].id))
                p.clips[inv.slots[p.equipped].id] = int(p.ammo);
            p.equipped = in.slot;
            p.ammo = float(p.clips[item]);
            p.reload = 0;
        }
        if (item == VEST && inv.vest < 100) {
            inv.take(item, 1);
            inv.vest = 100;
        }
        if (item == HELMET && inv.helmet < 80) {
            inv.take(item, 1);
            inv.helmet = 80;
        }
        if (item == BACKPACK && inv.backpack < 2) {
            inv.take(item, 1);
            ++inv.backpack;
        }
    }
    {
        LootEntity *closest = nullptr;
        float best = in.buttons & INTERACT ? 3.2f : 1.4f;
        for (auto &loot : loot_)
            if (!loot.taken && !(loot.droppedBy == p.id && time_ < loot.pickupAfter) &&
                std::abs(loot.pos.y - p.body.position.y) < 1.8f) {
                const float d = distanceXZ(loot.pos, p.body.position);
                if (d < best && lineOfSight(p.body.position + Vec3(0, .8f, 0), loot.pos + Vec3(0, .4f, 0))) {
                    best = d;
                    closest = &loot;
                }
            }
        if (closest) {
            int taken = inv.add(closest->item, closest->count);
            closest->count -= taken;
            closest->taken = closest->count <= 0;
        }
        if (p.equipped < 0)
            for (int i = 0; i < inv.capacity(); ++i)
                if (weapon(inv.slots[i].id) && inv.slots[i].count > 0) {
                    p.equipped = i;
                    p.ammo = float(p.clips[inv.slots[i].id]);
                    if (p.ammo < 1.f && inv.slots[i].id != KNIFE) {
                        const auto ws = weaponSpec(inv.slots[i].id);
                        const int fill = std::min(ws.magazine, inv.count(ws.ammo));
                        if (fill > 0 && inv.take(ws.ammo, fill)) {
                            p.ammo = float(fill);
                            p.clips[inv.slots[i].id] = fill;
                        }
                    }
                    break;
                }
    }
    const bool droppingEquipped = p.equipped >= 0 && inv.slots[p.equipped].id == in.dropItem;
    if (droppingEquipped)
        p.clips[in.dropItem] = int(p.ammo);
    const int dropEntities = weapon(in.dropItem) && p.clips[in.dropItem] > 0 ? 2 : 1;
    if ((in.buttons & DROP) && validItem(in.dropItem) && in.dropCount > 0 && lootRoom(dropEntities) &&
        inv.take(in.dropItem, in.dropCount)) {
        createLoot(p.body.position + direction(p.yaw, 0) * 1.2f, in.dropItem, in.dropCount);
        if (weapon(in.dropItem)) {
            if (p.clips[in.dropItem] > 0)
                createLoot(p.body.position, weaponSpec(in.dropItem).ammo, p.clips[in.dropItem]);
            p.clips[in.dropItem] = 0;
        }
        for (auto &l : loot_)
            if (l.id >= nextEntity_ - dropEntities) {
                l.droppedBy = p.id;
                l.pickupAfter = time_ + 1;
            }
        if (p.equipped >= 0 && !weapon(inv.slots[p.equipped].id)) {
            p.equipped = -1;
            p.ammo = 0;
            p.reload = 0;
        }
    }
    int gun = p.equipped >= 0 ? inv.slots[p.equipped].id : NONE;
    if (weapon(gun)) {
        const auto w = weaponSpec(gun);
        if (p.reload > 0) {
            p.reload -= dt;
            if (p.reload <= 0) {
                int n = std::min(w.magazine - int(p.ammo), inv.count(w.ammo));
                if (n > 0 && inv.take(w.ammo, n))
                    p.ammo += n;
            }
        }
        if (((in.buttons & RELOAD) || (in.buttons & FIRE && p.ammo < 1)) && p.reload <= 0 && gun != KNIFE &&
            p.ammo < w.magazine && inv.count(w.ammo) > 0)
            p.reload = w.reload;
        if (in.buttons & FIRE)
            fire(p, gun);
    }
    if (in.buttons & MELEE)
        fire(p, KNIFE);
    if (weapon(gun))
        p.clips[gun] = int(p.ammo);
    if (in.buttons & HEAL) {
        const int item = inv.count(MEDKIT) > 0 ? MEDKIT : (inv.count(BANDAGE) > 0 ? BANDAGE : ADRENALINE);
        if (p.health < 100 && inv.count(item) > 0 && !p.hurtThisTick && lengthXZ(p.body.velocity) < .5f &&
            !(in.buttons & (FIRE | MELEE))) {
            p.heal += dt;
            if (p.heal >= 3.0f && inv.take(item, 1)) {
                p.health = std::min(100.0f, p.health + (item == MEDKIT ? 70 : 25));
                p.heal = 0;
            }
        } else
            p.heal = 0;
    } else
        p.heal = 0;
    if ((in.buttons & PLACE_ICE) && p.iceCooldown <= 0 && inv.count(ICE) > 0 && walls_.size() < 128) {
        Vec3 pos = p.body.position + direction(p.yaw, 0) * 3.2f;
        pos.y = map_.heightAt(int(pos.x), int(pos.z));
        IceWall w{nextEntity_, p.id, pos, p.yaw, 250, 45};
        bool valid =
            pos.x > 4 && pos.z > 4 && pos.x < 1436 && pos.z < 1436 && std::abs(pos.y - p.body.position.y) < 2;
        for (int x = -2; x <= 2 && valid; ++x)
            for (int y = 0; y < 3 && valid; ++y) {
                const Vec3 q = pos + Vec3(std::cos(p.yaw) * x, float(y), -std::sin(p.yaw) * x);
                if (map_.solidAt(q.x, q.y, q.z))
                    valid = false;
            }
        for (const auto &q : players_)
            if (q.present && q.alive && containsWall(w, q.body.position + Vec3(0, .5f, 0), .5f))
                valid = false;
        for (const auto &q : walls_)
            if (distanceXZ(q.pos, pos) < 4.5f)
                valid = false;
        if (valid && lineOfSight(p.body.position + Vec3(0, 1.2f, 0), pos + Vec3(0, 1.2f, 0)) &&
            inv.take(ICE, 1)) {
            ++nextEntity_;
            walls_.push_back(w);
            p.iceCooldown = 2;
        }
    }
    const int throwable = in.buttons & THROW_GRENADE ? GRENADE : (in.buttons & THROW_SMOKE ? SMOKE : NONE);
    if (throwable && p.cooldown <= 0 && grenades_.size() < 100 && inv.take(throwable, 1)) {
        grenades_.push_back(
            {p.id, throwable, p.body.position + Vec3(0, 1.5f, 0), direction(p.yaw, p.pitch - .25f) * 16, 3});
        p.cooldown = 1;
    }
    if ((in.buttons & PING) && in.markX >= 0 && in.markZ >= 0 && in.markX < 1440 && in.markZ < 1440) {
        p.markX = in.markX;
        p.markZ = in.markZ;
    }
    // Revive is held and server-timed. Only one living teammate channels a given victim.
    if ((in.buttons & REVIVE) && in.target >= 0 && in.target < kMaxPlayers && !p.hurtThisTick &&
        lengthXZ(p.body.velocity) < .5f && !(in.buttons & (FIRE | MELEE))) {
        auto &q = players_[in.target];
        if (q.present && q.alive && q.down && !q.hurtThisTick && !q.reviveThisTick &&
            (q.reviver < 0 || q.reviver == p.id) && q.team == p.team &&
            length(p.body.position - q.body.position) < 2.5f &&
            lineOfSight(p.body.position + Vec3(0, .6f, 0), q.body.position + Vec3(0, .3f, 0))) {
            q.reviver = p.id;
            q.reviveThisTick = true;
            q.revive += dt;
            if (q.revive + 1e-5f >= config_.reviveSeconds) {
                q.down = false;
                q.health = 30;
                q.revive = 0;
                q.bleed = 0;
                q.reviver = -1;
            }
        }
    }
    // One-shot commands are consumed once. Fire/heal/revive remain held only until input timeout.
    in.buttons &= (FIRE | SPRINT | CROUCH | HEAL | REVIVE);
    in.slot = -1;
    in.dropCount = 0;
}
void BattleRoyale::playerTick(PlayerState &p, float dt) {
    p.sinceInput += dt;
    p.cooldown = std::max(0.0f, p.cooldown - dt);
    p.shot = std::max(0.0f, p.shot - dt);
    p.iceCooldown = std::max(0.f, p.iceCooldown - dt);
    if (!p.connected && !p.bot) {
        p.disconnected += dt;
        if (p.disconnected > config_.reconnectSeconds) {
            eliminate(p.id, -1);
            return;
        }
    }
    if (p.connected && !p.pendingInputs.empty()) {
        p.input = p.pendingInputs.front();
        p.pendingInputs.pop_front();
        p.sequence = p.input.sequence;
        p.sinceInput = 0;
    }
    if (p.bot)
        botInput(p, dt);
    if (p.sinceInput > .35f) {
        p.input.moveX = p.input.moveZ = 0;
        p.input.buttons = 0;
    }
    p.yaw = p.input.yaw;
    p.pitch = p.input.pitch;
    if (p.down) {
        p.bleed -= dt;
        if (p.bleed <= 0) {
            eliminate(p.id, p.attacker);
            return;
        }
    }
    if (p.flight == IN_PLANE) {
        p.body.position = planePosition();
        if ((p.input.buttons & JUMP) || phase_ == ACTIVE) {
            p.flight = FREEFALL;
            p.body.velocity = {0, -6, 0};
            p.input.buttons &= ~JUMP;
        }
        p.input.buttons &= (SPRINT | CROUCH);
        p.input.slot = -1;
        p.input.dropCount = 0;
        return;
    }
    Vec2 move(p.input.moveX, p.input.moveZ);
    const float m = move.length();
    if (m > 1)
        move = move * (1 / m);
    if (p.flight == LANDED) {
        const float impact = mission::stepGroundMovement(
            *this, p.body, p.movement, move.x, move.y, p.yaw,
            (p.input.buttons & SPRINT) && !(p.input.buttons & (FIRE | MELEE | HEAL | REVIVE)),
            p.input.buttons & CROUCH, p.input.buttons & JUMP, p.down, dt);
        if (p.body.height < 1.5f && !p.down)
            p.input.buttons |= CROUCH;
        else
            p.input.buttons &= ~CROUCH;
        if (impact > 14)
            damage(p.id, (impact - 14) * 4, -1);
    } else {
        const float altitude =
            p.body.position.y - map_.heightAt(int(p.body.position.x), int(p.body.position.z));
        mission::stepDescent(*this, p.body, p.flight, move.x, move.y, p.yaw, p.input.buttons & PARACHUTE,
                             altitude, dt);
    }
    if (p.alive && p.flight == LANDED && !p.down)
        actions(p, dt);
    else {
        p.input.buttons &= (SPRINT | CROUCH);
        p.input.slot = -1;
        p.input.dropCount = 0;
    }
}
void BattleRoyale::botInput(PlayerState &p, float) {
    auto &in = p.input;
    p.sinceInput = 0;
    in.buttons = 0;
    in.slot = -1;
    in.target = -1;
    if (p.flight == IN_PLANE) {
        if (phaseTime_ > config_.planeSeconds * (.15f + float(p.id % 11) * .055f))
            in.buttons = JUMP;
        return;
    }
    Vec3 goal{zone_.nextX, 0, zone_.nextZ};
    if (p.flight != LANDED && !map_.buildings().empty()) {
        goal = map_.doorPosition(map_.buildings()[size_t(p.id * 7) % map_.buildings().size()]);
    } else {
        const PlayerState *enemy = nullptr;
        float nearest = 95;
        for (const auto &q : players_)
            if (q.present && q.alive && q.team != p.team && q.flight == LANDED) {
                float d = distanceXZ(p.body.position, q.body.position);
                if (d < nearest) {
                    nearest = d;
                    enemy = &q;
                }
            }
        if (enemy && p.equipped >= 0 &&
            lineOfSight(p.body.position + Vec3(0, 1.4f, 0), enemy->body.position + Vec3(0, 1.1f, 0))) {
            goal = enemy->body.position;
            in.buttons |= FIRE;
            in.pitch = std::atan2(goal.y + 1.1f - (p.body.position.y + 1.4f), std::max(1.0f, nearest));
        } else if (distanceXZ(p.body.position, goal) < zone_.nextRadius * .8f || p.equipped < 0) {
            float best = 1e9f;
            for (const auto &l : loot_)
                if (!l.taken && (weapon(l.item) || p.inventory.count(l.item) < 30)) {
                    float d = distanceXZ(p.body.position, l.pos);
                    if (d < best) {
                        best = d;
                        goal = l.pos;
                    }
                }
            in.buttons |= INTERACT;
        }
        if (p.equipped >= 0 && p.ammo < 1)
            in.buttons |= RELOAD;
        if (p.health < 65)
            in.buttons |= HEAL;
        if (p.body.hitWall)
            in.buttons |= JUMP;
        for (const auto &q : players_)
            if (q.present && q.alive && q.down && q.team == p.team &&
                distanceXZ(q.body.position, p.body.position) < 2.5f) {
                in.buttons |= REVIVE;
                in.target = q.id;
            }
    }
    Vec3 d = goal - p.body.position;
    in.yaw = yawFromDirection(d.x, d.z);
    in.moveX = 0;
    in.moveZ = (in.buttons & FIRE) ? 0 : 1;
    in.buttons |= SPRINT;
}
void BattleRoyale::step(float dt) {
    if (!std::isfinite(dt) || dt <= 0 || dt > .1f || ended())
        return;
    accumulator_ += dt;
    while (accumulator_ + 1e-8 >= .05 && !ended()) {
        accumulator_ -= .05;
        fixedTick(.05f);
    }
}
void BattleRoyale::fixedTick(float dt) {
    time_ += dt;
    phaseTime_ += dt;
    if (phase_ == WAITING) {
        if (phaseTime_ >= config_.waitSeconds) {
            phase_ = PLANE;
            phaseTime_ = 0;
        } else
            return;
    }
    if (phase_ == PLANE && phaseTime_ >= config_.planeSeconds) {
        for (auto &p : players_)
            if (p.present && p.alive && p.flight == IN_PLANE) {
                p.body.position = routeB_;
                p.flight = FREEFALL;
                p.body.velocity = {0, -6, 0};
            }
        phase_ = ACTIVE;
        phaseTime_ = 0;
    }
    for (auto &p : players_) {
        p.reviveThisTick = false;
        p.hurtThisTick = false;
    }
    for (auto &p : players_)
        if (p.present && p.alive)
            playerTick(p, dt);
    for (auto &p : players_)
        if (p.down && !p.reviveThisTick) {
            p.revive = 0;
            p.reviver = -1;
        }
    for (auto &w : walls_)
        w.life -= dt;
    walls_.erase(std::remove_if(walls_.begin(), walls_.end(),
                                [](const IceWall &w) { return w.life <= 0 || w.health <= 0; }),
                 walls_.end());
    for (auto &g : grenades_) {
        g.velocity.y -= 18 * dt;
        Vec3 displacement = g.velocity * dt;
        const int n = std::max(1, int(std::ceil(length(displacement) / .15f)));
        for (int i = 0; i < n; ++i) {
            const Vec3 next = g.pos + displacement / float(n);
            if (boxCollides(next - Vec3(.08f, .08f, .08f), next + Vec3(.08f, .08f, .08f))) {
                g.velocity = g.velocity * -.3f;
                g.velocity.y = std::abs(g.velocity.y);
                break;
            }
            g.pos = next;
        }
        g.fuse -= dt;
        if (g.fuse <= 0) {
            if (g.kind == SMOKE) {
                if (smoke_.size() < 100)
                    smoke_.push_back({g.pos, 14});
            } else {
                for (auto &p : players_)
                    if (p.present && p.alive) {
                        float d = length(p.body.position - g.pos);
                        if (d < 7 && lineOfSight(g.pos, p.body.position + Vec3(0, .6f, 0)))
                            damage(p.id, 100 * (1 - d / 7), g.owner);
                    }
                for (auto &w : walls_)
                    if (length(w.pos - g.pos) < 7)
                        w.health -= 180;
            }
        }
    }
    grenades_.erase(
        std::remove_if(grenades_.begin(), grenades_.end(), [](const auto &g) { return g.fuse <= 0; }),
        grenades_.end());
    for (auto &smoke : smoke_)
        smoke.life -= dt;
    smoke_.erase(std::remove_if(smoke_.begin(), smoke_.end(), [](const auto &s) { return s.life <= 0; }),
                 smoke_.end());
    if (phase_ == ACTIVE) {
        advanceZone(dt);
        dropTimer_ -= dt;
        if (dropTimer_ <= 0 && drops_.size() < 12) {
            const float a = rng_.range(0, kTwoPi), r = rng_.range(0, std::max(0.0f, zone_.radius - 15));
            drops_.push_back(
                {nextEntity_++, {zone_.x + std::sin(a) * r, 170, zone_.z + std::cos(a) * r}, false, 0});
            dropTimer_ = 90;
        }
        for (auto &drop : drops_)
            if (!drop.landed) {
                drop.age += dt;
                drop.pos.y -= 5 * dt;
                const float ground = map_.heightAt(int(drop.pos.x), int(drop.pos.z));
                if (drop.pos.y <= ground && lootRoom(4)) {
                    drop.pos.y = ground;
                    drop.landed = true;
                    createLoot(drop.pos, SNIPER, 1);
                    createLoot(drop.pos + Vec3(1, 0, 0), AMMO_SNIPER, 25);
                    createLoot(drop.pos + Vec3(-1, 0, 0), ICE, 3);
                    createLoot(drop.pos + Vec3(0, 0, 1), MEDKIT, 3);
                } else
                    drop.pos.y = std::max(drop.pos.y, ground);
            }
        if (aliveTeams() <= 1) {
            phase_ = FINISHED;
            winner_ = -1;
            for (auto &p : players_)
                if (p.present && p.alive)
                    winner_ = p.team;
            for (auto &p : players_)
                if (p.present && p.team == winner_)
                    p.placement = 1;
        }
    }
}

// Float snapshot contract is versioned, bounded and identical through JNI and Node-API.
// Header 24; players 32; loot 7; walls 10; drops 7; smoke 5; own inventory 24*2.
std::vector<float> BattleRoyale::snapshot(int viewer) const {
    std::vector<float> out;
    out.reserve(8000);
    const bool hasViewer = viewer >= 0 && viewer < kMaxPlayers && players_[viewer].present;
    const int viewerTeam = hasViewer ? players_[viewer].team : -1;
    Vec3 eye = hasViewer ? players_[viewer].body.position : Vec3(720, 0, 720);
    std::vector<const PlayerState *> visiblePlayers;
    for (const auto &p : players_)
        if (p.present && (!hasViewer || p.team == viewerTeam || phase_ == FINISHED ||
                          distanceXZ(eye, p.body.position) <= 500))
            visiblePlayers.push_back(&p);
    std::vector<const LootEntity *> visible;
    for (const auto &l : loot_)
        if (!l.taken && (viewer < 0 || distanceXZ(eye, l.pos) < 180))
            visible.push_back(&l);
    const Vec3 plane = planePosition();
    out = {1,
           float(phase_),
           time_,
           float(winner_),
           zone_.x,
           zone_.z,
           zone_.radius,
           zone_.nextX,
           zone_.nextZ,
           zone_.nextRadius,
           zone_.timer,
           float(zone_.stage),
           zone_.shrinking ? 1.f : 0.f,
           plane.x,
           plane.y,
           plane.z,
           float(visiblePlayers.size()),
           float(visible.size()),
           float(walls_.size()),
           float(drops_.size()),
           float(smoke_.size()),
           float(aliveTeams()),
           phaseTime_,
           float(config_.teamSize)};
    for (const auto *state : visiblePlayers) {
        const auto &p = *state;
        const bool own = hasViewer && p.id == viewer;
        const bool ally = hasViewer && p.team == viewerTeam;
        int flags = (p.alive ? 1 : 0) | (p.down ? 2 : 0) | (p.connected ? 4 : 0) | (p.bot ? 8 : 0) |
                    ((p.input.buttons & CROUCH) ? 16 : 0) | ((p.input.buttons & SPRINT) ? 32 : 0);
        int gun = p.equipped >= 0 ? p.inventory.slots[p.equipped].id : 0;
        out.insert(out.end(), {float(p.id),
                               float(p.team),
                               float(uint32_t(p.look) & 65535u),
                               float(flags),
                               p.body.position.x,
                               p.body.position.y,
                               p.body.position.z,
                               p.yaw,
                               p.pitch,
                               p.health,
                               float(p.kills),
                               float(p.flight),
                               float(gun),
                               own ? p.ammo : 0.f,
                               own ? p.reload : 0.f,
                               own ? float(p.inventory.vest) : 0.f,
                               own ? float(p.inventory.helmet) : 0.f,
                               float(p.inventory.backpack),
                               own ? float(p.sequence) : 0.f,
                               ally ? p.bleed : 0.f,
                               ally ? p.revive : 0.f,
                               own ? p.heal : 0.f,
                               p.shot,
                               float(p.placement),
                               ally ? p.markX : -1.f,
                               ally ? p.markZ : -1.f,
                               own ? float(p.equipped) : -1.f,
                               p.body.velocity.x,
                               p.body.velocity.y,
                               p.body.velocity.z,
                               float(uint32_t(p.look) >> 16),
                               float(p.shotCount)});
    }
    for (const auto *l : visible)
        out.insert(out.end(),
                   {float(l->id), float(l->item), float(l->count), l->pos.x, l->pos.y, l->pos.z, 0});
    for (const auto &w : walls_)
        out.insert(out.end(),
                   {float(w.id), float(w.owner), w.pos.x, w.pos.y, w.pos.z, w.yaw, w.health, w.life, 0, 0});
    for (const auto &d : drops_)
        out.insert(out.end(), {float(d.id), d.pos.x, d.pos.y, d.pos.z, d.landed ? 1.f : 0.f, d.age, 0});
    for (const auto &s : smoke_)
        out.insert(out.end(), {s.pos.x, s.pos.y, s.pos.z, s.life, 5});
    for (int i = 0; i < 24; ++i) {
        Stack s = hasViewer ? players_[viewer].inventory.slots[i] : Stack{};
        out.push_back(float(s.id));
        out.push_back(float(s.count));
    }
    return out;
}
} // namespace zz::br
