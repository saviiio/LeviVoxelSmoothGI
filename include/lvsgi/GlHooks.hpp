#pragma once
#include "lvsgi/Config.hpp"
#include "lvsgi/BinaryCache.hpp"
#include "lvsgi/ShaderPatcher.hpp"
#include "lvsgi/VoxelRuntime.hpp"
#include "lvsgi/ItemPrewarmer.hpp"

#include <pl/memory/Hook.hpp>
#include <GLES3/gl31.h>
#include <EGL/egl.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lvsgi {

class GlHooks {
public:
    bool arm(const Config&, const std::filesystem::path& dataDir,
             const std::filesystem::path& prewarmFile);
    void uninstall();
    static GlHooks* active();

private:
    struct ShaderState {
        GLenum type{};
        std::string original;
        std::string source;
        bool compileRequested{};
        bool compiled{};
        PatchKind kind{PatchKind::None};
        int binding{-1};
    };

    struct ProgramState {
        std::vector<GLuint> shaders;
        std::vector<std::pair<GLuint, std::string>> attribBindings;
        std::vector<std::string> tfVaryings;
        GLenum tfMode{};
        std::string key;
        PatchKind kind{PatchKind::None};
        int binding{-1};
        bool linked{};
        bool binaryLoaded{};
        bool queuedCapture{};
        GLint uDims{-1};
        GLint uParity{-1};
        GLint uGi{-1};
        GLint uRefStrength{-1};
        GLint uRefRange{-1};
    };

    struct PendingCapture {
        GLuint program{};
        std::string key;
        std::uint64_t readyFrame{};
    };

    void bootstrapLoop();
    bool installMinecraftHooks(std::uintptr_t moduleBase);
    void resetGraphicsHooks();
    bool hookDynamicPlt(const char* label, std::uintptr_t address,
                        void* detour, void** original,
                        pl::memory::HookHandle& handle,
                        bool required = true);
    void initFunctions();
    void initCapabilities();
    std::string makeProgramKey(GLuint, ProgramState&);
    bool compileShaderNow(GLuint, ShaderState&);
    void linkProgram(GLuint);
    void useProgram(GLuint);
    void captureOne();
    void fixScreenEdgeRow();

    static __eglMustCastToProperFunctionPointerType hGetProcAddress(const char*);
    static GLuint hCreateShader(GLenum);
    static void hShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*);
    static void hCompileShader(GLuint);
    static void hGetShaderiv(GLuint, GLenum, GLint*);
    static void hGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar*);
    static void hDeleteShader(GLuint);
    static GLuint hCreateProgram();
    static void hAttachShader(GLuint, GLuint);
    static void hDetachShader(GLuint, GLuint);
    static void hBindAttribLocation(GLuint, GLuint, const GLchar*);
    static void hTransformFeedbackVaryings(GLuint, GLsizei, const GLchar* const*, GLenum);
    static void hLinkProgram(GLuint);
    static void hGetProgramiv(GLuint, GLenum, GLint*);
    static void hDeleteProgram(GLuint);
    static void hUseProgram(GLuint);
    static EGLBoolean hSwap(EGLDisplay, EGLSurface);

    Config cfg_{};
    std::filesystem::path dataDir_;
    std::filesystem::path prewarmFile_;
    std::string minecraftBuildId_;
    ShaderPatcher patcher_;
    BinaryCache cache_;
    VoxelRuntime voxels_;
    ItemPrewarmer prewarmer_;

    mutable std::mutex mu_;
    std::mutex hookMu_;
    std::unordered_map<GLuint, ShaderState> shaders_;
    std::unordered_map<GLuint, ProgramState> programs_;
    std::vector<PendingCapture> captures_;

    std::thread bootstrapThread_;
    std::atomic<bool> bootstrapStop_{false};
    std::atomic<bool> minecraftModuleSeen_{false};
    std::atomic<bool> dynamicResolverReady_{false};
    std::atomic<bool> graphicsInstalled_{false};
    std::atomic<std::uint64_t> hookCalls_{0};
    std::atomic<std::uint64_t> shaderSourceHits_{0};
    std::atomic<std::uint64_t> patchedShaderHits_{0};
    std::atomic<std::uint64_t> patchedProgramLinks_{0};
    std::atomic<std::uint64_t> patchedProgramUses_{0};
    std::atomic<std::uint64_t> swapHits_{0};
    std::atomic<std::uint64_t> cacheHits_{0};
    std::atomic<std::uint64_t> cacheMisses_{0};

    int maxSsboBindings_{8};
    bool capsInit_{};
    bool binarySupport_{};
    bool parallelCompileSupport_{};
    bool voxelInitTried_{};
    std::uint64_t nextVoxelRetryFrame_{};

    using GetProcAddressFn = __eglMustCastToProperFunctionPointerType (*)(const char*);
    using CreateShaderFn = GLuint (*)(GLenum);
    using ShaderSourceFn = void (*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    using CompileShaderFn = void (*)(GLuint);
    using GetShaderivFn = void (*)(GLuint, GLenum, GLint*);
    using GetShaderInfoLogFn = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
    using DeleteShaderFn = void (*)(GLuint);
    using CreateProgramFn = GLuint (*)();
    using AttachShaderFn = void (*)(GLuint, GLuint);
    using DetachShaderFn = void (*)(GLuint, GLuint);
    using BindAttribLocationFn = void (*)(GLuint, GLuint, const GLchar*);
    using TransformFeedbackVaryingsFn = void (*)(GLuint, GLsizei, const GLchar* const*, GLenum);
    using LinkProgramFn = void (*)(GLuint);
    using GetProgramivFn = void (*)(GLuint, GLenum, GLint*);
    using DeleteProgramFn = void (*)(GLuint);
    using UseProgramFn = void (*)(GLuint);
    using ProgramBinaryFn = void (*)(GLuint, GLenum, const void*, GLsizei);
    using GetProgramBinaryFn = void (*)(GLuint, GLsizei, GLsizei*, GLenum*, void*);
    using ProgramParameteriFn = void (*)(GLuint, GLenum, GLint);
    using MaxCompilerThreadsFn = void (*)(GLuint);
    using SwapFn = EGLBoolean (*)(EGLDisplay, EGLSurface);

    GetProcAddressFn fGetProcAddress_{};
    CreateShaderFn fCreateShader_{};
    ShaderSourceFn fShaderSource_{};
    CompileShaderFn fCompileShader_{};
    GetShaderivFn fGetShaderiv_{};
    GetShaderInfoLogFn fGetShaderInfoLog_{};
    DeleteShaderFn fDeleteShader_{};
    CreateProgramFn fCreateProgram_{};
    AttachShaderFn fAttachShader_{};
    DetachShaderFn fDetachShader_{};
    BindAttribLocationFn fBindAttribLocation_{};
    TransformFeedbackVaryingsFn fTransformFeedbackVaryings_{};
    LinkProgramFn fLinkProgram_{};
    GetProgramivFn fGetProgramiv_{};
    DeleteProgramFn fDeleteProgram_{};
    UseProgramFn fUseProgram_{};
    ProgramBinaryFn fProgramBinary_{};
    GetProgramBinaryFn fGetProgramBinary_{};
    ProgramParameteriFn fProgramParameteri_{};
    MaxCompilerThreadsFn fMaxCompilerThreads_{};
    SwapFn fSwap_{};

    pl::memory::HookHandle hkGetProcAddress_;
    pl::memory::HookHandle hkCreateShader_;
    pl::memory::HookHandle hkShaderSource_;
    pl::memory::HookHandle hkCompileShader_;
    pl::memory::HookHandle hkGetShaderiv_;
    pl::memory::HookHandle hkGetShaderInfoLog_;
    pl::memory::HookHandle hkDeleteShader_;
    pl::memory::HookHandle hkCreateProgram_;
    pl::memory::HookHandle hkAttachShader_;
    pl::memory::HookHandle hkDetachShader_;
    pl::memory::HookHandle hkLinkProgram_;
    pl::memory::HookHandle hkGetProgramiv_;
    pl::memory::HookHandle hkDeleteProgram_;
    pl::memory::HookHandle hkUseProgram_;
    pl::memory::HookHandle hkSwap_;
};

} // namespace lvsgi
