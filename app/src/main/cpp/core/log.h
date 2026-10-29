#pragma once

#include <android/log.h>

#define ZZ_TAG "ZombieZoneNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, ZZ_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, ZZ_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, ZZ_TAG, __VA_ARGS__)
