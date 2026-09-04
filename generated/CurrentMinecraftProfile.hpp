#pragma once
#include <array>
#include <cstdint>
namespace lvsgi::profile {
inline constexpr char kMinecraftBuildId[] = "868e275cb295e9a275bb29d2258edc2f7dc48761";
// Exact virtual offsets for the validated PLT stubs in the Build ID above.
// The first executable PT_LOAD has p_vaddr == p_offset == 0, so these are
// directly added to dlpi_addr after the Build ID is verified.
inline constexpr std::uintptr_t kOffGlCreateShader = 0x11b90a10u;
inline constexpr std::uintptr_t kOffGlShaderSource = 0x11b90830u;
inline constexpr std::uintptr_t kOffGlCompileShader = 0x11b90910u;
inline constexpr std::uintptr_t kOffGlGetShaderiv = 0x11b90760u;
inline constexpr std::uintptr_t kOffGlGetShaderInfoLog = 0x11b90ba0u;
inline constexpr std::uintptr_t kOffGlDeleteShader = 0x11b90a90u;
inline constexpr std::uintptr_t kOffGlCreateProgram = 0x11b908b0u;
inline constexpr std::uintptr_t kOffGlAttachShader = 0x11b907f0u;
inline constexpr std::uintptr_t kOffGlDetachShader = 0x11b909a0u;
inline constexpr std::uintptr_t kOffGlLinkProgram = 0x11b90720u;
inline constexpr std::uintptr_t kOffGlGetProgramiv = 0x11b909d0u;
inline constexpr std::uintptr_t kOffGlDeleteProgram = 0x11b907d0u;
inline constexpr std::uintptr_t kOffGlUseProgram = 0x11b90af0u;
inline constexpr std::uintptr_t kOffEglGetProcAddress = 0x11c0dc70u;
inline constexpr std::uintptr_t kOffEglSwapBuffers = 0x11c0dca0u;

// Unique AArch64 PLT signatures extracted from the exact libminecraftpe.so above.
// Hooking these stubs keeps the detours inside Minecraft's own module and avoids
// Android linker-namespace ambiguity from hooking a separately resolved GLES/EGL export.
inline constexpr char kSigGlCreateShader[] = "D0 49 00 B0 11 EE 42 F9 10 62 17 91 20 02 1F D6";
inline constexpr char kSigGlShaderSource[] = "D0 49 00 B0 11 76 42 F9 10 A2 13 91 20 02 1F D6";
inline constexpr char kSigGlCompileShader[] = "D0 49 00 B0 11 AE 42 F9 10 62 15 91 20 02 1F D6";
inline constexpr char kSigGlGetShaderiv[] = "D0 49 00 B0 11 42 42 F9 10 02 12 91 20 02 1F D6";
inline constexpr char kSigGlGetShaderInfoLog[] = "D0 49 00 B0 11 52 43 F9 10 82 1A 91 20 02 1F D6";
inline constexpr char kSigGlDeleteShader[] = "D0 49 00 B0 11 0E 43 F9 10 62 18 91 20 02 1F D6";
inline constexpr char kSigGlCreateProgram[] = "D0 49 00 B0 11 96 42 F9 10 A2 14 91 20 02 1F D6";
inline constexpr char kSigGlAttachShader[] = "D0 49 00 B0 11 66 42 F9 10 22 13 91 20 02 1F D6";
inline constexpr char kSigGlDetachShader[] = "D0 49 00 B0 11 D2 42 F9 10 82 16 91 20 02 1F D6";
inline constexpr char kSigGlLinkProgram[] = "D0 49 00 B0 11 32 42 F9 10 82 11 91 20 02 1F D6";
inline constexpr char kSigGlGetProgramiv[] = "D0 49 00 B0 11 DE 42 F9 10 E2 16 91 20 02 1F D6";
inline constexpr char kSigGlDeleteProgram[] = "D0 49 00 B0 11 5E 42 F9 10 E2 12 91 20 02 1F D6";
inline constexpr char kSigGlUseProgram[] = "D0 49 00 B0 11 26 43 F9 10 22 19 91 20 02 1F D6";
inline constexpr char kSigEglGetProcAddress[] = "D0 47 00 D0 11 86 47 F9 10 22 3C 91 20 02 1F D6";
inline constexpr char kSigEglSwapBuffers[] = "D0 47 00 D0 11 92 47 F9 10 82 3C 91 20 02 1F D6";
inline constexpr std::array<std::uint64_t, 5> kDeferredHashes = {
    0x4ab431fd167d7937ULL,
    0x4771e2f070fe6e40ULL,
    0xe24ff768a2a0fdd2ULL,
    0x0d7cce40dc0ce700ULL,
    0xbbcddede67ea4013ULL,
};
inline constexpr std::array<std::uint64_t, 12> kRenderChunkForwardHashes = {
    0xde72d085b8674199ULL,
    0xc1c0995b2e93fcc3ULL,
    0x6a803e00867b6238ULL,
    0xe2e7887ad38f6a4eULL,
    0x4beb7785da774f95ULL,
    0x73f2dfb0de5114b7ULL,
    0x5602349011f136ebULL,
    0x98919e9682b6aa57ULL,
    0xa33bf6c0be8aca2cULL,
    0x7de867a2aed0abbaULL,
    0x5ab86121063f5b8dULL,
    0xfbd8edd5b5e4e17fULL,
};
inline constexpr std::array<std::uint64_t, 5> kSsrHashes = {
    0x65a4331fc62cd9a3ULL,
    0xf9117b6a2ea5d9deULL,
    0xc603f3c19fc51800ULL,
    0xe33aa5e5e89c20fbULL,
    0x39b4ee9cdefda187ULL,
};
inline constexpr std::array<std::uint64_t, 78> kItemHashes = {
    0x3caf2ac2c99cdcb0ULL,
    0xf2b77e333c7de79aULL,
    0x188729cd98770712ULL,
    0xbbcddede67ea4013ULL,
    0x296e362f664c9cf8ULL,
    0xdcdefa518d37ec4cULL,
    0xce6f6c73519accafULL,
    0xb593e0cf34fecc6bULL,
    0x4302690774297461ULL,
    0x5e526708cdd4f624ULL,
    0x16b210e0bdc0cd44ULL,
    0x397dc305343598b7ULL,
    0xdaa66d9b533cb039ULL,
    0xe3a342228743bfebULL,
    0x54fd3dc1a944cfd2ULL,
    0x7dc52b1e7ba42ca5ULL,
    0x9fe39d5ac31e3d04ULL,
    0xbb136763860e6f48ULL,
    0x02bc1502046b3ed4ULL,
    0xab3a9a24502ec67bULL,
    0x95088514253f67f9ULL,
    0x5dbc455939cf6e8aULL,
    0xd3225c7bf07bc240ULL,
    0x077db2702d79ea06ULL,
    0x2074eeab8cd51b85ULL,
    0x449e98cefbc60d27ULL,
    0x1fdf6aed793f0fbeULL,
    0xeab69da8c68650d8ULL,
    0x0737dc772cf18b97ULL,
    0x78c7c56c9c0b3719ULL,
    0x5fc0e173c53e97dbULL,
    0x80f518e052445373ULL,
    0x424c9e51cc2a699eULL,
    0x724f3a5cd64ab49cULL,
    0x990ad2f9ff253860ULL,
    0x6f1fcb01e034aedaULL,
    0xd9f2b4252df247dbULL,
    0xc40d46b5ce91df85ULL,
    0xd1feeba8855125fdULL,
    0x6c3331cfb8f8913aULL,
    0x88f2eeb71ad41b34ULL,
    0xa3c62b9027087517ULL,
    0x10acf5766b1170c8ULL,
    0x3fcae7a5da1bf555ULL,
    0x08a415f76ffbfbfaULL,
    0x508d24e94b73d763ULL,
    0x9cd8e05a336995d4ULL,
    0xc7d0201a936098daULL,
    0x7c42605012e2c37bULL,
    0x49d672489ef18f63ULL,
    0x1a4c002373cbed76ULL,
    0x4d5e2e69ad3532d6ULL,
    0x36d6789bf1d2352dULL,
    0xb864ce81dfe4fd8fULL,
    0xf2a64dd6a145419bULL,
    0x5127560537fae120ULL,
    0xa7c355be1d0e8207ULL,
    0x5f604a95e77496b3ULL,
    0xe58115447bd7d700ULL,
    0x6d8a7ec0cbe3941fULL,
    0xd672e460bb271486ULL,
    0x078e9ecc8188e184ULL,
    0xe44f0c0eca28fc8fULL,
    0x61844531c0c97e1dULL,
    0xfe4aff785d2f3837ULL,
    0xf71eb791c296dbe2ULL,
    0x0d27caddcd0109c3ULL,
    0x5b6ce60384f8137fULL,
    0x00781d18f3dbab80ULL,
    0xe3bdc2f9f4d438aaULL,
    0x95c3aeaebeadc628ULL,
    0x3598c4f896d4c8d3ULL,
    0xba0ec84854e77309ULL,
    0xa0d814b76b633a74ULL,
    0x9d987873c4c90653ULL,
    0xa7b200d09213b5d9ULL,
    0x758cda561caba117ULL,
    0x9c8d5cfdc71b7418ULL,
};
} // namespace lvsgi::profile
