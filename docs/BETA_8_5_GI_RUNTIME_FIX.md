# Beta 8.5 — GI runtime/data-path fix

This revision was made after five complete readings of each of these public LeviLaunchroid API pages:

- `api/types-and-macros`
- `api/signature`
- `api/patch`
- `api/mod`

The per-pass notes are stored in `LEVI_API_4_PAGES_READ_5X.txt`; the reread summary is in `LEVI_API_4_PAGES_SUMMARY.txt`.

## Problems found after beta.8.4

The mod could compile, load and run without crashing while still producing no visible GI. Two silent blockers were possible:

1. **Full-source hash gating was too strict.** RenderDragon/BGFX may concatenate runtime preambles/defines into `glShaderSource`. The same known DeferredShading family can therefore have a different FNV hash from the raw source extracted from the material binary. Beta.8.4 could intercept the shader but leave it untouched.
2. **Camera publication was fragile.** Earlier code depended on a particular fragment/pixel and later on a per-program frame uniform. A program can remain bound across frame boundaries without another `glUseProgram`, which makes a per-program serial stale. Compute then rejects the camera and propagates around no valid current-frame center.

## Corrections

### Levi API / hook safety

- The exact `libminecraftpe.so` GNU Build ID is still required before version-specific hooks.
- Prevalidated PLT offsets are still used to avoid rescanning the large Minecraft image while RenderDragon workers are active.
- Target bytes are read with the public `pl::memory::readBytes` Patch API and compared with the exact expected PLT bytes before each `HookHandle` is installed.
- Hook handles remain owned for the lifetime of `enable()` and are reset during `disable()`/`unload()`.
- A partially installed hook set is never treated as graphics-active.

### Shader identification

- DeferredShading and RenderChunkForwardPBR no longer depend exclusively on a hash of the complete runtime concatenated string.
- They use strict material-family fingerprints containing the current RenderDragon uniforms/buffers/varyings. This accepts harmless runtime preamble/define changes while rejecting simple/fancy lookalikes.
- Native SSR replacement remains exact-hash-gated because disabling a wrong shader would be destructive and SSR replacement is not required for GI activation.
- `PATCH APPLIED` logs the actual runtime source hash for future profile refinement.

### Camera and frame synchronization

- Header slots 0..2 store signed camera block coordinates with `intBitsToUint`/`uintBitsToInt` so negative world coordinates survive exactly.
- The current raster-frame serial is published by CPU in SSBO header slot 15 at each frame boundary. Fragment shaders read that shared serial directly; it no longer depends on `glUseProgram` being called every frame.
- Camera ownership uses monotonic `atomicMax`; a late/stale draw cannot roll diagnostic/frame stamps backward.
- Camera publication no longer depends on one bottom-left framebuffer pixel.
- Compute refuses stale camera data by requiring the camera commit serial to equal the current compute serial.

### GI data-path telemetry

Every 300 frames the heartbeat can now prove the full path:

- `shaderSources`: intercepted shader sources
- `patched`: sources actually transformed
- `linked`: patched programs observed successfully linked
- `used`: patched programs used while the voxel SSBO exists
- `cam/camClaim/camCommit`: camera position and serial publication
- `capture`: a surface-capture path executed
- `source`: nonzero surface source was written into adjacent air
- `deferred`: Deferred wrapper executed
- `computeSource`: compute saw nonzero input/source
- `computeNonzero`: compute produced nonzero propagated GI
- `computeFrame`: compute ran with a valid current-frame camera
- `giSample`: Deferred sampled nonzero GI

This distinguishes “the mod is loaded” from “GI energy reached the final Deferred shader.”

## Validation

- Host core builds with warnings as errors.
- `ctest`: 1/1 passing.
- Android-facing sources pass C++20 syntax checking with `-Wall -Wextra -Wpedantic -Wmissing-designated-field-initializers -Werror` against local API/GLES stubs.
- Exact Minecraft Build ID/profile validation: 15/15 PLT targets unique and at expected offsets.
- All four extracted current Deferred fragment variants are accepted and transformed by the beta.8.5 patcher.
- Tests cover a runtime-generated Deferred source variation whose full hash is intentionally outside the exact profile while its strict structural fingerprint remains valid.

Actual RenderDragon/GLES execution must still be verified on the Android device. The new heartbeat is designed to identify the first dead stage if GI remains visually absent.
