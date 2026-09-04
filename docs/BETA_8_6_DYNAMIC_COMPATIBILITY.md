# beta.8.6 — dynamic Minecraft compatibility

## Goal
Remove the runtime dependency on one hard-coded Minecraft GNU Build ID and fixed PLT offsets while keeping hook installation fail-closed.

## Runtime resolver
1. Wait for `libminecraftpe.so` through `dl_iterate_phdr`.
2. Read the GNU Build ID only as telemetry/cache salt.
3. Parse the loaded module's `PT_DYNAMIC`.
4. Read `DT_JMPREL`, `DT_PLTRELSZ`, `DT_PLTREL`, `DT_SYMTAB`, and `DT_STRTAB`.
5. Find the GOT relocation slot for each required EGL/GLES import.
6. Scan executable `PT_LOAD` ranges for the canonical AArch64 PLT sequence `ADRP x16; LDR x17,[x16,#imm]; ADD x16,x16,#imm; BR x17`.
7. Decode the ADRP/immediates and require the stub to address the exact GOT slot found from the symbol relocation.
8. Resolve all 15 targets before installing any hook.
9. Re-read the final 16-byte stub with `pl::memory::readBytes()` and validate it immediately before constructing `HookHandle`.
10. If any required target is missing or invalid, install no partial hook set.

## Shader compatibility
DeferredShading and RenderChunkForwardPBR are selected by strict structural fingerprints, so a small source preamble/define change does not disable GI. Historical full-source hashes remain regression evidence. Native SSR suppression remains exact-profile-only because replacing a false-positive SSR shader is destructive and SSR suppression is not required for GI.

## Cache
The observed runtime Build ID is included in the program-binary cache key when present, preventing binary reuse across incompatible game binaries without blocking unknown versions.
