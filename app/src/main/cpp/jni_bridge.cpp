// Ponte JNI: NativeBridge.kt <-> motor C++.
#include <jni.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "core/log.h"
#include "core/platform.h"
#include "game/game.h"
#include "game/perf_bench.h"
#include "game/contract_rules.h"
#include "game/protocol.h"

using zz::Game;
using zz::MissionConfig;

namespace {
std::unique_ptr<Game> gGame;

Game& game() {
    if (!gGame) gGame = std::make_unique<Game>();
    return *gGame;
}
}  // namespace

#define ZZ_FN(name) Java_com_fabiosilva_ecosdeeldra_engine_NativeBridge_##name

extern "C" {

JNIEXPORT void JNICALL ZZ_FN(nativeInit)(JNIEnv* env, jobject thiz, jobject assetManager) {
    jclass cls = env->GetObjectClass(thiz);
    zz::platform::init(env, assetManager, cls);
    env->DeleteLocalRef(cls);
    game();
    LOGI("Motor nativo inicializado");
}

JNIEXPORT void JNICALL ZZ_FN(surfaceCreated)(JNIEnv*, jobject) { game().onSurfaceCreated(); }

JNIEXPORT void JNICALL ZZ_FN(surfaceChanged)(JNIEnv*, jobject, jint width, jint height) { game().onSurfaceChanged(width, height); }

JNIEXPORT void JNICALL ZZ_FN(drawFrame)(JNIEnv*, jobject, jlong nanos) { game().frame(static_cast<double>(nanos) * 1e-9); }

JNIEXPORT void JNICALL ZZ_FN(enterLobby)(JNIEnv*, jobject, jint look, jint screen) { game().enterLobby(static_cast<uint32_t>(look), screen); }

JNIEXPORT void JNICALL ZZ_FN(startMission)(JNIEnv* env, jobject, jfloatArray values, jint look) {
    using namespace zz::proto;
    float c[C_COUNT] = {0};
    jsize n = std::min<jsize>(env->GetArrayLength(values), C_COUNT);
    env->GetFloatArrayRegion(values, 0, n, c);
    MissionConfig cfg;
    cfg.stage = static_cast<int>(c[C_STAGE]);
    cfg.skin = static_cast<uint32_t>(look);
    cfg.zombieCount = static_cast<int>(c[C_ZOMBIES]);
    cfg.zombieHealth = c[C_ZOMBIE_HP];
    cfg.zombieDamage = c[C_ZOMBIE_DMG];
    cfg.zombieSpeed = c[C_ZOMBIE_SPEED];
    cfg.playerHealth = c[C_PLAYER_HP];
    cfg.magazine = static_cast<int>(c[C_MAGAZINE]);
    cfg.power = c[C_POWER];
    cfg.crystalMask = static_cast<int>(c[C_CRYSTAL_MASK]);
    cfg.crystalTotal = static_cast<int>(c[C_CRYSTAL_TOTAL]);
    cfg.sensitivity = c[C_SENSITIVITY];
    cfg.aimAssist = c[C_AIM_ASSIST] > 0.5f;
    cfg.invertY = c[C_INVERT_Y] > 0.5f;
    cfg.seed = static_cast<uint32_t>(c[C_SEED]);
    cfg.reloadSpeed = c[C_RELOAD_SPEED] > 0 ? c[C_RELOAD_SPEED] : 1.0f;
    cfg.boostHealth = c[C_BOOST_HEALTH] > 0 ? c[C_BOOST_HEALTH] : 1.0f;
    cfg.boostDamage = c[C_BOOST_DAMAGE] > 0 ? c[C_BOOST_DAMAGE] : 1.0f;
    cfg.mode = static_cast<int>(c[C_MODE]);
    cfg.armor = c[C_ARMOR];
    cfg.regen = c[C_REGEN];
    cfg.speedMul = c[C_SPEED] > 0 ? c[C_SPEED] : 1.0f;
    cfg.headshotMul = c[C_HEADSHOT] > 0 ? c[C_HEADSHOT] : 1.0f;
    cfg.luck = c[C_LUCK] > 0 ? c[C_LUCK] : 1.0f;
    cfg.explosives = static_cast<int>(c[C_EXPLOSIVES]);
    // Contrato de expedição: "0 = neutro" protege chamadas antigas e arrays zerados.
    cfg.contractId = static_cast<int>(c[C_CONTRACT_ID]);
    cfg.ammoClips = c[C_AMMO_CLIPS] > 0 ? c[C_AMMO_CLIPS] : -1.0f;
    cfg.chestAmmoPct = static_cast<int>(c[C_CHEST_AMMO]);
    cfg.visionMul = c[C_VISION_MUL] > 0 ? c[C_VISION_MUL] : 1.0f;
    cfg.aggression = c[C_AGGRESSION] > 0 ? c[C_AGGRESSION] : 1.0f;
    cfg.redSky = c[C_RED_SKY] > 0.5f;
    cfg.guardMul = c[C_GUARD_MUL] > 0 ? c[C_GUARD_MUL] : 1.0f;
    cfg.nestBomb = c[C_NEST_BOMB];
    cfg.cargo = c[C_CARGO] > 0.5f;
    cfg.perfMode = static_cast<int>(c[C_PERF]);
    cfg.perfFrames = static_cast<int>(c[C_PERF_FRAMES]);
    game().setGraphics(static_cast<int>(c[C_QUALITY]));
    game().startMission(cfg);
}


JNIEXPORT void JNICALL ZZ_FN(setPaused)(JNIEnv*, jobject, jboolean paused) { game().setPaused(paused); }

JNIEXPORT void JNICALL ZZ_FN(revive)(JNIEnv*, jobject) { game().revive(); }

JNIEXPORT void JNICALL ZZ_FN(setSettings)(JNIEnv*, jobject, jfloat sensitivity, jboolean aimAssist, jboolean invertY) {
    game().setSettings(sensitivity, aimAssist, invertY);
}

JNIEXPORT void JNICALL ZZ_FN(setGraphics)(JNIEnv*, jobject, jint quality) { game().setGraphics(quality); }

JNIEXPORT void JNICALL ZZ_FN(requestSupply)(JNIEnv*, jobject) { game().requestSupply(); }

JNIEXPORT void JNICALL ZZ_FN(toggleCrouch)(JNIEnv*, jobject) { game().toggleCrouch(); }

JNIEXPORT void JNICALL ZZ_FN(toggleSprint)(JNIEnv*, jobject) { game().toggleSprint(); }

JNIEXPORT void JNICALL ZZ_FN(pressMelee)(JNIEnv*, jobject) { game().pressMelee(); }

JNIEXPORT void JNICALL ZZ_FN(pressGrenade)(JNIEnv*, jobject) { game().pressGrenade(); }

JNIEXPORT void JNICALL ZZ_FN(swapShoulder)(JNIEnv*, jobject) { game().swapShoulder(); }

JNIEXPORT void JNICALL ZZ_FN(tapSlot)(JNIEnv*, jobject, jint slot) { game().tapSlot(slot); }

JNIEXPORT void JNICALL ZZ_FN(useSlot)(JNIEnv*, jobject, jint slot) { game().useSlot(slot); }

JNIEXPORT void JNICALL ZZ_FN(dropItem)(JNIEnv*, jobject, jint id, jint count) { game().dropFromPlayer(id, count); }

JNIEXPORT void JNICALL ZZ_FN(setThrowAim)(JNIEnv*, jobject, jboolean held, jboolean cancel) { game().setThrowAim(held, cancel); }

JNIEXPORT void JNICALL ZZ_FN(setInventory)(JNIEnv* env, jobject, jintArray values) {
    int data[zz::proto::kInvIntCount] = {0};
    jsize n = std::min<jsize>(env->GetArrayLength(values), zz::proto::kInvIntCount);
    env->GetIntArrayRegion(values, 0, n, reinterpret_cast<jint*>(data));
    game().setInventory(data, n);
}

JNIEXPORT void JNICALL ZZ_FN(setMove)(JNIEnv*, jobject, jfloat x, jfloat y) { game().setMove(x, y); }

JNIEXPORT void JNICALL ZZ_FN(addLook)(JNIEnv*, jobject, jfloat dx, jfloat dy) { game().addLook(dx, dy); }

JNIEXPORT void JNICALL ZZ_FN(setFire)(JNIEnv*, jobject, jboolean held) { game().setFire(held); }

JNIEXPORT void JNICALL ZZ_FN(setAim)(JNIEnv*, jobject, jboolean on) { game().setAim(on); }
JNIEXPORT void JNICALL ZZ_FN(setAimStage)(JNIEnv*, jobject, jint stage) { game().setAimStage(stage); }

JNIEXPORT void JNICALL ZZ_FN(pressJump)(JNIEnv*, jobject) { game().pressJump(); }

JNIEXPORT void JNICALL ZZ_FN(pressReload)(JNIEnv*, jobject) { game().pressReload(); }

JNIEXPORT void JNICALL ZZ_FN(copyHud)(JNIEnv* env, jobject, jfloatArray out) {
    float buffer[zz::proto::H_SIZE];
    jsize n = std::min<jsize>(env->GetArrayLength(out), zz::proto::H_SIZE);
    game().copyHud(buffer, n);
    env->SetFloatArrayRegion(out, 0, n, buffer);
}

JNIEXPORT jint JNICALL ZZ_FN(drainEvents)(JNIEnv* env, jobject, jintArray out) {
    jsize capacity = env->GetArrayLength(out);
    std::vector<int> buffer(static_cast<size_t>(capacity));
    int n = game().drainEvents(buffer.data(), capacity);
    if (n > 0) env->SetIntArrayRegion(out, 0, n, buffer.data());
    return n;
}

JNIEXPORT void JNICALL ZZ_FN(copyResult)(JNIEnv* env, jobject, jfloatArray out) {
    float buffer[zz::proto::R_SIZE];
    jsize n = std::min<jsize>(env->GetArrayLength(out), zz::proto::R_SIZE);
    game().copyResult(buffer, n);
    env->SetFloatArrayRegion(out, 0, n, buffer);
}

JNIEXPORT jint JNICALL ZZ_FN(minimapSize)(JNIEnv*, jobject) { return game().minimapSize(); }

JNIEXPORT void JNICALL ZZ_FN(copyMinimap)(JNIEnv* env, jobject, jintArray out) {
    jsize n = env->GetArrayLength(out);
    std::vector<uint32_t> buffer(static_cast<size_t>(n));
    game().copyMinimap(buffer.data(), n);
    env->SetIntArrayRegion(out, 0, n, reinterpret_cast<const jint*>(buffer.data()));
}

JNIEXPORT jint JNICALL ZZ_FN(bigMapSize)(JNIEnv*, jobject) { return game().bigMapSize(); }

JNIEXPORT void JNICALL ZZ_FN(copyBigMap)(JNIEnv* env, jobject, jintArray out) {
    jsize n = env->GetArrayLength(out);
    std::vector<uint32_t> buffer(static_cast<size_t>(n));
    game().copyBigMap(buffer.data(), n);
    env->SetIntArrayRegion(out, 0, n, reinterpret_cast<const jint*>(buffer.data()));
}

// Autoteste das regras de reserva de munição dos contratos (diagnóstico; só o Kotlin de depuração chama).
// Retorna 0 se tudo passou; senão (linha da primeira falha em contract_rules.h) * 1000 + número de falhas.
JNIEXPORT jint JNICALL ZZ_FN(contractSelfTest)(JNIEnv*, jobject) {
    int line = 0;
    int failed = zz::contractrules::selfTest(&line);
    return failed == 0 ? 0 : line * 1000 + std::min(failed, 999);
}

// Bancada headless de streaming (diagnóstico; só o Kotlin de depuração chama). Roda na thread GL, sem renderizar.
JNIEXPORT jint JNICALL ZZ_FN(perfBench)(JNIEnv*, jobject, jint seed, jint stage, jint radius, jint meters, jint policy, jint flags) {
    zz::perf::BenchConfig c;
    c.seed = static_cast<uint32_t>(seed);
    c.stage = stage;
    c.radius = std::max(3, std::min(12, static_cast<int>(radius)));
    c.meters = std::max(50, static_cast<int>(meters));
    c.policy = std::max(0, std::min(2, static_cast<int>(policy)));
    c.selfTest = (flags & 1) != 0;
    return zz::perf::runBench(c);
}

}  // extern "C"
