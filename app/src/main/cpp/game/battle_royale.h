#pragma once
#include <array>
#include <vector>
#include <deque>
#include "../core/random.h"
#include "../world/competitive_map.h"
#include "../world/body_motion.h"
#include "br_inventory.h"
#include "mission_movement.h"

namespace zz::br {
constexpr int kMaxPlayers = 50, kProtocolVersion = 1;
enum Button : uint32_t {
    FIRE = 1,
    JUMP = 2,
    RELOAD = 4,
    SPRINT = 8,
    CROUCH = 16,
    INTERACT = 32,
    HEAL = 64,
    PLACE_ICE = 128,
    THROW_GRENADE = 256,
    THROW_SMOKE = 512,
    REVIVE = 1024,
    PARACHUTE = 2048,
    MELEE = 4096,
    DROP = 8192,
    PING = 16384
};
enum Phase : int { WAITING = 0, PLANE = 1, ACTIVE = 2, FINISHED = 3 };
enum Flight : int { IN_PLANE = 0, FREEFALL = 1, CHUTE = 2, LANDED = 3 };
struct Config {
    int teamSize = 1;
    uint32_t seed = 1;
    float waitSeconds = 15, planeSeconds = 50, zoneWait = 55, zoneShrink = 35;
    float bleedSeconds = 30, reviveSeconds = 5, reconnectSeconds = 30;
    bool friendlyFire = false;
};
struct InputCommand {
    uint32_t sequence = 0;
    float moveX = 0, moveZ = 0, yaw = 0, pitch = 0;
    uint32_t buttons = 0;
    int slot = -1, target = -1, dropItem = 0, dropCount = 0;
    float markX = -1, markZ = -1;
};
struct PlayerState {
    bool present = false, connected = true, alive = true, down = false, bot = false, reviveThisTick = false;
    int id = 0, team = 0, look = 0, kills = 0, placement = 0, flight = IN_PLANE, equipped = -1, attacker = -1,
        reviver = -1, shotCount = 0;
    uint32_t sequence = 0, receivedSequence = 0;
    BodyState body;
    mission::GroundMotion movement;
    std::deque<InputCommand> pendingInputs;
    bool hurtThisTick = false;
    float health = 100, yaw = 0, pitch = 0, ammo = 0, reload = 0, cooldown = 0, sinceInput = 0,
          disconnected = 0, bleed = 0, revive = 0, heal = 0, shot = 0;
    float markX = -1, markZ = -1;
    Inventory inventory;
    InputCommand input;
    std::array<int, 7> clips{};
    float iceCooldown = 0;
};
struct ZoneState {
    float x = 720, z = 720, radius = 710, nextX = 720, nextZ = 720, nextRadius = 710, timer = 0;
    int stage = 0;
    bool shrinking = false;
};
struct LootEntity {
    int id = 0, item = 0, count = 0;
    Vec3 pos;
    bool taken = false;
    int droppedBy = -1;
    float pickupAfter = 0;
};
struct IceWall {
    int id = 0, owner = 0;
    Vec3 pos;
    float yaw = 0, health = 250, life = 45;
};
struct DropEntity {
    int id = 0;
    Vec3 pos;
    bool landed = false;
    float age = 0;
};
struct GrenadeEntity {
    int owner = 0, kind = 0;
    Vec3 pos, velocity;
    float fuse = 3;
};
struct SmokeEntity {
    Vec3 pos;
    float life = 14;
};

class BattleRoyale {
  public:
    explicit BattleRoyale(const Config &config = Config());
    bool join(int id, int team, int look, bool bot);
    bool submitInput(int id, const InputCommand &command);
    void setConnected(int id, bool connected);
    void forfeit(int id);
    void step(float dt);
    std::vector<float> snapshot(int viewer = -1) const;
    const auto &players() const { return players_; }
    const auto &loot() const { return loot_; }
    const auto &walls() const { return walls_; }
    const auto &drops() const { return drops_; }
    const auto &grenades() const { return grenades_; }
    const auto &zone() const { return zone_; }
    const CompetitiveMap &map() const { return map_; }
    Phase phase() const { return phase_; }
    bool ended() const { return phase_ == FINISHED; }
    int winner() const { return winner_; }
    float time() const { return time_; }
    bool boxCollides(const Vec3 &mn, const Vec3 &mx) const;
    bool isWaterAt(const Vec3 &p) const;
    bool solid(int x, int y, int z) const { return isSolid(map_.blockAt(x, y, z)); }
    Vec3 planePosition() const;

  private:
    void playerTick(PlayerState &p, float dt);
    void actions(PlayerState &p, float dt);
    void botInput(PlayerState &p, float dt);
    void advanceZone(float dt);
    void chooseNextZone();
    void fire(PlayerState &p, int weaponId);
    void damage(int target, float amount, int attacker, bool head = false);
    void eliminate(int id, int attacker);
    bool lineOfSight(const Vec3 &a, const Vec3 &b) const;
    float obstruction(const Vec3 &origin, const Vec3 &dir, float range, int *ice = nullptr) const;
    void createLoot(const Vec3 &pos, int item, int count);
    int aliveTeams() const;
    void fixedTick(float dt);
    bool lootRoom(int count = 1) const;
    Config config_;
    CompetitiveMap map_;
    Rng rng_;
    std::array<PlayerState, kMaxPlayers> players_{};
    std::vector<LootEntity> loot_;
    std::vector<IceWall> walls_;
    std::vector<DropEntity> drops_;
    std::vector<GrenadeEntity> grenades_;
    std::vector<SmokeEntity> smoke_;
    ZoneState zone_;
    Phase phase_ = WAITING;
    float time_ = 0, phaseTime_ = 0, dropTimer_ = 70;
    double accumulator_ = 0;
    int winner_ = -1, nextEntity_ = 1;
    Vec3 routeA_, routeB_;
};
} // namespace zz::br
