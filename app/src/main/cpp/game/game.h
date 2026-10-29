// Lógica principal: lobby 3D, missão na ilha em streaming (jogador, zumbis procedurais com habilidades,
// projéteis, ninhos, baús, suprimentos, portal), câmera e HUD.
#pragma once

#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "../core/math.h"
#include "../core/perf.h"
#include "../core/random.h"
#include "../render/blocky.h"
#include "../render/character.h"
#include "../render/renderer.h"
#include "../render/voxel_weapons.h"
#include "../world/island.h"
#include "../world/terrain.h"
#include "../world/fluids.h"
#include "../world/world_clock.h"
#include "../world/destruction.h"
#include "../world/theme.h"
#include "../world/world.h"
#include "contract_rules.h"
#include "items.h"
#include "nav.h"
#include "perf_scenario.h"
#include "protocol.h"

namespace zz {

struct MissionConfig {
    int stage = 1;
    uint32_t skin = 0;  // aparência do personagem (Look codificado)
    int zombieCount = 10;     // população de zumbis mantida ao redor do jogador
    float zombieHealth = 70;
    float zombieDamage = 8;
    float zombieSpeed = 1.0f;
    float playerHealth = 100;
    int magazine = 30;
    float power = 1.0f;
    int crystalMask = 0;
    int crystalTotal = 3;
    float sensitivity = 1.0f;
    bool aimAssist = true;
    bool invertY = false;
    uint32_t seed = 1;
    float reloadSpeed = 1.0f;
    float boostHealth = 1.0f;  // bônus de anúncio antes da fase
    float boostDamage = 1.0f;
    int mode = 0;              // 0 campanha, 1 sobrevivência por ondas
    // Melhorias compradas com diamantes
    float armor = 0;           // redução de dano (0..0.6)
    float regen = 0;           // fração da vida máxima por segundo
    float speedMul = 1.0f;
    float headshotMul = 1.0f;
    float luck = 1.0f;         // chance de itens dos zumbis
    int explosives = 0;        // nível de explosivos
    // Diagnóstico de desempenho (só builds de depuração enviam isto): 0 desligado, 1 autopiloto sem inimigos, 2 com inimigos
    int perfMode = 0;
    int perfFrames = 0;        // 0 = 7200
    // Contrato de expedição (etapa 4). Valores padrão = expedição padrão.
    int contractId = 0;         // ContractId do Kotlin
    float ammoClips = -1.0f;    // reserva em pentes; < 0 = ilimitada
    int chestAmmoPct = 0;       // reserva ganha por baú (centésimos de pente)
    float visionMul = 1.0f;     // x alcance da névoa
    float aggression = 1.0f;    // x percepção/persistência dos zumbis
    bool redSky = false;
    float guardMul = 1.0f;      // x guardas dos pontos de interesse
    float nestBomb = 0.0f;      // 4C: chance de o ninho gerar Inchado (> 0 = ninho instável)
    bool cargo = false;         // 4D: existe carga opcional (caixa perdida)
};

// Entrada de toque escrita pela thread de UI e consumida no quadro.
struct InputState {
    float moveX = 0, moveY = 0;
    float lookX = 0, lookY = 0;  // acumulado em dp
    bool fire = false;
    bool aim = false;
    int aimStage = 0;  // 0 = fora da mira, 1 = ADS, 2 = segundo nível da luneta
    int jumps = 0;
    int reloads = 0;
    int supplyRequests = 0;
    int crouchToggles = 0;
    int sprintToggles = 0;
    int melees = 0;
    int grenades = 0;
    int shoulderSwaps = 0;
    // Inventário: toques na barra rápida e "usar" vindos da mochila.
    int taps[8] = {};
    int tapCount = 0;
    int uses[8] = {};
    int useCount = 0;
    // Mira do arremesso: segurar mostra a trajetória, soltar arremessa.
    bool throwAim = false;
    int throwReleases = 0;
    int throwCancels = 0;
};

// Tipos de zumbi (cada um com habilidade própria). A ordem espelha ZombieKinds no Kotlin.
enum class ZombieKind {
    Walker = 0,      // Errante: corpo a corpo comum
    Runner,          // Corredor: rápido, dá botes
    Brute,           // Brutamontes: pancada no chão que arremessa
    Thrower,         // Arremessador: atira pedras
    Molotov,         // Incendiário: coquetéis molotov que deixam fogo no chão
    Knife,           // Retalhador: faca, investida e sangramento
    Exploder,        // Inchado: corre e explode
    Screamer,        // Gritador: grito que chama e enfurece a horda
    Spitter,         // Cuspidor: ácido que deixa poça corrosiva
    KnifeThrower,    // Atirador de facas: facas rápidas em linha reta
    Count
};

enum ZombieMutation : uint8_t {
    MUT_ARMORED = 1,  // capacete e colete: tiros no corpo valem menos
    MUT_GIANT = 2,
    MUT_SWIFT = 4,
    MUT_REGEN = 8,
    MUT_TOXIC = 16,   // golpes envenenam
    MUT_RAGE = 32,    // enfurece com pouca vida
};

enum class ZState { Idle, Wander, Chase, Attack, Dead };
enum class ZAction { None, Throw, Scream, Slam, Dash, Fuse };

struct Zombie {
    ZombieKind kind = ZombieKind::Walker;
    uint8_t mutations = 0;
    CharacterAsset* asset = nullptr;
    BodyState body;
    Animator anim;
    float yaw = 0;
    float health = 0, maxHealth = 0;
    float speed = 2.3f;
    float damage = 8;
    float scale = 1.0f;
    float bloat = 1.0f;
    Vec3 tint{1, 1, 1};
    ZState state = ZState::Idle;
    ZAction action = ZAction::None;
    float actionTime = 0;
    bool actionDone = false;
    bool alerted = false;
    float lostSight = 0;
    float windup = 0;
    float cooldown = 0;
    float abilityTimer = 0;
    bool attackHit = false;
    float hitFlash = 0;
    float deathTime = 0;
    float groanTimer = 3;
    float perceptionTimer = 0;
    bool canSee = false;
    Vec3 home;
    Vec3 wanderTarget;
    float wanderTimer = 0;
    float stuckTimer = 0;
    float sidestepTimer = 0;
    Vec3 sidestepDir;
    Vec3 lastPos;
    float stun = 0;
    float barTimer = 0;
    float frenzy = 0;
    float regenDelay = 0;
    float itemHidden = 0;   // item da mão acabou de ser arremessado
    float strafe = 1;
    float strafeTimer = 0;
    float burn = 0;
    int nest = -1;          // ninho que o gerou
    float burnDps = 14.0f;  // dano do fogo por segundo
    bool exploded = false;
    bool announced = false; // jogador já foi avisado deste tipo/elite
    bool counted = false;   // já entrou na contagem de alertas do contrato
};

struct Pickup {
    int type = 0;  // 0 cristal, 1 vida, 2 caixa de suprimentos, 3 item do inventário
    int crystalIndex = -1;
    int item = 0;
    int count = 0;
    float blocked = 0;  // mochila cheia: espera antes de tentar de novo
    bool discarded = false;  // jogado fora pela mochila: só volta a ser recolhido depois que o jogador se afastar
    Vec3 position;
    Vec3 velocity;
    float age = 0;
    bool taken = false;
};

// Bullet: projétil das armas do tipo Projectile (escopeta do template), reto e sem gravidade.
enum class ProjectileKind { Rock, Molotov, Acid, Knife, Grenade, Smoke, Bullet };

struct Projectile {
    ProjectileKind kind = ProjectileKind::Rock;
    Vec3 pos, vel;
    Vec3 target;
    float damage = 10;
    float gravity = 22;
    float life = 4;
    float spin = 0;
    bool friendly = false;  // lançado pelo jogador (não o atinge)
    float radius = 5.0f;    // raio da explosão (granada) ou do dano em área da bala (AreaOfEffect)
    int weapon = 0;         // arma que disparou a bala (Bullet)
};

// Área perigosa no chão: fogo do molotov ou poça de ácido.
struct Hazard {
    int kind = 0;  // 0 fogo, 1 ácido
    Vec3 pos;
    float radius = 2.5f;
    float life = 5, maxLife = 5;
    float damage = 8;
    float fxTimer = 0;
    bool friendly = false;   // fogo do molotov do jogador: queima só os zumbis
    float zombieDps = 14.0f;
};

struct Explosion {
    Vec3 pos;
    float radius;
    float playerDamage;
    float zombieDamage;
    bool fire;
    bool destroyTerrain = false;
};

struct FlashLight {
    Vec3 pos;
    float radius;
    Vec3 color;
    float life, maxLife;
};

struct SmokeCloud {
    Vec3 pos;
    float radius = 5.0f;
    float life = 11.0f, maxLife = 11.0f;
    float fxTimer = 0;
};

struct Nest {
    int poi = -1;
    Vec3 core;
    float health = 300, maxHealth = 300;
    bool destroyed = false;
    float spawnTimer = 3;
    float hitFlash = 0;
    bool awake = false;
    int spawnCount = 0;     // zumbis gerados por este ninho (diagnóstico do ninho instável)
};

// Carga opcional do contrato Caixa perdida: posição fixa por semente, sem colisão.
struct Cargo {
    Vec3 pos;
    bool placed = false;
    bool revealed = false;   // revelada ao destruir o primeiro ninho
    bool collected = false;
};

struct Chest {
    Vec3 pos;
    int diamonds = 8;
    bool opened = false;
    float openAnim = 0;
};

struct SupplyDrop {
    bool active = false;
    bool landed = false;
    Vec3 pos;
    float age = 0;
};

struct FxParticle {
    Vec3 position, velocity;
    Vec4 color;
    float size = 0.1f;
    float life = 0, maxLife = 1;
    float gravity = 0;
    bool additive = false;
};

struct FxLabel {
    Vec3 position;
    int value = 0;
    int kind = 0;  // 1 dano, 2 tiro na cabeça, 3 cura
    float life = 0;
};

struct Tracer {
    Vec3 from, to;
    float life = 0;
    Vec4 color{1.0f, 0.9f, 0.6f, 1.0f};
};

// Céu e névoa do contrato (game_contract.cpp). O terreno da ilha não lê theme.sky: a semente gera a mesma ilha em qualquer contrato.
Theme applyContractTheme(Theme t, const MissionConfig& c);

class Game {
public:
    Game();
    void onSurfaceCreated();
    void onSurfaceChanged(int width, int height);
    void frame(double nowSeconds);

    // Comandos (thread GL)
    void enterLobby(uint32_t look, int screen);
    void startMission(const MissionConfig& config);
    void setPaused(bool paused);
    void revive();
    void setSettings(float sensitivity, bool aimAssist, bool invertY);
    void setGraphics(int quality);

    // Entrada (qualquer thread)
    void setMove(float x, float y);
    void addLook(float dx, float dy);
    void setFire(bool held);
    void setAim(bool on);
    void setAimStage(int stage);
    void pressJump();
    void pressReload();
    void requestSupply();
    void toggleCrouch();
    void toggleSprint();
    void pressMelee();
    void pressGrenade();
    void swapShoulder();
    void tapSlot(int slot);
    void useSlot(int slot);
    void setThrowAim(bool held, bool cancel);
    // Inventário vindo do Kotlin (thread GL): (id, qtd) x 24 + espaço da arma + do arremessável.
    void setInventory(const int* data, int count);
    // Itens descartados na mochila (thread GL): caem à frente do jogador.
    void dropFromPlayer(int id, int count);

    // Saídas (qualquer thread)
    void copyHud(float* out, int count);
    int drainEvents(int* out, int maxInts);
    void copyResult(float* out, int count);
    // Minimapa RGBA (thread GL).
    int minimapSize() const;
    void copyMinimap(uint32_t* argb, int count);
    int bigMapSize() const { return island_ ? kBigMapSize : 0; }
    void copyBigMap(uint32_t* argb, int count);

private:
    enum class Mode { None, Lobby, Mission };
    struct ItemStack {
        int id = 0;
        int count = 0;
    };
    static constexpr int kMinimapSize = 160;
    static constexpr int kBigMapSize = 256;

    CharacterAsset* character(uint32_t look);
    void addVoxelItem(VoxelItem item, const Mat4& world);
    void emit(int type, int a = 0, int b = 0, int c = 0);
    void emitSound(int sound, const Vec3& at, float volume = 1.0f);

    void updateLobby(float dt);
    void updateMission(float dt);
    void resetPlayerState(const Vec3& position, float yaw, float health, int magazine);
    void addWeaponModel(const CharacterDraw& draw, int armRight, int weapon, Vec3* muzzle);
    void addWeaponParts(const Mat4& frame, int weapon, Vec3* muzzle);
    void addWeaponOnBack(const CharacterDraw& draw, int torso, int weapon);
    void addThrowableModel(const CharacterDraw& draw, int armRight, int item);
    void holdGun(Pose& pose, const CharacterModel& model, float pitch, bool moveHead, float weight, int weapon);
    void poseArmedBlend(Pose& pose, const CharacterModel& model, float pitch, bool moveHead, float lean, float weight, int weapon = IT_RIFLE);
    void poseAir(Pose& pose, const CharacterModel& model, float blend, bool armsFree);
    void updatePlayer(float dt, const InputState& in);
    void updateCamera(float dt, const InputState& in);
    void updateWeapon(float dt, const InputState& in);
    void fireShot();
    void fireProjectile();
    // Primeiro alvo no raio (zumbi, ninho ou bloco), como o raycast do template.
    struct ShotTrace {
        enum Kind { None, World, Zombie, Nest } kind = None;
        float t = 0;
        int index = -1;
        bool head = false;
        RayHit world;
    };
    ShotTrace traceShot(const Vec3& origin, const Vec3& dir, float maxT, float minT);
    bool applyShot(const ShotTrace& hit, const Vec3& origin, const Vec3& dir, int weapon, float damage);
    void meleeHit();

    // Inventário e itens (game_items.cpp)
    bool inventoryEnabled() const { return mode_ == Mode::Mission; }
    int weaponId() const;
    int magazineFor(int weapon) const;
    void resetInventory();
    void ensureWeapon();
    void loadWeaponAmmo();
    void equipSlot(int slot, bool silent);
    void handleSlotTap(int slot);
    void useItemSlot(int slot);
    int addItem(int id, int count);        // retorna o que não coube
    int countItem(int id) const;
    bool takeItem(int id, int count);
    int throwableId() const;
    void dropItem(const Vec3& at, int id, int count);
    void rollZombieDrop(const Zombie& z);
    void giveSupplyBundle();
    void updateThrowAim(float dt, const InputState& in);
    bool throwVelocity(Vec3& hand, Vec3& vel, float& gravity) const;
    void simulateThrow();
    void throwSelected();
    void updateItemStatus(float dt);
    bool canStand() const;
    // Escalada: agarra bordas de até 3 blocos; resgate se ficar preso num buraco sem saída.
    bool tryMantle(const Vec3& dir);
    bool playerEnclosed() const;
    void unstickPlayer();
    void consumeInput(InputState& in);
    void damagePlayer(float amount, const Vec3& from);
    void damageOverTime(float amount);
    void updateStatus(float dt);
    void updatePickups(float dt);
    bool collectPickup(Pickup& p);
    void updateEffects(float dt);
    void spawnBurst(const Vec3& at, const Vec4& color, int count, float speed, float size, bool additive, float gravity);
    void addFlash(const Vec3& at, float radius, const Vec3& color, float life);
    bool smokeObscures(const Vec3& from, const Vec3& to) const;
    bool clearLineOfSight(const Vec3& from, const Vec3& to) const;
    void finishResult();

    // Ilha (game_mission.cpp)
    void streamWorld();                       // orçamento de tempo adaptativo (world.h: StreamBudget)
    int streamRadius() const;                 // raio de mundo efetivo em chunks (derivado da névoa)
    int perfFlags() const;
    void updateSpawner(float dt);
    void updatePois(float dt);
    void updateNests(float dt);
    void updateChests(float dt);
    void scatterMapChests();
    void updateSupply(float dt, const InputState& in);
    void rebuildMinimap();
    void buildBigMap();
    bool findSpawnSpot(const Vec3& around, float minDist, float maxDist, bool behindCamera, Vec3& out);
    bool damageNestRay(const Vec3& origin, const Vec3& dir, float maxT, float& tHit, int& nestIndex);
    void damageNest(int index, float amount, const Vec3& hitPoint);

    // Sobrevivência por ondas (game_survival.cpp)
    void startSurvivalState();
    void updateWaves(float dt);
    void startWave();
    void spawnWaveZombie();
    bool survival() const { return config_.mode == 1; }

    // Contrato de expedição (game_contract.cpp)
    void startContractState();
    void updateContract(float dt);
    void alertZombie(Zombie& z);              // único lugar que vira alerted=true por percepção/ruído
    bool ammoLimited() const { return mode_ == Mode::Mission && reservePct_ >= 0; }
    bool canReloadNow(int magazine) const;
    void completeReload(int magazine);
    void notifyOutOfAmmo();
    void grantReserve(int pct);
    void placeCargo();                        // 4D: escolhe o ponto da carga (determinístico por semente)
    void revealCargo();                       // 4D: chamado quando um ninho é destruído
    void publishContractHud(std::array<float, proto::H_SIZE>& h) const;

    // Zumbis (game_zombies.cpp)
    void loadZombieAssets();
    Zombie makeZombie(ZombieKind kind, uint8_t mutations, const Vec3& pos);
    ZombieKind rollKind();
    uint8_t rollMutations();
    void spawnZombie(const Vec3& pos, bool alerted, int nest = -1, bool forceElite = false, int forceKind = -1);
    void updateZombies(float dt);
    void separateBodies();
    void updateZombie(Zombie& z, size_t index, float dt);
    void zombieAbility(Zombie& z, float dist, float dt);
    void damageZombie(Zombie& z, float amount, bool headshot, const Vec3& dir, const Vec3& hitPoint);
    void killZombie(Zombie& z, bool headshot);
    void alertNear(const Vec3& at, float radius);
    float zombieHeight(const Zombie& z) const { return 1.8f * z.scale; }
    void throwProjectile(Zombie& z, ProjectileKind kind);
    void updateProjectiles(float dt);
    void impactProjectile(const Projectile& p, const Vec3& at, bool hitPlayer);
    void updateHazards(float dt);
    void explode(const Vec3& at, float radius, float playerDamage, float zombieDamage, bool fire, bool destroyTerrain = false);
    bool protectedTerrain(int x,int y,int z) const;
    bool breakTerrain(int x,int y,int z);
    void terrainEdited(int x,int y,int z);
    void writeTerrain(int x,int y,int z,uint8_t block);
    void updateWater(float dt);
    void processExplosions();
    void addZombieVisuals(const Zombie& z, const CharacterDraw& draw);

    void buildScene();
    void addCharacter(CharacterAsset* asset, const Vec3& pos, float yaw, float scale, const Pose& pose, const Vec3& tint, float flash, const Vec3& scaleXYZ = Vec3(1, 1, 1), float roll = 0);
    void poseCrouch(Pose& pose, const CharacterModel& model, float blend, float moving);
    void posePlayer(Pose& pose, bool armed);
    void poseZombie(Pose& pose, const Zombie& z, const CharacterModel& model, float t);
    void publishHud();
    bool project(const Vec3& world, float& sx, float& sy) const;

    Renderer renderer_;
    World world_;
    fluids::FluidSimulation fluids_;
    uint32_t terrainRevision_ = 0;
    NavField nav_;
    WorldLayout layout_;
    std::unique_ptr<IslandGenerator> island_;
    Theme theme_ = lobbyTheme();
    Mode mode_ = Mode::None;
    bool paused_ = false;
    MissionConfig config_;
    // Contrato de expedição
    int reservePct_ = -1;          // reserva de munição em centésimos de pente; < 0 = ilimitada
    int alerts_ = 0;               // zumbis alertados pelo jogador (sem ninhos nem ondas)
    float noAmmoTimer_ = 0;
    float contractLogTimer_ = 0;
    int bombs_ = 0;                // 4C: Inchados gerados por ninhos
    Cargo cargo_;                  // 4D
    std::map<uint32_t, std::unique_ptr<CharacterAsset>> characters_;
    // Armas, granadas e faca em blocos (render/voxel_weapons.cpp), um modelo por VoxelItem.
    std::array<CharacterAsset, static_cast<size_t>(VoxelItem::Count)> voxelItems_;
    std::vector<Vec3> layerColors_;
    int viewRadius_ = 8;
    int quality_ = 1;
    // Desempenho (core/perf.h)
    perf::FrameProfiler prof_;
    StreamStats streamStats_;            // último resultado de World::stream (antes: 'static int pending')
    int uploadUnlimitedFrames_ = 3;      // quadros pós-carga sem orçamento de upload de malhas
    // Autopiloto de depuração (game_perf.cpp): rota fixa e dt fixo para medir desempenho de forma reproduzível
    perf::AutopilotState autopilot_;
    bool noEnemies_ = false;
    void perfBegin();
    void perfDrive(InputState& in);
    void perfEnd();

    // Entrada
    std::mutex inputMutex_;
    InputState input_;
    bool aimToggle_ = false;
    int aimStage_ = 0;

    // Saídas
    std::mutex outMutex_;
    std::vector<int> events_;
    std::array<float, proto::H_SIZE> hud_{};
    std::array<float, proto::R_SIZE> result_{};

    // Tempo
    double lastTime_ = -1;
    float time_ = 0;
    double worldTime_ = 0;
    float missionTime_ = 0;
    float fps_ = 60;

    // Jogador
    BodyState player_;
    CharacterAsset* playerAsset_ = nullptr;
    Animator playerAnim_;
    float playerYaw_ = 0;
    float playerModelYaw_ = 0;
    float health_ = 100, maxHealth_ = 100;
    float visualStep_ = 0;
    float invulnerable_ = 0;
    float damageFlash_ = 0;
    float damageDir_ = 0, damageDirTimer_ = 0;
    bool dead_ = false;
    bool victory_ = false;
    bool sprinting_ = false;
    float stepTimer_ = 0;
    bool wasInWater_ = false;
    float lobbyEmoteTimer_ = 5;
    int lobbyEmoteCount_ = 0;
    int lobbyScreen_ = 0;
    float burning_ = 0, bleeding_ = 0, poisoned_ = 0, slowed_ = 0;
    float damageBoost_ = 0;
    float shake_ = 0;
    float distance_ = 0;
    Vec3 lastPlayerPos_;
    // Movimento e controles completos
    bool crouching_ = false;
    float crouchBlend_ = 0;
    bool sprintLock_ = false;
    float shoulder_ = 1.0f, shoulderBlend_ = 1.0f;
    float coyote_ = 0, jumpBuffer_ = 0;
    float leanRoll_ = 0;
    float meleeCooldown_ = 0, meleeTime_ = -1;
    bool meleeDone_ = false;
    float grenadeCooldown_ = 0;
    float emptyClick_ = 0;
    bool rackPlayed_ = false;
    Vec3 camPivot_;
    bool camPivotInit_ = false;
    float sprintFov_ = 0;
    float bobPhase_ = 0;
    float mantleTime_ = -1, mantleDuration_ = 0.4f;
    float armedBlend_ = 1.0f;  // 1 = arma nas mãos, 0 = nas costas (correndo)
    // Herói voxel com as duas mãos na arma (holdGun): a pose prepara a moldura da arma para o próximo desenho.
    struct GunHold {
        bool valid = false;
        Mat4 frame;  // espaço do modelo do personagem
    } gunHold_;
    float ikAim_ = 0;      // 0 = pronta (atravessada no corpo), 1 = coronha no ombro
    float ikTwist_ = 1;    // gira o tronco de lado ao mirar (0 quando a animação já faz isso)
    float ikKick_ = 0;     // coice do disparo
    float ikDip_ = 0;      // troca de arma (a arma desce e volta)
    float ikReload_ = -1;  // progresso da recarga (0..1) ou -1
    float gunAim_ = 0;     // mistura suave entre pronta e mirando
    float aimHold_ = 0;    // segundos de mira depois do último disparo
    std::vector<Mat4> ikGlobals_;
    float airBlend_ = 0;       // pose de pulo/queda
    Vec3 mantleFrom_, mantleTo_;
    float wallPush_ = 0;
    float stuckTimer_ = 0, stuckCheck_ = 1.0f;
    Vec3 stuckAnchor_;

    // Inventário
    std::array<ItemStack, kInvSlots> inv_{};
    int weaponSlot_ = -1;     // espaço da arma em mãos (-1: fuzil padrão)
    int throwSlot_ = -1;      // espaço do arremessável escolhido
    int clips_[16] = {};      // munição no pente de cada arma
    bool clipsInit_[16] = {};
    float swapTimer_ = 0;
    float armor_ = 0, armorMax_ = 0;
    float adrenaline_ = 0;
    float healCooldown_ = 0;  // intervalo entre kits médicos e bandagens
    float sinceHurt_ = 0;
    float invFullTimer_ = 0;
    // Mira do arremesso
    bool throwAiming_ = false;
    std::vector<Vec3> throwArc_;
    Vec3 throwLanding_;
    bool throwLandingValid_ = false;
    float throwAimTime_ = 0;
    float throwAnimTime_ = -1;
    int throwAnimItem_ = 0;

    // Arma
    int ammo_ = 30;
    float reloadTimer_ = 0;
    float reloadDuration_ = 1.6f;
    float fireCooldown_ = 0;
    float shotFeedback_ = 0;  // mira branca por 100 ms depois do disparo (InGameHUD do template)
    float recoil_ = 0;
    float aimBlend_ = 0;
    float hitMark_ = 0;
    bool headshotMark_ = false;
    float muzzleTimer_ = 0;
    float weaponKick_ = 0;
    Vec3 muzzlePos_;
    bool firingNow_ = false;
    bool targetLocked_ = false;
    float targetHp_ = 0;
    int assistTarget_ = -1;
    float assistAngle_ = 10;
    int shots_ = 0, hits_ = 0;
    int combo_ = 0;
    float comboTimer_ = 0;

    // Câmera
    float camYaw_ = 0, camPitch_ = -0.12f;
    float camDistance_ = 4.2f;
    Vec3 camPos_, camForward_;
    Mat4 view_, proj_;
    float fov_ = 1.0f;

    // Missão
    std::vector<Zombie> zombies_;
    std::vector<Pickup> pickups_;
    std::vector<FxParticle> particles_;
    std::vector<FxLabel> labels_;
    std::vector<Tracer> tracers_;
    std::vector<Projectile> projectiles_;
    std::vector<Hazard> hazards_;
    std::vector<SmokeCloud> smokeClouds_;
    std::vector<Explosion> explosions_;
    std::vector<FlashLight> flashes_;
    std::vector<Nest> nests_;
    std::vector<Chest> chests_;
    std::unordered_set<int> visitedPois_;
    std::unordered_set<int> announcedPois_;
    std::vector<const IslandPoi*> poiScratch_;
    std::vector<const IslandPoi*> prewarmScratch_;  // moldes de POI construídos antes de o jogador chegar
    float prewarmTimer_ = 0;
    std::vector<uint32_t> zombieLooks_;
    std::vector<Vec3> pendingSpawns_;
    SupplyDrop supply_;
    int suppliesLeft_ = 0;
    float supplyCooldown_ = 0;
    float supplyPeriod_ = 150;
    int crystalsSpawnedMask_ = 0;
    int crystalsCollectedMask_ = 0;
    int kills_ = 0, headshots_ = 0;
    int chestsOpened_ = 0, chestDiamonds_ = 0;
    int seenKinds_ = 0;
    float damageTaken_ = 0;
    bool portalOpen_ = false;
    float portalAnim_ = 0;
    float navTimer_ = 0;
    int navX_ = -1, navZ_ = -1;
    float fireEmberTimer_ = 0;
    float spawnTimer_ = 0;
    float poiTimer_ = 0;
    float minimapTimer_ = 0;
    int minimapCX_ = 0, minimapCZ_ = 0;
    std::vector<uint32_t> minimap_;
    std::vector<uint32_t> bigMap_;
    Rng rng_{1234};

    // Sobrevivência
    int wave_ = 0;
    int waveState_ = 0;        // 0 preparando, 1 em andamento, 2 vencida (intervalo)
    float waveTimer_ = 0;      // contagem regressiva / tempo da onda
    int waveTotal_ = 0;        // zumbis desta onda
    int waveSpawned_ = 0;
    int waveKilledBase_ = 0;   // abates antes da onda começar
    float waveSpawnTimer_ = 0;
    bool waveBig_ = false;
    float frontAngles_[3] = {0, 0, 0};
    int frontCount_ = 1;
    float survivalTime_ = 0;
    int score_ = 0;

    FrameScene scene_;
    Pose scratchPose_;
    std::vector<Mat4> scratchGlobals_;
};

}  // namespace zz
