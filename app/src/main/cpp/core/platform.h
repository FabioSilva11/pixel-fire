// Acesso a recursos do Android: AssetManager e decodificação de PNG via BitmapFactory (Kotlin).
#pragma once

#include <android/asset_manager.h>
#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

namespace zz {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;  // RGBA8, linha 0 no topo
};

namespace platform {

void init(JNIEnv* env, jobject assetManager, jclass bridgeClass);
bool readAsset(const std::string& path, std::vector<uint8_t>& out);
// Decodifica uma imagem dos assets com BitmapFactory; maxSize limita o maior lado.
bool decodeAssetImage(const std::string& path, int maxSize, Image& out);

}  // namespace platform
}  // namespace zz
