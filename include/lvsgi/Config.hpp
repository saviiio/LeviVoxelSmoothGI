#pragma once
#include <cstddef>
#include <algorithm>

namespace lvsgi {

// Simple aggregate on purpose: pl::config::ConfigFile reflects this type.
struct Config {
    int version{9};
    bool enabled{true};
    bool shaderCache{true};
    // Cross-context deferred glCompileShader is unsafe on RenderDragon worker contexts.
    bool deferShaderCompilation{false};
    bool parallelCompile{true};
    bool voxelCapture{true};
    bool hierarchicalDdaReflections{true};
    bool directionalFloodfillGi{true};
    bool disableNativeSsr{true};
    bool logStalls{true};
    bool prewarmItemPipelines{true};
    // The old default-framebuffer blit can force a full tile resolve on Mali.
    // Keep it opt-in until the final RenderDragon presentation pass is hooked.
    bool fixScreenEdgeRow{false};
    int voxelX{64};
    int voxelY{40};
    int voxelZ{64};
    float reflectionRange{48.0f};
    float reflectionStrength{0.70f};
    float giStrength{1.15f};
    float giDecay{0.935f};
    float giTurn{0.11f};
    float giBackTurn{0.025f};
    int maxCacheMiB{192};
    int maxCacheEntries{8192};
    int binaryCaptureDelayFrames{180};
    int binaryCaptureIntervalFrames{8};
    float stallLogMs{8.0f};
    int compilerThreads{6};
    int prewarmStartDelayFrames{10};
    int prewarmProgramsPerFrame{2};
    int prewarmMaxPending{12};
    // Time spent waiting for libminecraftpe.so before reporting a warning.
    // The worker keeps waiting after this; this only controls diagnostics.
    int minecraftHookWarningMs{15000};
};

// JSON-schema bounds are metadata for editors; they do not clamp values at
// runtime. Keep the runtime protected as well so a malformed/old config cannot
// request an enormous SSBO or invalid scheduling values.
inline void sanitizeConfig(Config& c) {
    c.version = 9;
    // Never fake GL_COMPILE_STATUS and move shader compilation to a later
    // RenderDragon context. The tombstone from beta.8.3 showed this can
    // make a valid original shader fail on a Rendering Pool thread.
    c.deferShaderCompilation = false;
    c.voxelX = std::clamp(c.voxelX, 24, 128);
    c.voxelY = std::clamp(c.voxelY, 16, 96);
    c.voxelZ = std::clamp(c.voxelZ, 24, 128);
    c.reflectionRange = std::clamp(c.reflectionRange, 4.0f, 128.0f);
    c.reflectionStrength = std::clamp(c.reflectionStrength, 0.0f, 2.0f);
    c.giStrength = std::clamp(c.giStrength, 0.0f, 3.0f);
    c.giDecay = std::clamp(c.giDecay, 0.5f, 0.99f);
    c.giTurn = std::clamp(c.giTurn, 0.0f, 0.25f);
    c.giBackTurn = std::clamp(c.giBackTurn, 0.0f, 0.1f);
    c.maxCacheMiB = std::clamp(c.maxCacheMiB, 16, 1024);
    c.maxCacheEntries = std::clamp(c.maxCacheEntries, 128, 32768);
    c.binaryCaptureDelayFrames = std::clamp(c.binaryCaptureDelayFrames, 0, 2000);
    c.binaryCaptureIntervalFrames = std::clamp(c.binaryCaptureIntervalFrames, 1, 120);
    c.stallLogMs = std::clamp(c.stallLogMs, 1.0f, 100.0f);
    c.compilerThreads = std::clamp(c.compilerThreads, 1, 16);
    c.prewarmStartDelayFrames = std::clamp(c.prewarmStartDelayFrames, 0, 1200);
    c.prewarmProgramsPerFrame = std::clamp(c.prewarmProgramsPerFrame, 0, 8);
    c.prewarmMaxPending = std::clamp(c.prewarmMaxPending, 1, 64);
    c.minecraftHookWarningMs = std::clamp(c.minecraftHookWarningMs, 1000, 120000);
}

} // namespace lvsgi
