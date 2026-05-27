#!/bin/bash
# setup_dependencies.sh - Fetches and sets up external dependencies for Guitar Amp Simulator
# Run this script once before building the project.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
EXTERNAL_DIR="$PROJECT_ROOT/external"

echo "=== Guitar Amp Simulator - Dependency Setup ==="
echo "Project root: $PROJECT_ROOT"

mkdir -p "$EXTERNAL_DIR"

# --- Dear ImGui ---
IMGUI_DIR="$EXTERNAL_DIR/imgui"
IMGUI_VERSION="v1.90.1"

if [ ! -d "$IMGUI_DIR" ]; then
    echo ""
    echo "Fetching Dear ImGui $IMGUI_VERSION..."
    git clone --depth 1 --branch "$IMGUI_VERSION" https://github.com/ocornut/imgui.git "$IMGUI_DIR"
    echo "Dear ImGui fetched successfully."
else
    echo "Dear ImGui already present, skipping."
fi

# --- kiss_fft (BSD-3-Clause) ---
KISS_FFT_DIR="$EXTERNAL_DIR/kiss_fft"

if [ ! -f "$KISS_FFT_DIR/kiss_fft.c" ]; then
    echo ""
    echo "Fetching kiss_fft..."
    mkdir -p "$KISS_FFT_DIR"
    curl -fsSL -o "$KISS_FFT_DIR/kiss_fft.h"       https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft.h
    curl -fsSL -o "$KISS_FFT_DIR/kiss_fft.c"       https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft.c
    curl -fsSL -o "$KISS_FFT_DIR/_kiss_fft_guts.h"  https://raw.githubusercontent.com/mborgerding/kissfft/master/_kiss_fft_guts.h
    curl -fsSL -o "$KISS_FFT_DIR/kiss_fft_log.h"    https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft_log.h
    echo "kiss_fft fetched successfully."
else
    echo "kiss_fft already present, skipping."
fi

# --- dr_wav (single-header WAV library) ---
if [ ! -f "$EXTERNAL_DIR/dr_wav.h" ]; then
    echo ""
    echo "Fetching dr_wav.h..."
    curl -fsSL -o "$EXTERNAL_DIR/dr_wav.h" https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h
    echo "dr_wav.h fetched successfully."
else
    echo "dr_wav.h already present, skipping."
fi

# --- nanosvg (single-header SVG rasterizer) ---
if [ ! -f "$EXTERNAL_DIR/nanosvg.h" ] || [ ! -f "$EXTERNAL_DIR/nanosvgrast.h" ]; then
    echo ""
    echo "Fetching nanosvg..."
    curl -fsSL -o "$EXTERNAL_DIR/nanosvg.h"     https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvg.h
    curl -fsSL -o "$EXTERNAL_DIR/nanosvgrast.h" https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvgrast.h
    echo "nanosvg fetched successfully."
else
    echo "nanosvg already present, skipping."
fi

# --- Install system dependencies ---
echo ""
echo "Checking system dependencies..."

install_deps() {
    if command -v apt-get &> /dev/null; then
        echo "Detected Debian/Ubuntu. Installing dependencies..."
        sudo apt-get update
        sudo apt-get install -y \
            build-essential cmake pkg-config \
            libportaudio2 portaudio19-dev \
            libsdl2-dev \
            libgl1-mesa-dev \
            libjack-jackd2-dev
    elif command -v dnf &> /dev/null; then
        echo "Detected Fedora/RHEL. Installing dependencies..."
        sudo dnf install -y \
            gcc-c++ cmake pkg-config \
            portaudio-devel \
            SDL2-devel \
            mesa-libGL-devel \
            jack-audio-connection-kit-devel
    elif command -v pacman &> /dev/null; then
        echo "Detected Arch Linux. Installing dependencies..."
        sudo pacman -S --noconfirm \
            base-devel cmake pkg-config \
            portaudio \
            sdl2 \
            mesa \
            jack2
    elif command -v brew &> /dev/null; then
        echo "Detected macOS with Homebrew. Installing dependencies..."
        brew install cmake portaudio sdl2 jack
    else
        echo "WARNING: Could not detect package manager."
        echo "Please install manually: cmake, portaudio, sdl2, opengl dev headers"
    fi
}

if [ -t 0 ]; then
    read -p "Install system dependencies? [y/N] " -n 1 -r
    echo
else
    REPLY="N"
    echo "Non-interactive shell detected; skipping system dependency install prompt."
fi
if [[ ${REPLY:-N} =~ ^[Yy]$ ]]; then
    install_deps
fi

echo ""
echo "=== Setup Complete ==="
echo "Build with:"
echo "  mkdir -p build && cd build && cmake .. && make -j\$(nproc)"
