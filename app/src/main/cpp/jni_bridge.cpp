// Ponte JNI: NativeBridge.kt <-> motor C++.
#include <jni.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "core/log.h"
#include "core/platform.h"
#include "game/game.h"

#include "game/protocol.h"

using zz::Game;
namespace {
std::unique_ptr<Game> gGame;
Game &game() {
    if (!gGame)
        gGame = std::make_unique<Game>();
    return *gGame;
}
} // namespace
#define ZZ_FN(name) Java_com_fabiosilva_ecosdeeldra_engine_NativeBridge_##name
extern "C" {
JNIEXPORT void JNICALL ZZ_FN(nativeInit)(JNIEnv *env, jobject thiz, jobject assetManager) {
    jclass cls = env->GetObjectClass(thiz);
    zz::platform::init(env, assetManager, cls);
    env->DeleteLocalRef(cls);
    game();
    LOGI("Motor nativo inicializado");
}
JNIEXPORT void JNICALL ZZ_FN(surfaceCreated)(JNIEnv *, jobject) { game().onSurfaceCreated(); }
JNIEXPORT void JNICALL ZZ_FN(surfaceChanged)(JNIEnv *, jobject, jint width, jint height) {
    game().onSurfaceChanged(width, height);
}
JNIEXPORT void JNICALL ZZ_FN(drawFrame)(JNIEnv *, jobject, jlong nanos) {
    game().frame(static_cast<double>(nanos) * 1e-9);
}
JNIEXPORT void JNICALL ZZ_FN(enterLobby)(JNIEnv *, jobject, jint look, jint screen) {
    game().enterLobby(static_cast<uint32_t>(look), screen);
}
JNIEXPORT void JNICALL ZZ_FN(previewWeapon)(JNIEnv *, jobject, jint id) { game().previewWeapon(id); }
JNIEXPORT void JNICALL ZZ_FN(setPaused)(JNIEnv *, jobject, jboolean paused) { game().setPaused(paused); }
JNIEXPORT void JNICALL ZZ_FN(setSettings)(JNIEnv *, jobject, jfloat sensitivity, jboolean aimAssist,
                                          jboolean invertY) {
    game().setSettings(sensitivity, aimAssist, invertY);
}
JNIEXPORT void JNICALL ZZ_FN(setGraphics)(JNIEnv *, jobject, jint quality) { game().setGraphics(quality); }
JNIEXPORT void JNICALL ZZ_FN(toggleCrouch)(JNIEnv *, jobject) { game().toggleCrouch(); }
JNIEXPORT void JNICALL ZZ_FN(toggleSprint)(JNIEnv *, jobject) { game().toggleSprint(); }
JNIEXPORT void JNICALL ZZ_FN(pressMelee)(JNIEnv *, jobject) { game().pressMelee(); }
JNIEXPORT void JNICALL ZZ_FN(pressGrenade)(JNIEnv *, jobject) { game().pressGrenade(); }
JNIEXPORT void JNICALL ZZ_FN(swapShoulder)(JNIEnv *, jobject) { game().swapShoulder(); }
JNIEXPORT void JNICALL ZZ_FN(tapSlot)(JNIEnv *, jobject, jint slot) { game().tapSlot(slot); }
JNIEXPORT void JNICALL ZZ_FN(useSlot)(JNIEnv *, jobject, jint slot) { game().useSlot(slot); }
JNIEXPORT void JNICALL ZZ_FN(dropItem)(JNIEnv *, jobject, jint id, jint count) {
    game().dropFromPlayer(id, count);
}
JNIEXPORT void JNICALL ZZ_FN(setThrowAim)(JNIEnv *, jobject, jboolean held, jboolean cancel) {
    game().setThrowAim(held, cancel);
}
JNIEXPORT void JNICALL ZZ_FN(setMove)(JNIEnv *, jobject, jfloat x, jfloat y) { game().setMove(x, y); }
JNIEXPORT void JNICALL ZZ_FN(addLook)(JNIEnv *, jobject, jfloat dx, jfloat dy) { game().addLook(dx, dy); }
JNIEXPORT void JNICALL ZZ_FN(setFire)(JNIEnv *, jobject, jboolean held) { game().setFire(held); }
JNIEXPORT void JNICALL ZZ_FN(setAim)(JNIEnv *, jobject, jboolean on) { game().setAim(on); }
JNIEXPORT void JNICALL ZZ_FN(setAimStage)(JNIEnv *, jobject, jint stage) { game().setAimStage(stage); }
JNIEXPORT void JNICALL ZZ_FN(pressJump)(JNIEnv *, jobject) { game().pressJump(); }
JNIEXPORT void JNICALL ZZ_FN(pressReload)(JNIEnv *, jobject) { game().pressReload(); }
JNIEXPORT void JNICALL ZZ_FN(copyHud)(JNIEnv *env, jobject, jfloatArray out) {
    float buffer[zz::proto::H_SIZE];
    jsize n = std::min<jsize>(env->GetArrayLength(out), zz::proto::H_SIZE);
    game().copyHud(buffer, n);
    env->SetFloatArrayRegion(out, 0, n, buffer);
}
JNIEXPORT jint JNICALL ZZ_FN(drainEvents)(JNIEnv *env, jobject, jintArray out) {
    jsize capacity = env->GetArrayLength(out);
    std::vector<int> buffer(static_cast<size_t>(capacity));
    int n = game().drainEvents(buffer.data(), capacity);
    if (n > 0)
        env->SetIntArrayRegion(out, 0, n, buffer.data());
    return n;
}
JNIEXPORT jint JNICALL ZZ_FN(minimapSize)(JNIEnv *, jobject) { return game().minimapSize(); }
JNIEXPORT void JNICALL ZZ_FN(copyMinimap)(JNIEnv *env, jobject, jintArray out) {
    jsize n = env->GetArrayLength(out);
    std::vector<uint32_t> buffer(static_cast<size_t>(n));
    game().copyMinimap(buffer.data(), n);
    env->SetIntArrayRegion(out, 0, n, reinterpret_cast<const jint *>(buffer.data()));
}
JNIEXPORT jint JNICALL ZZ_FN(bigMapSize)(JNIEnv *, jobject) { return game().bigMapSize(); }
JNIEXPORT void JNICALL ZZ_FN(copyBigMap)(JNIEnv *env, jobject, jintArray out) {
    jsize n = env->GetArrayLength(out);
    std::vector<uint32_t> buffer(static_cast<size_t>(n));
    game().copyBigMap(buffer.data(), n);
    env->SetIntArrayRegion(out, 0, n, reinterpret_cast<const jint *>(buffer.data()));
}
JNIEXPORT jboolean JNICALL ZZ_FN(prepareBattle)(JNIEnv *, jobject, jint id, jint look, jint sequence,
                                                jboolean resume) {
    return game().prepareBattle(id, uint32_t(look), uint32_t(sequence), resume);
}
JNIEXPORT void JNICALL ZZ_FN(applyBattleSnapshot)(JNIEnv *e, jobject, jfloatArray a) {
    jsize n = e->GetArrayLength(a);
    if (n < 72 || n > 24000)
        return;
    std::vector<float> b(n);
    e->GetFloatArrayRegion(a, 0, n, b.data());
    game().applyBattleSnapshot(b.data(), n);
}
JNIEXPORT void JNICALL ZZ_FN(applyBattleEffects)(JNIEnv *e, jobject, jfloatArray a) {
    const jsize n = e->GetArrayLength(a);
    if (n < 2 || n > 602)
        return;
    std::vector<float> data(n);
    e->GetFloatArrayRegion(a, 0, n, data.data());
    game().applyBattleEffects(data.data(), data.size());
}
JNIEXPORT jfloatArray JNICALL ZZ_FN(pollBattleInput)(JNIEnv *e, jobject) {
    float b[12];
    int n = game().pollBattleInput(b, 12);
    jfloatArray a = e->NewFloatArray(n);
    if (n)
        e->SetFloatArrayRegion(a, 0, n, b);
    return a;
}
JNIEXPORT jlong JNICALL ZZ_FN(battleMapHash)(JNIEnv *, jobject) { return jlong(game().battleMapHash()); }
JNIEXPORT void JNICALL ZZ_FN(battleAction)(JNIEnv *, jobject, jint b, jint t, jfloat x, jfloat z) {
    game().action(b, t, x, z);
}
}
