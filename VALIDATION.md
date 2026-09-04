# Validation — LeviVoxelSmoothGI 1.0.0-beta.8.7

## Local checks completed

- Host CMake/Ninja build: PASS.
- Host core tests (`ctest`): 1/1 PASS.
- Android-facing C++20 syntax check with warnings-as-errors: PASS.
- `libminecraftpe.so` GNU Build ID: exact match `868e275cb295e9a275bb29d2258edc2f7dc48761`.
- Direct-hook profile: 15/15 PLT signatures unique and profile offsets correct.
- Four extracted current Deferred fragment shaders: 4/4 transformed as `DeferredLighting`, binding 15 in the 16-binding validation configuration.
- Runtime-source-variation unit test: PASS; a harmless preamble/define hash change does not suppress the strict Deferred family match.
- Unsafe shader compilation deferral stays force-disabled.
- Config schema/sanitizer version: 9.

## Runtime instrumentation added

The heartbeat now reports shader interception, patching, successful patched-program linking/usage, camera serial publication, surface capture/source seeding, compute input/output energy, valid-camera compute execution and nonzero Deferred GI sampling.

## Device validation still required

This container cannot execute Minecraft Bedrock, RenderDragon or the Android GLES driver. The GitHub Android build and the real-device heartbeat remain the final runtime proof.

- beta.8.7: runtime Build ID gate removed; ELF relocation/PLT decoder passes C++20 syntax validation.
- All 15 historical PLT signatures decode as the generic ADRP/LDR/ADD/BR form used by the new resolver.
- Build ID is now diagnostic/cache salt only.
