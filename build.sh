#!/usr/bin/env bash
set -euo pipefail

#######################################
# Configuration
#######################################
BUILD_DIR="build"                 # where CMake will put its files
GENERATOR="Unix Makefiles"        # change to "Ninja" if you prefer

#######################################
# Clean old cache (prevents generator‐mismatch)
#######################################
if [ -d "$BUILD_DIR" ]; then
  echo "🧹  Removing old $BUILD_DIR to avoid generator conflicts…"
  rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

#######################################
# Configure CMake
#######################################
cmake -S . -B "$BUILD_DIR" \
  -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

#######################################
# Determine parallel-build jobs
#######################################
if command -v sysctl &> /dev/null; then
  JOBS=$(sysctl -n hw.logicalcpu)
elif command -v nproc &> /dev/null; then
  JOBS=$(nproc)
else
  JOBS=1
fi

#######################################
# Build
#######################################
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "✅  Build complete → $BUILD_DIR/main"