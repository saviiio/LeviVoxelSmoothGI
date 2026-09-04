# Beta 8.7 — EGL/Surface lifecycle safety

This revision keeps beta.8.6 dynamic version-independent PLT discovery, but hardens the `eglSwapBuffers` detour.

Changes:
- LVSGI graphics work only runs when the intercepted display/surface is the current EGL draw surface and a valid context is current.
- Voxel allocation, GI end-of-frame compute, cache capture, prewarm, telemetry-related GL reads, and the optional edge fix run before the real `eglSwapBuffers`.
- No LVSGI GLES/EGL work is executed after the real swap returns.
- A failed swap is passed back unchanged; LVSGI does not call `eglGetError`, so it does not consume/change EGL error state.
- Stale/recreated/tearing-down SurfaceView swap calls are pure pass-through.

Reason: Android may abandon/recreate a SurfaceView while a final presentation call is still in flight. beta.8.6 could execute substantial GL work after `eglSwapBuffers`, including after a failed presentation. This revision removes that unsafe lifecycle window.
