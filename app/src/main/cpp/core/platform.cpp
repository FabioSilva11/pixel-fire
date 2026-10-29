#include "platform.h"

#include <android/asset_manager_jni.h>

#include "log.h"

namespace zz::platform {

namespace {
JavaVM* gVm = nullptr;
jobject gAssetManagerRef = nullptr;
AAssetManager* gAssets = nullptr;
jclass gBridgeClass = nullptr;
jmethodID gDecodeMethod = nullptr;

JNIEnv* currentEnv() {
    if (!gVm) return nullptr;
    JNIEnv* env = nullptr;
    if (gVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) return nullptr;
    return env;
}
}  // namespace

void init(JNIEnv* env, jobject assetManager, jclass bridgeClass) {
    env->GetJavaVM(&gVm);
    if (gAssetManagerRef) env->DeleteGlobalRef(gAssetManagerRef);
    gAssetManagerRef = env->NewGlobalRef(assetManager);
    gAssets = AAssetManager_fromJava(env, gAssetManagerRef);
    if (gBridgeClass) env->DeleteGlobalRef(gBridgeClass);
    gBridgeClass = static_cast<jclass>(env->NewGlobalRef(bridgeClass));
    gDecodeMethod = env->GetStaticMethodID(gBridgeClass, "decodeAssetImage", "(Ljava/lang/String;I)[I");
    if (!gDecodeMethod) {
        env->ExceptionClear();
        LOGE("NativeBridge.decodeAssetImage não encontrado");
    }
}

bool readAsset(const std::string& path, std::vector<uint8_t>& out) {
    if (!gAssets) return false;
    AAsset* asset = AAssetManager_open(gAssets, path.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Asset ausente: %s", path.c_str());
        return false;
    }
    off_t length = AAsset_getLength(asset);
    out.resize(static_cast<size_t>(length));
    int read = AAsset_read(asset, out.data(), static_cast<size_t>(length));
    AAsset_close(asset);
    return read == length;
}

bool decodeAssetImage(const std::string& path, int maxSize, Image& out) {
    JNIEnv* env = currentEnv();
    if (!env || !gBridgeClass || !gDecodeMethod) return false;
    jstring jPath = env->NewStringUTF(path.c_str());
    auto result = static_cast<jintArray>(env->CallStaticObjectMethod(gBridgeClass, gDecodeMethod, jPath, maxSize));
    env->DeleteLocalRef(jPath);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    if (!result) return false;
    jsize count = env->GetArrayLength(result);
    if (count < 2) { env->DeleteLocalRef(result); return false; }
    jint* data = env->GetIntArrayElements(result, nullptr);
    out.width = data[0];
    out.height = data[1];
    size_t pixels = static_cast<size_t>(out.width) * static_cast<size_t>(out.height);
    bool ok = static_cast<size_t>(count) >= pixels + 2;
    if (ok) {
        out.rgba.resize(pixels * 4);
        for (size_t i = 0; i < pixels; ++i) {
            auto argb = static_cast<uint32_t>(data[2 + i]);
            out.rgba[i * 4 + 0] = static_cast<uint8_t>((argb >> 16) & 0xFF);
            out.rgba[i * 4 + 1] = static_cast<uint8_t>((argb >> 8) & 0xFF);
            out.rgba[i * 4 + 2] = static_cast<uint8_t>(argb & 0xFF);
            out.rgba[i * 4 + 3] = static_cast<uint8_t>((argb >> 24) & 0xFF);
        }
    }
    env->ReleaseIntArrayElements(result, data, JNI_ABORT);
    env->DeleteLocalRef(result);
    return ok;
}

}  // namespace zz::platform
