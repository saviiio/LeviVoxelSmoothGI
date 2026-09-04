# beta.8.3 runtime tombstone analysis

Observed on LeviLauncher 1.5.15 / Android 16 / arm64:

- crashing thread: `Rendering Pool(`;
- signal: `SIGABRT`, abort message `terminating`;
- the crash stack contained `Failed to compile shader` and live GLSL source text;
- at the same instant the LVSGI bootstrap thread was inside `pl::memory::resolveSignatures` -> `resolveSignature` -> `GlHooks::hookSignature` -> `installMinecraftHooks`.

This exposed two unsafe windows in beta.8.3:

1. Hooks were installed one by one while RenderDragon worker threads were already compiling shaders. A `glShaderSource` detour could become active before the complete shader hook set was ready.
2. `deferShaderCompilation=true` could report a fake successful `GL_COMPILE_STATUS` and move the real `glCompileShader` call to a later `glLinkProgram`, possibly on another RenderDragon worker/context.

beta.8.4 changes:

- Build-ID-locked, byte-validated direct PLT offsets replace repeated full-module signature scans.
- Every installed detour is pass-through until the complete hook set is committed with `graphicsInstalled=true`.
- Deferred compile/fake compile status is disabled and sanitized off.
- Patched shader compilation is attempted on the original worker/context; on failure the compiler log is captured, the original source is restored, and the original shader is recompiled immediately.
- Deferred and SSR rewrites require exact source hashes from the validated current profile.
