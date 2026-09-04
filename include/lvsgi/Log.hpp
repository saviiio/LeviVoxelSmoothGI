#pragma once
#include <android/log.h>
#define LVSGI_TAG "LeviVoxelGI"
#define LVSGI_I(...) __android_log_print(ANDROID_LOG_INFO, LVSGI_TAG, __VA_ARGS__)
#define LVSGI_W(...) __android_log_print(ANDROID_LOG_WARN, LVSGI_TAG, __VA_ARGS__)
#define LVSGI_E(...) __android_log_print(ANDROID_LOG_ERROR, LVSGI_TAG, __VA_ARGS__)
