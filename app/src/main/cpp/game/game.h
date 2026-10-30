#pragma once
// Android presentation and prediction. Match authority lives on the VPS.
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include "../core/random.h"
#include "../render/blocky.h"
#include "../render/renderer.h"
#include "../render/voxel_weapons.h"
#include "../world/competitive_source.h"
#include "../world/terrain.h"
#include "competitive_wire.h"
#include "mission_movement.h"
#include "hero_presentation.h"
#include "protocol.h"
namespace zz {
struct InputState {
    float moveX = 0, moveY = 0, lookX = 0, lookY = 0;
    bool fire = false, aim = false, throwAim = false;
    int aimStage = 0;
    uint32_t buttons = 0;
    int slot = -1, target = -1, dropItem = 0, dropCount = 0;
    float markX = 0, markZ = 0;
    int shoulderSwaps = 0;
    bool crouch = false, sprint = false;
};
class Game {
  public:
    Game();
    void onSurfaceCreated();
    void onSurfaceChanged(int, int);
    void frame(double);
    void enterLobby(uint32_t, int);
    void previewWeapon(int id) {
        if (id >= 1 && id <= 6)
            previewWeapon_ = id;
    }
    bool prepareBattle(int, uint32_t, uint32_t, bool);
    void applyBattleSnapshot(const float *, size_t);
    void applyBattleEffects(const float *, size_t);
    int pollBattleInput(float *, int);
    uint32_t battleMapHash() const;
    void setPaused(bool);
    void setSettings(float, bool, bool);
    void setGraphics(int);
    void setMove(float, float);
    void addLook(float, float);
    void setFire(bool);
    void setAim(bool);
    void setAimStage(int);
    void pressJump();
    void pressReload();
    void toggleCrouch();
    void toggleSprint();
    void pressMelee();
    void pressGrenade();
    void swapShoulder();
    void tapSlot(int);
    void useSlot(int);
    void dropFromPlayer(int, int);
    void setThrowAim(bool, bool);
    void action(int, int, float, float);
    void copyHud(float *, int);
    int drainEvents(int *, int);
    int minimapSize() const { return 256; }
    int bigMapSize() const { return 256; }
    void copyMinimap(uint32_t *, int);
    void copyBigMap(uint32_t *d, int n) { copyMinimap(d, n); }

  private:
    struct Actor {
        std::array<float, 32> state{};
        Vec3 from, to, position, muzzle;
        float sample = 0;
        Animator anim;
        HeroPresentation presentation;
        float deathTime = 0, swapTime = 0, meleeTime = -1, throwTime = -1, kick = 0, hitFlash = 0, barTimer = 0;
    };
    struct Pending {
        std::array<float, 12> command;
        mission::GroundMotion motionAfter;
    };
    struct Tracer {
        Vec3 from, to;
        Vec4 color{1.f, .9f, .6f, 1.f};
        float life = .06f;
    };
    struct Bullet {
        Vec3 pos, vel;
        Vec4 color{1.f, .8f, .5f, 1.f};
        float life = 0;
        int weapon = 0;
    };
    struct FxParticle {
        Vec3 position, velocity;
        Vec4 color{1.f, 1.f, 1.f, 1.f};
        float size = .12f, life = .4f, maxLife = .4f, gravity = 0;
        bool additive = false;
    };
    struct FxLabel {
        Vec3 position;
        int value = 0, kind = 0;
        float life = .8f;
    };
    struct ShotTrace {
        enum Kind { None, World, Wall, ActorHit } kind = None;
        float t = 0;
        int actorId = -1;
        bool head = false;
        RayHit world{};
    };
    struct CollisionQuery {
        const Game &game;
        bool boxCollides(const Vec3 &, const Vec3 &) const;
        bool isWaterAt(const Vec3 &p) const { return game.world_.isWaterAt(p); }
        bool solid(int x, int y, int z) const {
            return boxCollides({x + .001f, y + .001f, z + .001f}, {x + .999f, y + .999f, z + .999f});
        }
    };
    void emit(int, int = 0, int = 0, int = 0);
    void emitSoundAt(int sound, const Vec3 &pos, float volume = .8f);
    Vec3 visualPlayerPos() const;
    ShotTrace traceShot(const Vec3 &origin, const Vec3 &dir, float maxT, float minT, int ignoreId) const;
    bool smokeObscures(const Vec3 &from, const Vec3 &to) const;
    void spawnBurst(const Vec3 &at, const Vec4 &color, int count, float speed, float size, bool additive,
                    float gravity);
    void spawnMuzzleFx(const Vec3 &muzzle, const Vec3 &dir, int weapon, bool local);
    void fireVisualShot(const Vec3 &origin, const Vec3 &muzzle, const Vec3 &dir, int weapon, int shooterId,
                        bool local);
    void updateWeapon(float dt, const InputState &in);
    void updateEffects(float dt);
    bool project(const Vec3 &p, float &sx, float &sy) const;
    void updateCamera(float, InputState &);
    void tickInput();
    void predict(const std::array<float, 12> &);
    void buildScene();
    void updateHud();
    void box(Vec3, Vec3, Vec4, float = 0);
    CharacterAsset *character(uint32_t);
    void addCharacter(CharacterAsset *, const Vec3 &, float, float, const Pose &,
                      const Vec3 &tint = Vec3(1, 1, 1), float = 0, const Vec3 &scale = Vec3(1, 1, 1),
                      float = 0);
    void poseCrouch(Pose &, const CharacterModel &, float, float);
    void poseAir(Pose &, const CharacterModel &, float, bool);
    void poseArmedBlend(Pose &, const CharacterModel &, float, bool, float, float, int);
    void holdGun(Pose &, const CharacterModel &, float, bool, float, int);
    void addWeaponOnBack(const CharacterDraw &, int, int);
    void addWeaponModel(const CharacterDraw &, int, int, Vec3 *);
    void addWeaponParts(const Mat4 &, int, Vec3 *);
    void addVoxelItem(VoxelItem, const Mat4 &);
    void updateActor(Actor &, bool, const InputState &, float);
    void poseActor(Actor &, bool, Pose &);
    Renderer renderer_;
    World world_;
    Theme theme_ = lobbyTheme();
    WorldLayout layout_;
    std::unique_ptr<CompetitiveMapSource> source_;
    FrameScene scene_;
    std::map<uint32_t, std::unique_ptr<CharacterAsset>> characters_;
    std::array<CharacterAsset, size_t(VoxelItem::Count)> voxelItems_;
    std::map<int, Actor> actors_;
    std::array<float, 32> self_{};
    std::vector<float> snapshot_;
    std::vector<float> effects_;
    std::vector<std::array<float, 10>> walls_;
    std::deque<Pending> pending_;
    std::deque<std::array<float, 12>> outgoing_;
    std::vector<uint32_t> mapImage_;
    BodyState player_, visualPlayer_;
    mission::GroundMotion motion_, ackMotion_;
    Vec3 correction_, muzzlePos_;
    float visualStep_ = 0;
    int lobbyScreen_ = 0, lobbyEmoteCount_ = 0;
    Animator lobbyAnim_;
    float lobbyEmoteTimer_ = 3, bobPhase_ = 0, sprintFov_ = 0;
    bool throwAiming_ = false;
    int localAimStage_ = 0;
    int localId_ = -1, quality_ = 1, predictedFlight_ = 0, previewWeapon_ = 3;
    uint32_t sequence_ = 0, look_ = 0;
    float lastSnapshot_ = -1;
    bool battle_ = false, loading_ = false, ready_ = false, paused_ = false, expandedReleased_ = false,
         crouching_ = false, sprinting_ = false, dead_ = false;
    double lastFrame_ = 0;
    float time_ = 0, inputTime_ = 0, sensitivity_ = 1, fps_ = 60, lastTelemetry_ = 0;
    bool invertY_ = false, aimAssist_ = true;
    int assistTarget_ = -1;
    float assistAngle_ = 10, targetHp_ = 0;
    bool targetLocked_ = false, firingNow_ = false, headshotMark_ = false, rackPlayed_ = true;
    float fireCooldown_ = 0, shotFeedback_ = 0, muzzleTimer_ = 0, recoil_ = 0, shake_ = 0, hitMark_ = 0,
          emptyClick_ = 0, damageDir_ = 0, damageDirTimer_ = 0, localAmmo_ = 0, lastReloadState_ = 0;
    int lastWeaponId_ = 0;
    Rng rng_{0xC0FFEEu};
    std::vector<Tracer> tracers_;
    std::vector<Bullet> bullets_;
    std::vector<FxParticle> particles_;
    std::vector<FxLabel> labels_;
    float camYaw_ = 0, camPitch_ = -.12f, camDistance_ = 4.2f, fov_ = 1, planeYaw_ = 0, shoulder_ = 1,
          shoulderBlend_ = 1, aimBlend_ = 0, crouchBlend_ = 0, weaponKick_ = 0;
    Vec3 camPivot_, camPos_, camForward_;
    bool camPivotInit_ = false, planePosInit_ = false;
    Vec3 visualPlanePos_{720, 190, 720}, serverPlanePos_{720, 190, 720}, planeVel_{};
    float planeSnapAge_ = 0, visualZoneRadius_ = 1050.f;
    Vec3 visualZoneCenter_{720, 0, 720};
    bool zoneInit_ = false;
    std::array<bool, 7> weaponInitialLoaded_{};
    Mat4 view_, proj_;
    float ikAim_ = 0, ikTwist_ = 1, ikKick_ = 0, ikDip_ = 0, ikReload_ = -1, ikMove_ = 0;
    bool ikFalling_ = false;
    std::vector<Mat4> ikGlobals_;
    struct {
        bool valid = false;
        Mat4 frame;
    } gunHold_;
    std::mutex inputMutex_, outMutex_;
    InputState input_;
    std::array<float, proto::H_SIZE> hud_{};
    std::vector<int> events_;
};
} // namespace zz
