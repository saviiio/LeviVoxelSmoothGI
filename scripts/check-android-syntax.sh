#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX="${CXX:-$(command -v clang++ || command -v g++)}"
for src in src/VoxelRuntime.cpp src/ItemPrewarmer.cpp src/GlHooks.cpp src/Mod.cpp; do
  echo "syntax: $src"
  "$CXX" -std=c++20 -Wall -Wextra -Wpedantic -Wmissing-designated-field-initializers -Werror -fsyntax-only \
    -I"$ROOT/tests/stubs" -I"$ROOT/include" -I"$ROOT/generated" \
    "$ROOT/$src"
done
