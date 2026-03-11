#!/usr/bin/env bash
# ============================================================
# build.sh — сборка checkin-service
# Использование:
#   ./build.sh              # Release, все зависимости через FetchContent
#   ./build.sh --debug      # Debug-сборка
#   ./build.sh --system     # Использовать системные пакеты вместо FetchContent
#   ./build.sh --clean      # Удалить build/ перед сборкой
# ============================================================
set -euo pipefail

BUILD_TYPE="Release"
USE_SYSTEM_DEPS="OFF"
CLEAN=0
BUILD_DIR="build"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

for arg in "$@"; do
    case "$arg" in
        --debug)  BUILD_TYPE="Debug" ;;
        --system) USE_SYSTEM_DEPS="ON" ;;
        --clean)  CLEAN=1 ;;
        --help)
            echo "Usage: $0 [--debug] [--system] [--clean]"
            exit 0
            ;;
    esac
done

echo "════════════════════════════════════════"
echo "  checkin-service build"
echo "  BUILD_TYPE      = $BUILD_TYPE"
echo "  USE_SYSTEM_DEPS = $USE_SYSTEM_DEPS"
echo "  JOBS            = $JOBS"
echo "════════════════════════════════════════"

# ── Системные зависимости ────────────────────────────────────────────────────
install_deps_ubuntu() {
    echo "[*] Installing system packages..."
    sudo apt-get update -qq
    # Всегда нужны: компилятор, cmake, git, libpq-dev, libssl, zlib
    sudo apt-get install -y --no-install-recommends \
        build-essential cmake git pkg-config \
        libpq-dev \
        libssl-dev \
        zlib1g-dev

    if [[ "$USE_SYSTEM_DEPS" == "ON" ]]; then
        # Дополнительно: системные C++ библиотеки
        sudo apt-get install -y --no-install-recommends \
            libpqxx-dev \
            librdkafka-dev \
            nlohmann-json3-dev \
            libspdlog-dev
    fi
}

install_deps_macos() {
    echo "[*] Installing Homebrew packages..."
    brew install cmake git openssl zlib postgresql@16
    if [[ "$USE_SYSTEM_DEPS" == "ON" ]]; then
        brew install libpqxx librdkafka nlohmann-json spdlog
    fi
}

if command -v apt-get &>/dev/null; then
    install_deps_ubuntu
elif command -v brew &>/dev/null; then
    install_deps_macos
else
    echo "[!] Unknown OS — install cmake, libpq-dev, libssl-dev, zlib1g-dev manually."
fi

# ── CMake configure ──────────────────────────────────────────────────────────
if [[ $CLEAN -eq 1 && -d "$BUILD_DIR" ]]; then
    echo "[*] Cleaning $BUILD_DIR..."
    rm -rf "$BUILD_DIR"
fi

echo "[*] CMake configure..."
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DUSE_SYSTEM_DEPS="$USE_SYSTEM_DEPS" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# ── Build ────────────────────────────────────────────────────────────────────
echo "[*] Building ($JOBS jobs)..."
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo ""
echo "✓ Build complete: $BUILD_DIR/checkin-service"
echo ""
echo "Запуск:"
echo "  export DB_HOST=localhost DB_USER=checkin DB_PASSWORD=checkin"
echo "  export KAFKA_BROKERS=localhost:9092"
echo "  ./$BUILD_DIR/checkin-service"
