#!/usr/bin/env bash
# Configure debug/build dirs for the bootloader.
#   debug -> bin/debug/<hash>  (DEBUG_PRINT_KEYS=ON)
#   build -> bin/build/<hash>  (keys never printed)

set -euo pipefail

TOOLCHAIN="cmake/gcc-arm-none-eabi.cmake"

usage() {
    cat <<EOF
Usage: $0 <command> [hash]

Commands:
  debug [sha|blake2]   Configure bin/debug/<hash> with DEBUG_PRINT_KEYS=ON
  build [sha|blake2]   Configure bin/build/<hash> (no key printing)
  all                  Configure debug and build for both hashes
  --help, -h           Show this help

If [hash] is omitted, both sha and blake2 are configured.

Examples:
  $0 debug sha         # bin/debug/sha, SHA-256, keys printed
  $0 build blake2      # bin/build/blake2, BLAKE2s, no keys
  $0 all               # all four directories

After configuring, compile with:
  cmake --build bin/<debug|build>/<sha|blake2>
EOF
}

# configure <debug|build> <sha|blake2>
configure() {
    local mode="$1" hash="$2"
    local dir="bin/${mode}/${hash}"
    local args=(-S . -B "$dir" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN")

    [[ "$hash" == "blake2" ]] && args+=(-DUSE_BLAKE2S=ON)
    if [[ "$mode" == "debug" ]]; then
        args+=(-DDEBUG_PRINT_KEYS=ON)
    else
        args+=(-DDEBUG_PRINT_KEYS=OFF)
    fi

    echo ">> configuring $dir"
    cmake "${args[@]}"
}

# run configure for one hash, or both if none given
configure_hashes() {
    local mode="$1" hash="${2:-}"
    case "$hash" in
        "")     configure "$mode" sha; configure "$mode" blake2 ;;
        sha|blake2) configure "$mode" "$hash" ;;
        *)      echo "Unknown hash '$hash' (use sha or blake2)"; exit 1 ;;
    esac
}

if [[ $# -lt 1 ]]; then
    echo "Type $0 --help for specific commands"
    exit 1
fi

case "$1" in
    --help|-h) usage ;;
    all)       configure_hashes debug; configure_hashes build ;;
    debug)     configure_hashes debug "${2:-}" ;;
    build)     configure_hashes build "${2:-}" ;;
    *)         echo "Unknown command '$1'"; usage; exit 1 ;;
esac