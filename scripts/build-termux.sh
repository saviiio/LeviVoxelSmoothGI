#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
termux-setup-storage >/dev/null 2>&1 || true
for tool in git cmake ninja zip file readelf; do
  command -v "$tool" >/dev/null 2>&1 || NEED=1
done
if [ "${NEED:-0}" = 1 ]; then
  pkg install -y git cmake ninja zip file binutils
fi
TOOLCHAIN="$(find "$HOME/Android/Sdk/ndk" "$HOME/android-sdk/ndk" "$PREFIX/share/android-ndk" /sdcard/Download -type f -path '*/build/cmake/android.toolchain.cmake' 2>/dev/null | sort -V | tail -n1 || true)"
[ -n "$TOOLCHAIN" ] || { echo 'ERRO: Android NDK não encontrado.'; exit 1; }
NDK="${TOOLCHAIN%/build/cmake/android.toolchain.cmake}"
NDK_CLANG="$(find "$NDK/toolchains/llvm/prebuilt" -type f -path '*/bin/clang' -print -quit 2>/dev/null || true)"
[ -n "$NDK_CLANG" ] || { echo 'ERRO: clang do NDK não encontrado.'; exit 1; }
echo "NDK: $NDK"
file "$NDK_CLANG" || true
if ! "$NDK_CLANG" --version >/dev/null 2>&1; then
  echo 'ERRO: o clang deste NDK não executa neste aparelho (provavelmente host x86_64 em Termux ARM64).'
  echo 'Use .github/workflows/build-android.yml ou um NDK com ferramentas host AArch64.'
  exit 1
fi
DEPS="$HOME/.cache/levi-build-deps"
mkdir -p "$DEPS"
PRELOADER="${PRELOADER_ANDROID_ROOT:-$DEPS/preloader-android-0.2.2}"
if [ ! -f "$PRELOADER/include/pl/Mod.hpp" ]; then
  FOUND="$(find "$HOME" /sdcard/Download -type f -path '*/preloader-android*/include/pl/Mod.hpp' -print -quit 2>/dev/null || true)"
  if [ -n "$FOUND" ]; then PRELOADER="${FOUND%/include/pl/Mod.hpp}"; else rm -rf "$PRELOADER"; git clone --depth 1 --branch 0.2.2 https://github.com/LiteLDev/preloader-android.git "$PRELOADER"; fi
fi
FMT="${FMT_ROOT:-$DEPS/fmt-11.2.0}"
if [ ! -f "$FMT/include/fmt/format.h" ] || [ ! -f "$FMT/CMakeLists.txt" ]; then
  FOUND="$(find "$HOME" /sdcard/Download -type f -path '*/fmt*/include/fmt/format.h' -print -quit 2>/dev/null || true)"
  if [ -n "$FOUND" ]; then FMT="${FOUND%/include/fmt/format.h}"; else rm -rf "$FMT"; git clone --depth 1 --branch 11.2.0 https://github.com/fmtlib/fmt.git "$FMT"; fi
fi
BUILD="$ROOT/build-android-arm64"
rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_STL=c++_shared \
  -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_BASE_DIR="$DEPS/fetchcontent" \
  -DPRELOADER_ANDROID_ROOT="$PRELOADER" \
  -DFETCHCONTENT_SOURCE_DIR_FMT="$FMT"
cmake --build "$BUILD" --target levi_package -j2
SO="$BUILD/liblevi_voxel_smooth_gi.so"
[ -f "$SO" ] || SO="$(find "$BUILD" -type f -name 'liblevi_voxel_smooth_gi.so' -print -quit)"
[ -n "$SO" ] && [ -f "$SO" ] || { echo 'ERRO: .so não gerado.'; exit 1; }
file "$SO"
readelf -h "$SO" | grep -E 'Class:|Machine:|Type:' || true
readelf -l "$SO" | grep -E 'LOAD|Align' || true
OUT="$(find "$BUILD" -maxdepth 1 -name 'LeviVoxelSmoothGI-*.levipack' -print -quit)"
[ -n "$OUT" ] || { echo 'ERRO: .levipack não gerado.'; exit 1; }
cp -f "$OUT" /sdcard/Download/
echo "Gerado: /sdcard/Download/$(basename "$OUT")"
