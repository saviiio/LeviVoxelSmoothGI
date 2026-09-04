#include "lvsgi/GlHooks.hpp"
#include "lvsgi/Log.hpp"
#include "lvsgi/Sha256.hpp"
#include "CurrentMinecraftProfile.hpp"

#include <pl/memory/Patch.hpp>

#include <EGL/egl.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <elf.h>
#include <iomanip>
#include <link.h>
#include <sstream>
#include <string_view>
#include <type_traits>

#ifndef GL_PROGRAM_BINARY_RETRIEVABLE_HINT
#define GL_PROGRAM_BINARY_RETRIEVABLE_HINT 0x8257
#endif
#ifndef GL_NUM_PROGRAM_BINARY_FORMATS
#define GL_NUM_PROGRAM_BINARY_FORMATS 0x87FE
#endif
#ifndef GL_PROGRAM_BINARY_LENGTH
#define GL_PROGRAM_BINARY_LENGTH 0x8741
#endif

namespace lvsgi {
namespace {
std::atomic<GlHooks*> gActive{nullptr};
thread_local bool gInside = false;
using Clock = std::chrono::steady_clock;

double elapsedMs(Clock::time_point a) {
    return std::chrono::duration<double, std::milli>(Clock::now() - a).count();
}

std::string shaderText(GLsizei count, const GLchar* const* strings,
                       const GLint* lengths) {
    std::string s;
    for (GLsizei i = 0; i < count; ++i) {
        if (!strings || !strings[i]) continue;
        if (lengths && lengths[i] >= 0)
            s.append(strings[i], strings[i] + lengths[i]);
        else
            s += strings[i];
    }
    return s;
}

const char* safeString(GLenum e) {
    auto* p = glGetString(e);
    return p ? reinterpret_cast<const char*>(p) : "";
}

struct Guard {
    Guard() { gInside = true; }
    ~Guard() { gInside = false; }
};

struct MinecraftModuleInfo {
    bool found{};
    uintptr_t base{};
    std::string path;
    std::string buildId;
    const ElfW(Phdr)* phdr{};
    ElfW(Half) phnum{};
};

constexpr std::size_t align4(std::size_t n) { return (n + 3u) & ~std::size_t(3u); }

std::string readBuildId(const dl_phdr_info* info) {
    for (ElfW(Half) i = 0; i < info->dlpi_phnum; ++i) {
        const auto& ph = info->dlpi_phdr[i];
        if (ph.p_type != PT_NOTE || ph.p_memsz < sizeof(ElfW(Nhdr))) continue;
        const auto* cur = reinterpret_cast<const std::uint8_t*>(info->dlpi_addr + ph.p_vaddr);
        const auto* end = cur + ph.p_memsz;
        while (cur + sizeof(ElfW(Nhdr)) <= end) {
            const auto* nh = reinterpret_cast<const ElfW(Nhdr)*>(cur);
            cur += sizeof(ElfW(Nhdr));
            const std::size_t namesz = align4(nh->n_namesz);
            const std::size_t descsz = align4(nh->n_descsz);
            if (cur + namesz > end) break;
            const auto* name = cur;
            cur += namesz;
            if (cur + descsz > end) break;
            const auto* desc = cur;
            cur += descsz;
            if (nh->n_type != NT_GNU_BUILD_ID || nh->n_namesz < 3 ||
                std::memcmp(name, "GNU", 3) != 0 || nh->n_descsz == 0) {
                continue;
            }
            std::ostringstream out;
            out << std::hex << std::setfill('0');
            for (std::size_t b = 0; b < nh->n_descsz; ++b)
                out << std::setw(2) << static_cast<unsigned>(desc[b]);
            return out.str();
        }
    }
    return {};
}

int findMinecraftCallback(dl_phdr_info* info, std::size_t, void* opaque) {
    auto& result = *static_cast<MinecraftModuleInfo*>(opaque);
    if (!info || !info->dlpi_name) return 0;
    const std::string_view name(info->dlpi_name);
    if (name.find("libminecraftpe.so") == std::string_view::npos) return 0;
    result.found = true;
    result.base = static_cast<uintptr_t>(info->dlpi_addr);
    result.path = std::string(name);
    result.buildId = readBuildId(info);
    result.phdr = info->dlpi_phdr;
    result.phnum = info->dlpi_phnum;
    return 1;
}

MinecraftModuleInfo findMinecraftModule() {
    MinecraftModuleInfo result;
    dl_iterate_phdr(&findMinecraftCallback, &result);
    return result;
}

std::uintptr_t modulePointer(const MinecraftModuleInfo& module, ElfW(Addr) value) {
    // Dynamic-table pointers and relocation offsets in ET_DYN images are virtual
    // addresses. Android normally leaves those values relative to the load bias.
    // If a loader has already materialized an absolute pointer, keep it unchanged.
    std::uintptr_t minAddress = static_cast<std::uintptr_t>(-1);
    std::uintptr_t maxAddress = 0;
    for (ElfW(Half) i = 0; i < module.phnum; ++i) {
        const auto& ph = module.phdr[i];
        if (ph.p_type != PT_LOAD) continue;
        const auto begin = module.base + static_cast<std::uintptr_t>(ph.p_vaddr);
        const auto end = begin + static_cast<std::uintptr_t>(ph.p_memsz);
        minAddress = std::min(minAddress, begin);
        maxAddress = std::max(maxAddress, end);
    }
    const auto raw = static_cast<std::uintptr_t>(value);
    if (raw >= minAddress && raw < maxAddress) return raw;
    return module.base + raw;
}

std::uintptr_t findPltGotSlot(const MinecraftModuleInfo& module, const char* symbol) {
    const ElfW(Dyn)* dynamic = nullptr;
    for (ElfW(Half) i = 0; i < module.phnum; ++i) {
        const auto& ph = module.phdr[i];
        if (ph.p_type == PT_DYNAMIC) {
            dynamic = reinterpret_cast<const ElfW(Dyn)*>(
                module.base + static_cast<std::uintptr_t>(ph.p_vaddr));
            break;
        }
    }
    if (!dynamic) return 0;

    const ElfW(Sym)* symtab = nullptr;
    const char* strtab = nullptr;
    const void* jmprel = nullptr;
    std::size_t pltrelsz = 0;
    ElfW(Sword) pltrelType = 0;
    for (const ElfW(Dyn)* d = dynamic; d->d_tag != DT_NULL; ++d) {
        switch (d->d_tag) {
            case DT_SYMTAB:
                symtab = reinterpret_cast<const ElfW(Sym)*>(modulePointer(module, d->d_un.d_ptr));
                break;
            case DT_STRTAB:
                strtab = reinterpret_cast<const char*>(modulePointer(module, d->d_un.d_ptr));
                break;
            case DT_JMPREL:
                jmprel = reinterpret_cast<const void*>(modulePointer(module, d->d_un.d_ptr));
                break;
            case DT_PLTRELSZ:
                pltrelsz = static_cast<std::size_t>(d->d_un.d_val);
                break;
            case DT_PLTREL:
                pltrelType = static_cast<ElfW(Sword)>(d->d_un.d_val);
                break;
            default:
                break;
        }
    }
    if (!symtab || !strtab || !jmprel || pltrelsz == 0) return 0;

    if (pltrelType == DT_RELA) {
        const auto* rel = static_cast<const ElfW(Rela)*>(jmprel);
        const std::size_t count = pltrelsz / sizeof(ElfW(Rela));
        for (std::size_t i = 0; i < count; ++i) {
            const auto symIndex = static_cast<std::size_t>(ELF64_R_SYM(rel[i].r_info));
            const char* name = strtab + symtab[symIndex].st_name;
            if (name && std::strcmp(name, symbol) == 0)
                return modulePointer(module, rel[i].r_offset);
        }
    } else if (pltrelType == DT_REL) {
        const auto* rel = static_cast<const ElfW(Rel)*>(jmprel);
        const std::size_t count = pltrelsz / sizeof(ElfW(Rel));
        for (std::size_t i = 0; i < count; ++i) {
            const auto symIndex = static_cast<std::size_t>(ELF64_R_SYM(rel[i].r_info));
            const char* name = strtab + symtab[symIndex].st_name;
            if (name && std::strcmp(name, symbol) == 0)
                return modulePointer(module, rel[i].r_offset);
        }
    }
    return 0;
}

std::int64_t signExtend(std::uint64_t value, unsigned bits) {
    const std::uint64_t sign = std::uint64_t{1} << (bits - 1u);
    return static_cast<std::int64_t>((value ^ sign) - sign);
}

bool decodeAarch64PltStub(std::uintptr_t pc, const std::uint8_t* bytes,
                          std::uintptr_t& gotAddress) {
    std::uint32_t a = 0, l = 0, add = 0, br = 0;
    std::memcpy(&a, bytes + 0, 4);
    std::memcpy(&l, bytes + 4, 4);
    std::memcpy(&add, bytes + 8, 4);
    std::memcpy(&br, bytes + 12, 4);
    // adrp x16, ... ; ldr x17,[x16,#imm] ; add x16,x16,#imm ; br x17
    if ((a & 0x9f00001fu) != 0x90000010u) return false;
    if ((l & 0xffc003ffu) != 0xf9400211u) return false;
    if ((add & 0xffc003ffu) != 0x91000210u) return false;
    if (br != 0xd61f0220u) return false;

    const std::uint64_t immlo = (a >> 29u) & 0x3u;
    const std::uint64_t immhi = (a >> 5u) & 0x7ffffu;
    const std::int64_t pageDelta = signExtend((immhi << 2u) | immlo, 21u) << 12u;
    const auto pcPage = static_cast<std::int64_t>(pc & ~std::uintptr_t{0xfffu});
    const auto page = static_cast<std::uintptr_t>(pcPage + pageDelta);
    const std::uintptr_t ldrOffset = static_cast<std::uintptr_t>((l >> 10u) & 0xfffu) * 8u;
    const std::uintptr_t addOffset = static_cast<std::uintptr_t>((add >> 10u) & 0xfffu);
    if (ldrOffset != addOffset) return false;
    gotAddress = page + ldrOffset;
    return true;
}

std::uintptr_t findPltStubForSymbol(const MinecraftModuleInfo& module, const char* symbol) {
    const std::uintptr_t got = findPltGotSlot(module, symbol);
    if (!got) return 0;
    for (ElfW(Half) i = 0; i < module.phnum; ++i) {
        const auto& ph = module.phdr[i];
        if (ph.p_type != PT_LOAD || (ph.p_flags & PF_X) == 0 || ph.p_memsz < 16) continue;
        const std::uintptr_t begin = module.base + static_cast<std::uintptr_t>(ph.p_vaddr);
        const std::uintptr_t end = begin + static_cast<std::uintptr_t>(ph.p_memsz) - 16u;
        for (std::uintptr_t pc = begin; pc <= end; pc += 4u) {
            std::uintptr_t decodedGot = 0;
            if (decodeAarch64PltStub(pc, reinterpret_cast<const std::uint8_t*>(pc), decodedGot) &&
                decodedGot == got) {
                return pc;
            }
        }
    }
    return 0;
}

} // namespace

GlHooks* GlHooks::active() { return gActive.load(std::memory_order_acquire); }

bool GlHooks::hookDynamicPlt(const char* label, std::uintptr_t address,
                             void* detour, void** original,
                             pl::memory::HookHandle& handle, bool required) {
    if (!address) {
        if (required) LVSGI_E("DYNAMIC PLT RESOLVE FAILED: %s", label);
        else LVSGI_W("optional dynamic PLT resolve failed: %s", label);
        return !required;
    }
    // Re-read the four instructions through Levi's public Patch API immediately
    // before installing the hook. This prevents a stale/incorrect scan result
    // from being treated as executable code.
    const auto bytes = pl::memory::readBytes(address, 16);
    std::uintptr_t ignoredGot = 0;
    if (bytes.size() != 16 ||
        !decodeAarch64PltStub(address, bytes.data(), ignoredGot)) {
        if (required) LVSGI_E("DYNAMIC PLT VALIDATION FAILED: %s @ %p", label,
                              reinterpret_cast<void*>(address));
        else LVSGI_W("optional dynamic PLT validation failed: %s", label);
        return !required;
    }
    LVSGI_I("DYNAMIC PLT RESOLVED: %s @ %p", label, reinterpret_cast<void*>(address));
    handle = pl::memory::HookHandle(reinterpret_cast<void*>(address), detour,
                                    original, pl::memory::HookPriority::Normal);
    if (!handle.installed() || !*original) {
        if (required) LVSGI_E("HOOK INSTALL FAILED: %s", label);
        else LVSGI_W("optional hook failed: %s", label);
        return !required;
    }
    LVSGI_I("HOOK INSTALLED: %s", label);
    return true;
}

void GlHooks::resetGraphicsHooks() {
    hkSwap_.reset();
    hkUseProgram_.reset();
    hkDeleteProgram_.reset();
    hkGetProgramiv_.reset();
    hkLinkProgram_.reset();
    hkDetachShader_.reset();
    hkAttachShader_.reset();
    hkCreateProgram_.reset();
    hkDeleteShader_.reset();
    hkGetShaderInfoLog_.reset();
    hkGetShaderiv_.reset();
    hkCompileShader_.reset();
    hkShaderSource_.reset();
    hkCreateShader_.reset();
    hkGetProcAddress_.reset();

    fGetProcAddress_ = nullptr;
    fCreateShader_ = nullptr;
    fShaderSource_ = nullptr;
    fCompileShader_ = nullptr;
    fGetShaderiv_ = nullptr;
    fGetShaderInfoLog_ = nullptr;
    fDeleteShader_ = nullptr;
    fCreateProgram_ = nullptr;
    fAttachShader_ = nullptr;
    fDetachShader_ = nullptr;
    fBindAttribLocation_ = nullptr;
    fTransformFeedbackVaryings_ = nullptr;
    fLinkProgram_ = nullptr;
    fGetProgramiv_ = nullptr;
    fDeleteProgram_ = nullptr;
    fUseProgram_ = nullptr;
    fProgramBinary_ = nullptr;
    fGetProgramBinary_ = nullptr;
    fProgramParameteri_ = nullptr;
    fMaxCompilerThreads_ = nullptr;
    fSwap_ = nullptr;
    graphicsInstalled_.store(false, std::memory_order_release);
}

bool GlHooks::installMinecraftHooks(std::uintptr_t moduleBase) {
    std::lock_guard lock(hookMu_);
    if (graphicsInstalled_.load(std::memory_order_acquire)) return true;

    resetGraphicsHooks();
    const auto module = findMinecraftModule();
    if (!module.found || module.base != moduleBase || !module.phdr || module.phnum == 0) {
        LVSGI_E("dynamic RenderDragon resolver lost libminecraftpe.so while installing hooks");
        return false;
    }

    struct Target { const char* symbol; const char* label; std::uintptr_t address; };
    Target targets[] = {
        {"eglGetProcAddress", "libminecraftpe!eglGetProcAddress@plt", 0},
        {"glCreateShader", "libminecraftpe!glCreateShader@plt", 0},
        {"glShaderSource", "libminecraftpe!glShaderSource@plt", 0},
        {"glCompileShader", "libminecraftpe!glCompileShader@plt", 0},
        {"glGetShaderiv", "libminecraftpe!glGetShaderiv@plt", 0},
        {"glGetShaderInfoLog", "libminecraftpe!glGetShaderInfoLog@plt", 0},
        {"glDeleteShader", "libminecraftpe!glDeleteShader@plt", 0},
        {"glCreateProgram", "libminecraftpe!glCreateProgram@plt", 0},
        {"glAttachShader", "libminecraftpe!glAttachShader@plt", 0},
        {"glDetachShader", "libminecraftpe!glDetachShader@plt", 0},
        {"glLinkProgram", "libminecraftpe!glLinkProgram@plt", 0},
        {"glGetProgramiv", "libminecraftpe!glGetProgramiv@plt", 0},
        {"glDeleteProgram", "libminecraftpe!glDeleteProgram@plt", 0},
        {"glUseProgram", "libminecraftpe!glUseProgram@plt", 0},
        {"eglSwapBuffers", "libminecraftpe!eglSwapBuffers@plt", 0},
    };
    // Resolve the complete set before installing any hook. This preserves the
    // beta.8.4 safety property: RenderDragon never observes a partial detour set.
    for (auto& target : targets) {
        target.address = findPltStubForSymbol(module, target.symbol);
        if (!target.address) {
            LVSGI_E("DYNAMIC PLT DISCOVERY FAILED: symbol=%s", target.symbol);
            return false;
        }
    }
    dynamicResolverReady_.store(true, std::memory_order_release);

    bool ok = true;
    std::size_t n = 0;
#define LVSGI_HOOK_DYNAMIC(detour, original, handle) \
    ok &= hookDynamicPlt(targets[n].label, targets[n].address, \
                         reinterpret_cast<void*>(detour), \
                         reinterpret_cast<void**>(original), handle); ++n
    LVSGI_HOOK_DYNAMIC(&hGetProcAddress, &fGetProcAddress_, hkGetProcAddress_);
    LVSGI_HOOK_DYNAMIC(&hCreateShader, &fCreateShader_, hkCreateShader_);
    LVSGI_HOOK_DYNAMIC(&hShaderSource, &fShaderSource_, hkShaderSource_);
    LVSGI_HOOK_DYNAMIC(&hCompileShader, &fCompileShader_, hkCompileShader_);
    LVSGI_HOOK_DYNAMIC(&hGetShaderiv, &fGetShaderiv_, hkGetShaderiv_);
    LVSGI_HOOK_DYNAMIC(&hGetShaderInfoLog, &fGetShaderInfoLog_, hkGetShaderInfoLog_);
    LVSGI_HOOK_DYNAMIC(&hDeleteShader, &fDeleteShader_, hkDeleteShader_);
    LVSGI_HOOK_DYNAMIC(&hCreateProgram, &fCreateProgram_, hkCreateProgram_);
    LVSGI_HOOK_DYNAMIC(&hAttachShader, &fAttachShader_, hkAttachShader_);
    LVSGI_HOOK_DYNAMIC(&hDetachShader, &fDetachShader_, hkDetachShader_);
    LVSGI_HOOK_DYNAMIC(&hLinkProgram, &fLinkProgram_, hkLinkProgram_);
    LVSGI_HOOK_DYNAMIC(&hGetProgramiv, &fGetProgramiv_, hkGetProgramiv_);
    LVSGI_HOOK_DYNAMIC(&hDeleteProgram, &fDeleteProgram_, hkDeleteProgram_);
    LVSGI_HOOK_DYNAMIC(&hUseProgram, &fUseProgram_, hkUseProgram_);
    LVSGI_HOOK_DYNAMIC(&hSwap, &fSwap_, hkSwap_);
#undef LVSGI_HOOK_DYNAMIC

    if (!ok) {
        LVSGI_E("DYNAMIC RENDERDRAGON HOOK SET INCOMPLETE; no partial hook set will be kept");
        resetGraphicsHooks();
        dynamicResolverReady_.store(false, std::memory_order_release);
        return false;
    }

    initFunctions();
    graphicsInstalled_.store(true, std::memory_order_release);
    LVSGI_I("GRAPHICS ACTIVE: version-independent libminecraftpe PLT hooks installed; buildId=%s",
            minecraftBuildId_.empty() ? "<missing>" : minecraftBuildId_.c_str());
    return true;
}

void GlHooks::bootstrapLoop() {
    const auto started = Clock::now();
    bool warned = false;
    while (!bootstrapStop_.load(std::memory_order_acquire)) {
        const auto module = findMinecraftModule();
        if (!module.found) {
            if (!warned && elapsedMs(started) >= cfg_.minecraftHookWarningMs) {
                warned = true;
                LVSGI_W("still waiting for libminecraftpe.so; hooks are not active yet");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        minecraftModuleSeen_.store(true, std::memory_order_release);
        LVSGI_I("MINECRAFT MODULE FOUND: base=%p path=%s buildId=%s",
                reinterpret_cast<void*>(module.base), module.path.c_str(),
                module.buildId.empty() ? "<missing>" : module.buildId.c_str());

        minecraftBuildId_ = module.buildId;
        LVSGI_I("BUILD ID OBSERVED (diagnostic only): %s",
                module.buildId.empty() ? "<missing>" : module.buildId.c_str());

        // No version gate: resolve imported GLES/EGL symbols from this loaded ELF's
        // own DT_JMPREL/dynsym metadata, then locate the AArch64 PLT stubs by decoding
        // their GOT target. Build ID is telemetry/cache salt only.
        for (int attempt = 1;
             attempt <= 20 && !bootstrapStop_.load(std::memory_order_acquire);
             ++attempt) {
            if (installMinecraftHooks(module.base)) return;
            LVSGI_W("direct RenderDragon hook attempt %d/20 failed; retrying", attempt);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        LVSGI_E("unable to install direct RenderDragon hooks after retries");
        return;
    }
}

bool GlHooks::arm(const Config& c, const std::filesystem::path& dataDir,
                  const std::filesystem::path& prewarmFile) {
    uninstall();
    cfg_ = c;
    dataDir_ = dataDir;
    prewarmFile_ = prewarmFile;
    gActive.store(this, std::memory_order_release);

    if (cfg_.shaderCache) {
        cache_.start(dataDir_ / "shader-binaries",
                     static_cast<std::size_t>(cfg_.maxCacheMiB) * 1024u * 1024u,
                     static_cast<std::size_t>(cfg_.maxCacheEntries));
    }
    if (cfg_.prewarmItemPipelines) prewarmer_.start(prewarmFile_);

    bootstrapStop_.store(false, std::memory_order_release);
    bootstrapThread_ = std::thread([this] { bootstrapLoop(); });
    LVSGI_I("MOD ENABLED -> waiting for libminecraftpe.so -> dynamic ELF/PLT discovery -> Patch API validation -> HookHandle");
    return true;
}

void GlHooks::uninstall() {
    bootstrapStop_.store(true, std::memory_order_release);
    if (bootstrapThread_.joinable()) bootstrapThread_.join();

    {
        std::lock_guard lock(hookMu_);
        resetGraphicsHooks();
    }

    prewarmer_.stop();
    cache_.stop();
    voxels_.shutdown();
    {
        std::lock_guard lock(mu_);
        shaders_.clear();
        programs_.clear();
        captures_.clear();
    }

    minecraftModuleSeen_.store(false, std::memory_order_release);
    dynamicResolverReady_.store(false, std::memory_order_release);
    minecraftBuildId_.clear();
    hookCalls_.store(0, std::memory_order_release);
    shaderSourceHits_.store(0, std::memory_order_release);
    patchedShaderHits_.store(0, std::memory_order_release);
    patchedProgramLinks_.store(0, std::memory_order_release);
    patchedProgramUses_.store(0, std::memory_order_release);
    swapHits_.store(0, std::memory_order_release);
    cacheHits_.store(0, std::memory_order_release);
    cacheMisses_.store(0, std::memory_order_release);
    capsInit_ = false;
    binarySupport_ = false;
    parallelCompileSupport_ = false;
    voxelInitTried_ = false;
    nextVoxelRetryFrame_ = 0;
    maxSsboBindings_ = 8;
    GlHooks* expected = this;
    gActive.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
}

void GlHooks::initFunctions() {
    auto extension = [this](const char* name) -> void* {
        __eglMustCastToProperFunctionPointerType fp = nullptr;
        if (fGetProcAddress_) fp = fGetProcAddress_(name);
        if (!fp) fp = eglGetProcAddress(name);
        return reinterpret_cast<void*>(fp);
    };

    fProgramBinary_ = reinterpret_cast<ProgramBinaryFn>(extension("glProgramBinary"));
    if (!fProgramBinary_)
        fProgramBinary_ = reinterpret_cast<ProgramBinaryFn>(extension("glProgramBinaryOES"));
    fGetProgramBinary_ = reinterpret_cast<GetProgramBinaryFn>(extension("glGetProgramBinary"));
    if (!fGetProgramBinary_)
        fGetProgramBinary_ = reinterpret_cast<GetProgramBinaryFn>(extension("glGetProgramBinaryOES"));
    fProgramParameteri_ =
        reinterpret_cast<ProgramParameteriFn>(extension("glProgramParameteri"));
    fMaxCompilerThreads_ = reinterpret_cast<MaxCompilerThreadsFn>(
        extension("glMaxShaderCompilerThreadsKHR"));
}

void GlHooks::initCapabilities() {
    if (eglGetCurrentContext() == EGL_NO_CONTEXT) return;
    if (!capsInit_) {
        capsInit_ = true;
        initFunctions();
        GLint formats = 0;
        glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &formats);
        binarySupport_ = formats > 0 && fProgramBinary_ && fGetProgramBinary_;
        const std::string extensions = safeString(GL_EXTENSIONS);
        parallelCompileSupport_ =
            fMaxCompilerThreads_ &&
            extensions.find("GL_KHR_parallel_shader_compile") != std::string::npos;
        glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxSsboBindings_);
        if (maxSsboBindings_ < 1) maxSsboBindings_ = 1;
        if (parallelCompileSupport_ && cfg_.parallelCompile)
            fMaxCompilerThreads_(static_cast<GLuint>(std::max(1, cfg_.compilerThreads)));
        LVSGI_I("GLES CAPABILITIES: %s | %s | binary=%d parallel=%d ssbo=%d",
                safeString(GL_VENDOR), safeString(GL_RENDERER),
                binarySupport_ ? 1 : 0, parallelCompileSupport_ ? 1 : 0,
                maxSsboBindings_);
    }
}

std::string GlHooks::makeProgramKey(GLuint, ProgramState& p) {
    Sha256 h;
    const char prefix[] = "LVSGI-PROGRAM-CACHE-v4";
    h.update(prefix, sizeof(prefix) - 1);
    h.update(minecraftBuildId_);
    const char* strings[] = {safeString(GL_VENDOR), safeString(GL_RENDERER),
                             safeString(GL_VERSION),
                             safeString(GL_SHADING_LANGUAGE_VERSION)};
    for (auto* s : strings) {
        h.update(s, std::strlen(s));
        char z = 0;
        h.update(&z, 1);
    }
    for (GLuint sid : p.shaders) {
        auto it = shaders_.find(sid);
        if (it == shaders_.end()) continue;
        h.update(&it->second.type, sizeof(it->second.type));
        h.update(it->second.source);
    }
    for (const auto& a : p.attribBindings) {
        h.update(&a.first, sizeof(a.first));
        h.update(a.second);
    }
    h.update(&p.tfMode, sizeof(p.tfMode));
    for (const auto& v : p.tfVaryings) h.update(v);
    return Sha256::hex(h.finish());
}

bool GlHooks::compileShaderNow(GLuint id, ShaderState& s) {
    if (s.compiled) return true;
    auto t = Clock::now();
    fCompileShader_(id);
    s.compiled = true;
    GLint ok = 0;
    fGetShaderiv_(id, GL_COMPILE_STATUS, &ok);
    if (cfg_.logStalls && elapsedMs(t) >= cfg_.stallLogMs)
        LVSGI_W("STALL shader compile %.2fms kind=%d", elapsedMs(t),
                static_cast<int>(s.kind));
    if (ok) return true;

    auto logCompileFailure = [&](const char* phase) {
        if (!fGetShaderInfoLog_) return;
        std::array<GLchar, 2048> log{};
        GLsizei written = 0;
        fGetShaderInfoLog_(id, static_cast<GLsizei>(log.size() - 1), &written,
                           log.data());
        const std::size_t used = written > 0
                                     ? std::min<std::size_t>(static_cast<std::size_t>(written), log.size() - 1)
                                     : std::strlen(log.data());
        log[used] = '\0';
        LVSGI_W("shader compile failure (%s) kind=%d: %s", phase,
                static_cast<int>(s.kind), log.data());
    };
    logCompileFailure("patched/current");

    if (s.source != s.original) {
        const GLchar* src = s.original.c_str();
        GLint len = static_cast<GLint>(s.original.size());
        fShaderSource_(id, 1, &src, &len);
        s.source = s.original;
        s.kind = PatchKind::None;
        s.binding = -1;
        s.compiled = false;
        auto fallbackTime = Clock::now();
        fCompileShader_(id);
        s.compiled = true;
        fGetShaderiv_(id, GL_COMPILE_STATUS, &ok);
        if (!ok) logCompileFailure("original fallback");
        LVSGI_W("patched shader rejected; original fallback %s %.2fms",
                ok ? "worked" : "failed", elapsedMs(fallbackTime));
    }
    return ok == GL_TRUE;
}

void GlHooks::linkProgram(GLuint program) {
    initCapabilities();
    ProgramState local;
    {
        std::lock_guard lock(mu_);
        auto& p = programs_[program];
        p.kind = PatchKind::None;
        p.binding = -1;
        for (GLuint sid : p.shaders) {
            auto it = shaders_.find(sid);
            if (it != shaders_.end() && it->second.kind != PatchKind::None) {
                p.kind = it->second.kind;
                p.binding = it->second.binding;
            }
        }
        p.key = makeProgramKey(program, p);
        local = p;
    }

    if (cfg_.shaderCache && binarySupport_) {
        auto binary = cache_.find(local.key);
        if (binary) {
            cacheHits_.fetch_add(1, std::memory_order_relaxed);
            auto t = Clock::now();
            fProgramBinary_(program, static_cast<GLenum>(binary->format),
                            binary->bytes.data(),
                            static_cast<GLsizei>(binary->bytes.size()));
            GLint ok = 0;
            fGetProgramiv_(program, GL_LINK_STATUS, &ok);
            if (cfg_.logStalls && elapsedMs(t) >= cfg_.stallLogMs)
                LVSGI_W("STALL glProgramBinary %.2fms", elapsedMs(t));
            if (ok) {
                std::lock_guard lock(mu_);
                auto& p = programs_[program];
                p.binaryLoaded = true;
                p.linked = true;
                return;
            }
        } else {
            cacheMisses_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    {
        std::lock_guard lock(mu_);
        auto& p = programs_[program];
        p.kind = PatchKind::None;
        p.binding = -1;
        for (GLuint sid : p.shaders) {
            auto it = shaders_.find(sid);
            if (it != shaders_.end()) compileShaderNow(sid, it->second);
            if (it != shaders_.end() && it->second.kind != PatchKind::None) {
                p.kind = it->second.kind;
                p.binding = it->second.binding;
            }
        }
        p.key = makeProgramKey(program, p);
    }

    if (fProgramParameteri_ && binarySupport_)
        fProgramParameteri_(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

    auto t = Clock::now();
    fLinkProgram_(program);
    if (cfg_.logStalls && elapsedMs(t) >= cfg_.stallLogMs)
        LVSGI_W("STALL program link %.2fms", elapsedMs(t));
}

void GlHooks::useProgram(GLuint program) {
    fUseProgram_(program);
    if (!program) return;
    initCapabilities();
    std::lock_guard lock(mu_);
    auto it = programs_.find(program);
    if (it == programs_.end()) return;
    auto& p = it->second;
    if (p.binding < 0 || !voxels_.buffer()) return;

    const auto uses = patchedProgramUses_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (uses == 1)
        LVSGI_I("PATCHED PROGRAM USED WITH VOXEL BUFFER: program=%u kind=%d binding=%d",
                program, static_cast<int>(p.kind), p.binding);
    voxels_.bindForProgram(p.binding);
    if (p.uDims < 0) {
        p.uDims = glGetUniformLocation(program, "uLeviVoxelDims");
        p.uParity = glGetUniformLocation(program, "uLeviFrameParity");
        p.uGi = glGetUniformLocation(program, "uLeviGiStrength");
        p.uRefStrength = glGetUniformLocation(program, "uLeviReflectionStrength");
        p.uRefRange = glGetUniformLocation(program, "uLeviReflectionRange");
    }
    if (p.uDims >= 0) glUniform4i(p.uDims, cfg_.voxelX, cfg_.voxelY, cfg_.voxelZ, 0);
    if (p.uParity >= 0) glUniform1i(p.uParity, voxels_.parity());
    if (p.uGi >= 0)
        glUniform1f(p.uGi,
                    cfg_.directionalFloodfillGi ? cfg_.giStrength : 0.0f);
    if (p.uRefStrength >= 0)
        glUniform1f(p.uRefStrength,
                    cfg_.hierarchicalDdaReflections ? cfg_.reflectionStrength
                                                    : 0.0f);
    if (p.uRefRange >= 0) glUniform1f(p.uRefRange, cfg_.reflectionRange);
}

void GlHooks::fixScreenEdgeRow() {
    if (!cfg_.fixScreenEdgeRow) return;
    // Kept only as an opt-in fallback. On tile-based GPUs this default-FBO blit
    // may force a resolve, so beta.8 disables it by default. A future version
    // should patch the final RenderDragon presentation shader instead.
    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[2] < 2 || vp[3] < 3) return;
    GLint oldRead = 0, oldDraw = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDraw);
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    if (scissor) glDisable(GL_SCISSOR_TEST);
    const GLint x0 = vp[0], x1 = vp[0] + vp[2], yTop = vp[1] + vp[3];
    glBlitFramebuffer(x0, yTop - 2, x1, yTop - 1, x0, yTop - 1, x1, yTop,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(oldRead));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(oldDraw));
}

void GlHooks::captureOne() {
    if (!cfg_.shaderCache || !binarySupport_ ||
        voxels_.frame() %
            static_cast<std::uint64_t>(
                std::max(1, cfg_.binaryCaptureIntervalFrames)))
        return;

    PendingCapture item;
    bool have = false;
    {
        std::lock_guard lock(mu_);
        auto it = std::find_if(captures_.begin(), captures_.end(), [&](auto& c) {
            return c.readyFrame <= voxels_.frame();
        });
        if (it != captures_.end()) {
            item = *it;
            captures_.erase(it);
            have = true;
        }
    }
    if (!have) return;

    GLint linked = 0;
    fGetProgramiv_(item.program, GL_LINK_STATUS, &linked);
    if (!linked) return;
    GLint len = 0;
    fGetProgramiv_(item.program, GL_PROGRAM_BINARY_LENGTH, &len);
    if (len <= 0 || len > 64 * 1024 * 1024) return;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(len));
    GLsizei written = 0;
    GLenum format = 0;
    auto t = Clock::now();
    fGetProgramBinary_(item.program, len, &written, &format, bytes.data());
    if (written <= 0) return;
    bytes.resize(static_cast<std::size_t>(written));
    cache_.storeAsync(item.key, static_cast<std::uint32_t>(format),
                      std::move(bytes));
    if (cfg_.logStalls && elapsedMs(t) >= cfg_.stallLogMs)
        LVSGI_W("STALL delayed binary capture %.2fms", elapsedMs(t));
}

__eglMustCastToProperFunctionPointerType GlHooks::hGetProcAddress(const char* name) {
    auto* s = active();
    if (!s || !s->fGetProcAddress_) return nullptr;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    auto real = s->fGetProcAddress_(name);
    if (!name) return real;
    if (!s->graphicsInstalled_.load(std::memory_order_acquire)) return real;

    auto set = [&](auto& slot) {
        if (!slot && real)
            slot = reinterpret_cast<std::decay_t<decltype(slot)>>(real);
    };
#define LV_ROUTE(N, SLOT, DET)                                                \
    if (std::strcmp(name, N) == 0) {                                         \
        set(s->SLOT);                                                        \
        return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(   \
            &DET);                                                           \
    }
    // These routes cover dynamically requested calls. Direct Minecraft imports
    // are independently intercepted at their libminecraftpe PLT stubs. The
    // original slots already point at HookHandle trampolines, so returning our
    // detour here does not cause the original call to recurse.
    LV_ROUTE("glCreateShader", fCreateShader_, hCreateShader)
    LV_ROUTE("glShaderSource", fShaderSource_, hShaderSource)
    LV_ROUTE("glCompileShader", fCompileShader_, hCompileShader)
    LV_ROUTE("glGetShaderiv", fGetShaderiv_, hGetShaderiv)
    LV_ROUTE("glGetShaderInfoLog", fGetShaderInfoLog_, hGetShaderInfoLog)
    LV_ROUTE("glDeleteShader", fDeleteShader_, hDeleteShader)
    LV_ROUTE("glCreateProgram", fCreateProgram_, hCreateProgram)
    LV_ROUTE("glAttachShader", fAttachShader_, hAttachShader)
    LV_ROUTE("glDetachShader", fDetachShader_, hDetachShader)
    LV_ROUTE("glBindAttribLocation", fBindAttribLocation_, hBindAttribLocation)
    LV_ROUTE("glTransformFeedbackVaryings", fTransformFeedbackVaryings_,
             hTransformFeedbackVaryings)
    LV_ROUTE("glLinkProgram", fLinkProgram_, hLinkProgram)
    LV_ROUTE("glGetProgramiv", fGetProgramiv_, hGetProgramiv)
    LV_ROUTE("glDeleteProgram", fDeleteProgram_, hDeleteProgram)
    LV_ROUTE("glUseProgram", fUseProgram_, hUseProgram)
#undef LV_ROUTE
    return real;
}

GLuint GlHooks::hCreateShader(GLenum type) {
    auto* s = active();
    if (!s || !s->fCreateShader_) return 0;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (gInside || !s->graphicsInstalled_.load(std::memory_order_acquire))
        return s->fCreateShader_(type);
    Guard guard;
    GLuint id = s->fCreateShader_(type);
    std::lock_guard lock(s->mu_);
    s->shaders_[id].type = type;
    return id;
}

void GlHooks::hShaderSource(GLuint id, GLsizei n, const GLchar* const* strings,
                            const GLint* lengths) {
    auto* s = active();
    if (!s || !s->fShaderSource_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (gInside || !s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fShaderSource_(id, n, strings, lengths);
        return;
    }
    Guard guard;
    s->initCapabilities();
    std::string original = shaderText(n, strings, lengths);
    const auto sourceHash = ShaderPatcher::fnv1a64(original);
    const auto hit = s->shaderSourceHits_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (hit == 1)
        LVSGI_I("HOOK CALLED -> SHADER/RENDER PATH INTERCEPTED: first glShaderSource");

    PatchResult patch{original, PatchKind::None, -1, false};
    {
        std::lock_guard lock(s->mu_);
        auto& state = s->shaders_[id];
        patch = s->patcher_.patch(state.type, original, s->maxSsboBindings_);
        if (patch.kind == PatchKind::DisableNativeSsr && !s->cfg_.disableNativeSsr)
            patch = {original, PatchKind::None, -1, false};
        if (patch.kind == PatchKind::ForwardWorldCapture && !s->cfg_.voxelCapture)
            patch = {original, PatchKind::None, -1, false};
        if (patch.kind == PatchKind::DeferredLighting && !s->cfg_.voxelCapture &&
            !s->cfg_.directionalFloodfillGi &&
            !s->cfg_.hierarchicalDdaReflections)
            patch = {original, PatchKind::None, -1, false};

        state.original = original;
        state.source = patch.source;
        state.kind = patch.kind;
        state.binding = patch.ssboBinding;
        state.compiled = false;
        state.compileRequested = false;
        if (patch.kind != PatchKind::None) {
            const auto patched =
                s->patchedShaderHits_.fetch_add(1, std::memory_order_relaxed) + 1;
            if (patched <= 12)
                LVSGI_I("PATCH APPLIED: kind=%d binding=%d sourceHash=%016llx total=%llu",
                        static_cast<int>(patch.kind), patch.ssboBinding,
                        static_cast<unsigned long long>(sourceHash),
                        static_cast<unsigned long long>(patched));
        }
    }
    const GLchar* src = patch.source.c_str();
    GLint len = static_cast<GLint>(patch.source.size());
    s->fShaderSource_(id, 1, &src, &len);
}

void GlHooks::hCompileShader(GLuint id) {
    auto* s = active();
    if (!s || !s->fCompileShader_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (gInside || !s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fCompileShader_(id);
        return;
    }
    Guard guard;
    s->initCapabilities();
    std::lock_guard lock(s->mu_);
    auto it = s->shaders_.find(id);
    if (it == s->shaders_.end()) {
        s->fCompileShader_(id);
        return;
    }
    it->second.compileRequested = true;
    // Compile on the exact RenderDragon worker/context that requested it.
    // Do not fake GL_COMPILE_STATUS and move compilation to glLinkProgram.
    s->compileShaderNow(id, it->second);
}

void GlHooks::hGetShaderiv(GLuint id, GLenum pname, GLint* out) {
    auto* s = active();
    if (!s || !s->fGetShaderiv_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    // Never lie about shader compilation state. RenderDragon uses multiple
    // rendering worker contexts and requires the real driver status.
    s->fGetShaderiv_(id, pname, out);
}

void GlHooks::hGetShaderInfoLog(GLuint id, GLsizei max, GLsizei* length,
                                GLchar* log) {
    auto* s = active();
    if (!s || !s->fGetShaderInfoLog_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    s->fGetShaderInfoLog_(id, max, length, log);
}

void GlHooks::hDeleteShader(GLuint id) {
    auto* s = active();
    if (!s || !s->fDeleteShader_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fDeleteShader_(id);
        return;
    }
    if (!gInside) {
        std::lock_guard lock(s->mu_);
        s->shaders_.erase(id);
    }
    s->fDeleteShader_(id);
}

GLuint GlHooks::hCreateProgram() {
    auto* s = active();
    if (!s || !s->fCreateProgram_) return 0;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire))
        return s->fCreateProgram_();
    if (gInside) return s->fCreateProgram_();
    Guard guard;
    GLuint id = s->fCreateProgram_();
    std::lock_guard lock(s->mu_);
    s->programs_[id];
    return id;
}

void GlHooks::hAttachShader(GLuint program, GLuint shader) {
    auto* s = active();
    if (!s || !s->fAttachShader_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fAttachShader_(program, shader);
        return;
    }
    s->fAttachShader_(program, shader);
    if (!gInside) {
        std::lock_guard lock(s->mu_);
        auto& v = s->programs_[program].shaders;
        if (std::find(v.begin(), v.end(), shader) == v.end()) v.push_back(shader);
    }
}

void GlHooks::hDetachShader(GLuint program, GLuint shader) {
    auto* s = active();
    if (!s || !s->fDetachShader_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fDetachShader_(program, shader);
        return;
    }
    s->fDetachShader_(program, shader);
    if (!gInside) {
        std::lock_guard lock(s->mu_);
        auto& v = s->programs_[program].shaders;
        v.erase(std::remove(v.begin(), v.end(), shader), v.end());
    }
}

void GlHooks::hBindAttribLocation(GLuint program, GLuint index,
                                  const GLchar* name) {
    auto* s = active();
    if (!s || !s->fBindAttribLocation_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fBindAttribLocation_(program, index, name);
        return;
    }
    s->fBindAttribLocation_(program, index, name);
    if (!gInside) {
        std::lock_guard lock(s->mu_);
        auto& v = s->programs_[program].attribBindings;
        auto it = std::find_if(v.begin(), v.end(),
                               [&](const auto& x) { return x.first == index; });
        std::string value = name ? name : "";
        if (it == v.end())
            v.emplace_back(index, std::move(value));
        else
            it->second = std::move(value);
    }
}

void GlHooks::hTransformFeedbackVaryings(GLuint program, GLsizei count,
                                         const GLchar* const* names,
                                         GLenum mode) {
    auto* s = active();
    if (!s || !s->fTransformFeedbackVaryings_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fTransformFeedbackVaryings_(program, count, names, mode);
        return;
    }
    s->fTransformFeedbackVaryings_(program, count, names, mode);
    if (!gInside) {
        std::lock_guard lock(s->mu_);
        auto& p = s->programs_[program];
        p.tfMode = mode;
        p.tfVaryings.clear();
        for (GLsizei i = 0; i < count; ++i)
            p.tfVaryings.emplace_back(names && names[i] ? names[i] : "");
    }
}

void GlHooks::hLinkProgram(GLuint program) {
    auto* s = active();
    if (!s || !s->fLinkProgram_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fLinkProgram_(program);
        return;
    }
    if (gInside) {
        s->fLinkProgram_(program);
        return;
    }
    Guard guard;
    s->linkProgram(program);
}

void GlHooks::hGetProgramiv(GLuint program, GLenum pname, GLint* out) {
    auto* s = active();
    if (!s || !s->fGetProgramiv_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fGetProgramiv_(program, pname, out);
        return;
    }
    s->fGetProgramiv_(program, pname, out);
    if (!gInside && out && pname == GL_LINK_STATUS && *out == GL_TRUE) {
        std::lock_guard lock(s->mu_);
        auto it = s->programs_.find(program);
        if (it != s->programs_.end()) {
            auto& p = it->second;
            const bool firstSuccessfulLink = !p.linked;
            p.linked = true;
            if (firstSuccessfulLink && p.kind != PatchKind::None) {
                const auto links = s->patchedProgramLinks_.fetch_add(1, std::memory_order_relaxed) + 1;
                if (links <= 12)
                    LVSGI_I("PATCHED PROGRAM LINKED: program=%u kind=%d binding=%d total=%llu",
                            program, static_cast<int>(p.kind), p.binding,
                            static_cast<unsigned long long>(links));
            }
            if (s->cfg_.shaderCache && !p.binaryLoaded && !p.queuedCapture &&
                !p.key.empty()) {
                p.queuedCapture = true;
                s->captures_.push_back(
                    {program, p.key,
                     s->voxels_.frame() + static_cast<std::uint64_t>(
                                                 std::max(0, s->cfg_.binaryCaptureDelayFrames))});
            }
        }
    }
}

void GlHooks::hDeleteProgram(GLuint program) {
    auto* s = active();
    if (!s || !s->fDeleteProgram_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fDeleteProgram_(program);
        return;
    }
    if (!gInside) {
        std::lock_guard lock(s->mu_);
        s->programs_.erase(program);
        s->captures_.erase(
            std::remove_if(s->captures_.begin(), s->captures_.end(),
                           [&](auto& c) { return c.program == program; }),
            s->captures_.end());
    }
    s->fDeleteProgram_(program);
}

void GlHooks::hUseProgram(GLuint program) {
    auto* s = active();
    if (!s || !s->fUseProgram_) return;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire)) {
        s->fUseProgram_(program);
        return;
    }
    if (gInside) {
        s->fUseProgram_(program);
        return;
    }
    Guard guard;
    s->useProgram(program);
}

EGLBoolean GlHooks::hSwap(EGLDisplay display, EGLSurface surface) {
    auto* s = active();
    if (!s || !s->fSwap_) return EGL_FALSE;
    s->hookCalls_.fetch_add(1, std::memory_order_relaxed);
    if (!s->graphicsInstalled_.load(std::memory_order_acquire))
        return s->fSwap_(display, surface);
    if (gInside) return s->fSwap_(display, surface);

    // beta.8.7 surface-lifecycle safety:
    // Never issue LVSGI GLES work for a stale/tearing-down EGL surface. Android
    // can destroy/recreate the SurfaceView while RenderDragon still reaches a
    // final eglSwapBuffers call. Running SSBO allocation, compute dispatch,
    // glGet* or binary capture after that point can prolong/perturb teardown and
    // produce BufferQueue "has been abandoned" noise (or worse on some drivers).
    // Only treat the call as a Minecraft frame boundary when it is the current
    // draw surface of the current display and a real context is current.
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE ||
        eglGetCurrentContext() == EGL_NO_CONTEXT ||
        eglGetCurrentDisplay() != display ||
        eglGetCurrentSurface(EGL_DRAW) != surface) {
        return s->fSwap_(display, surface);
    }

    Guard guard;
    s->swapHits_.fetch_add(1, std::memory_order_relaxed);
    s->initCapabilities();

    // Do all mod-owned GL work while the surface/context is known to be valid,
    // before handing presentation to EGL. If presentation fails, absolutely no
    // LVSGI GL work is executed afterwards.
    const bool needsVoxel = s->cfg_.voxelCapture || s->cfg_.directionalFloodfillGi ||
                            s->cfg_.hierarchicalDdaReflections;
    if (needsVoxel && !s->voxels_.buffer() &&
        (!s->voxelInitTried_ ||
         s->voxels_.frame() >= s->nextVoxelRetryFrame_)) {
        s->voxelInitTried_ = true;
        s->nextVoxelRetryFrame_ = s->voxels_.frame() + 120;
        if (s->voxels_.ensure(s->cfg_))
            LVSGI_I("VOXEL RUNTIME READY");
        else
            LVSGI_W("voxel runtime init failed; will retry at a later frame");
    }

    s->voxels_.endFrame(s->cfg_);
    if ((s->voxels_.frame() % 300u) == 0u) {
        VoxelDiagnostics d{};
        const bool haveDiag = s->voxels_.readDiagnostics(d);
        if (haveDiag) {
            LVSGI_I(
                "HEARTBEAT module=%d resolver=%d hooks=%d calls=%llu frames=%llu shaderSources=%llu patched=%llu linked=%llu used=%llu voxel=%d giDispatch=%llu cam=%d,%d,%d camClaim=%u camCommit=%u capture=%u source=%u deferred=%u giSample=%u computeSource=%u computeNonzero=%u computeFrame=%u drawSerial=%u cacheHit=%llu cacheMiss=%llu",
                s->minecraftModuleSeen_.load() ? 1 : 0,
                s->dynamicResolverReady_.load() ? 1 : 0,
                s->graphicsInstalled_.load() ? 1 : 0,
                static_cast<unsigned long long>(s->hookCalls_.load()),
                static_cast<unsigned long long>(s->swapHits_.load()),
                static_cast<unsigned long long>(s->shaderSourceHits_.load()),
                static_cast<unsigned long long>(s->patchedShaderHits_.load()),
                static_cast<unsigned long long>(s->patchedProgramLinks_.load()),
                static_cast<unsigned long long>(s->patchedProgramUses_.load()),
                s->voxels_.buffer() ? 1 : 0,
                static_cast<unsigned long long>(s->voxels_.dispatches()),
                d.cameraX, d.cameraY, d.cameraZ,
                d.cameraClaim, d.cameraCommit, d.captureFrame, d.sourceFrame,
                d.deferredFrame, d.giSampleFrame, d.computeSourceFrame,
                d.computeNonzeroFrame, d.computeFrame, d.drawFrameSerial,
                static_cast<unsigned long long>(s->cacheHits_.load()),
                static_cast<unsigned long long>(s->cacheMisses_.load()));
        } else {
            LVSGI_I(
                "HEARTBEAT module=%d resolver=%d hooks=%d calls=%llu frames=%llu shaderSources=%llu patched=%llu linked=%llu used=%llu voxel=%d giDispatch=%llu diag=unavailable cacheHit=%llu cacheMiss=%llu",
                s->minecraftModuleSeen_.load() ? 1 : 0,
                s->dynamicResolverReady_.load() ? 1 : 0,
                s->graphicsInstalled_.load() ? 1 : 0,
                static_cast<unsigned long long>(s->hookCalls_.load()),
                static_cast<unsigned long long>(s->swapHits_.load()),
                static_cast<unsigned long long>(s->shaderSourceHits_.load()),
                static_cast<unsigned long long>(s->patchedShaderHits_.load()),
                static_cast<unsigned long long>(s->patchedProgramLinks_.load()),
                static_cast<unsigned long long>(s->patchedProgramUses_.load()),
                s->voxels_.buffer() ? 1 : 0,
                static_cast<unsigned long long>(s->voxels_.dispatches()),
                static_cast<unsigned long long>(s->cacheHits_.load()),
                static_cast<unsigned long long>(s->cacheMisses_.load()));
        }
    }

    if (s->cfg_.prewarmItemPipelines)
        s->prewarmer_.tick(
            s->voxels_.frame(), s->cfg_.prewarmStartDelayFrames,
            s->cfg_.prewarmProgramsPerFrame, s->cfg_.prewarmMaxPending,
            s->parallelCompileSupport_ && s->cfg_.parallelCompile);
    s->captureOne();
    s->fixScreenEdgeRow();

    const EGLBoolean result = s->fSwap_(display, surface);
    if (result == EGL_FALSE) {
        // Do not call eglGetError here: consuming EGL's thread-local error would
        // change application-visible behavior. The important rule is that no
        // mod GL/EGL work happens after a failed presentation.
        static std::atomic<bool> warnedSwapFailure{false};
        if (!warnedSwapFailure.exchange(true, std::memory_order_relaxed))
            LVSGI_W("eglSwapBuffers returned EGL_FALSE; LVSGI skipped all post-swap graphics work");
    }
    return result;
}

} // namespace lvsgi
