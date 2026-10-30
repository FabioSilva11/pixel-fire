#pragma once

#if defined(__ANDROID__)
#include <android/log.h>

#define ZZ_TAG "ZombieZoneNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, ZZ_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, ZZ_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, ZZ_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) (std::fprintf(stderr, __VA_ARGS__), std::fputc('\n', stderr))
#define LOGW(...) LOGI(__VA_ARGS__)
#define LOGE(...) LOGI(__VA_ARGS__)
#endif
