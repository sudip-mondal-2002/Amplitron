# setup_dependencies.ps1 - Fetches and sets up external dependencies for Guitar Amp Simulator
# Run this script once before building the project.

$ErrorActionPreference = "Stop"

$PROJECT_ROOT = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$EXTERNAL_DIR = Join-Path $PROJECT_ROOT "external"

Write-Host "=== Guitar Amp Simulator - Dependency Setup ===" -ForegroundColor Cyan
Write-Host "Project root: $PROJECT_ROOT"

# Create external directory
if (-Not (Test-Path $EXTERNAL_DIR)) {
    New-Item -ItemType Directory -Path $EXTERNAL_DIR | Out-Null
}

# --- Dear ImGui ---
$IMGUI_DIR = Join-Path $EXTERNAL_DIR "imgui"
$IMGUI_VERSION = "v1.90.1"

if (-Not (Test-Path $IMGUI_DIR)) {
    Write-Host "`nFetching Dear ImGui $IMGUI_VERSION..." -ForegroundColor Yellow
    git clone --depth 1 --branch $IMGUI_VERSION https://github.com/ocornut/imgui.git $IMGUI_DIR
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Failed to clone ImGui" -ForegroundColor Red
        exit 1
    }
    Write-Host "Dear ImGui fetched successfully." -ForegroundColor Green
} else {
    Write-Host "Dear ImGui already present, skipping." -ForegroundColor Green
}

# --- kiss_fft (BSD-3-Clause) ---
$KISS_FFT_DIR = Join-Path $EXTERNAL_DIR "kiss_fft"

if (-Not (Test-Path (Join-Path $KISS_FFT_DIR "kiss_fft.c"))) {
    Write-Host "`nFetching kiss_fft..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $KISS_FFT_DIR -Force | Out-Null
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft.h" -OutFile (Join-Path $KISS_FFT_DIR "kiss_fft.h")
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft.c" -OutFile (Join-Path $KISS_FFT_DIR "kiss_fft.c")
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mborgerding/kissfft/master/_kiss_fft_guts.h" -OutFile (Join-Path $KISS_FFT_DIR "_kiss_fft_guts.h")
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft_log.h" -OutFile (Join-Path $KISS_FFT_DIR "kiss_fft_log.h")
    Write-Host "kiss_fft fetched successfully." -ForegroundColor Green
} else {
    Write-Host "kiss_fft already present, skipping." -ForegroundColor Green
}

# --- dr_wav (single-header WAV library) ---
$DR_WAV_PATH = Join-Path $EXTERNAL_DIR "dr_wav.h"

if (-Not (Test-Path $DR_WAV_PATH)) {
    Write-Host "`nFetching dr_wav.h..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h" -OutFile $DR_WAV_PATH
    Write-Host "dr_wav.h fetched successfully." -ForegroundColor Green
} else {
    Write-Host "dr_wav.h already present, skipping." -ForegroundColor Green
}

# --- nanosvg (single-header SVG rasterizer) ---
$NANOSVG_PATH = Join-Path $EXTERNAL_DIR "nanosvg.h"
$NANOSVG_RAST_PATH = Join-Path $EXTERNAL_DIR "nanosvgrast.h"

if ((-Not (Test-Path $NANOSVG_PATH)) -or (-Not (Test-Path $NANOSVG_RAST_PATH))) {
    Write-Host "`nFetching nanosvg..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvg.h" -OutFile (Join-Path $EXTERNAL_DIR "nanosvg.h")
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvgrast.h" -OutFile (Join-Path $EXTERNAL_DIR "nanosvgrast.h")
    Write-Host "nanosvg fetched successfully." -ForegroundColor Green
} else {
    Write-Host "nanosvg already present, skipping." -ForegroundColor Green
}

# --- Check for PortAudio ---
Write-Host "`nChecking for PortAudio..." -ForegroundColor Yellow
$pa_found = $false

# Check vcpkg
if (Get-Command vcpkg -ErrorAction SilentlyContinue) {
    Write-Host "vcpkg found. You can install PortAudio with: vcpkg install portaudio" -ForegroundColor Cyan
    $pa_found = $true
}

# Check common paths
$pa_paths = @(
    "C:\Program Files\portaudio",
    "C:\vcpkg\installed\x64-windows",
    "$env:VCPKG_ROOT\installed\x64-windows",
    "C:\msys64\mingw64"
)
foreach ($p in $pa_paths) {
    if (Test-Path (Join-Path $p "include\portaudio.h")) {
        Write-Host "PortAudio found at: $p" -ForegroundColor Green
        $pa_found = $true
        break
    }
}

if (-Not $pa_found) {
    Write-Host @"

PortAudio not found. Install it via one of these methods:
  1. vcpkg:   vcpkg install portaudio:x64-windows
  2. Manual:  Download from http://www.portaudio.com/download.html
              Build and install to C:\Program Files\portaudio
"@ -ForegroundColor Red
}

# --- Check for SDL2 ---
Write-Host "`nChecking for SDL2..." -ForegroundColor Yellow
$sdl_found = $false

$sdl_paths = @(
    "C:\Program Files\SDL2",
    "C:\vcpkg\installed\x64-windows",
    "$env:VCPKG_ROOT\installed\x64-windows",
    "C:\msys64\mingw64"
)
foreach ($p in $sdl_paths) {
    if ((Test-Path (Join-Path $p "include\SDL2\SDL.h")) -or (Test-Path (Join-Path $p "include\SDL.h"))) {
        Write-Host "SDL2 found at: $p" -ForegroundColor Green
        $sdl_found = $true
        break
    }
}

if (-Not $sdl_found) {
    Write-Host @"

SDL2 not found. Install it via one of these methods:
  1. vcpkg:   vcpkg install sdl2:x64-windows
  2. Manual:  Download from https://github.com/libsdl-org/SDL/releases
              Extract to C:\Program Files\SDL2
"@ -ForegroundColor Red
}
# --- RTNeural (real-time neural network inference) ---
$RTNEURAL_DIR = Join-Path $EXTERNAL_DIR "RTNeural"
$RTNEURAL_VERSION = "1fb1f075a5d66e85bfc8f488c3f3626840cb3a1d"

if (-not (Test-Path $RTNEURAL_DIR)) {
    Write-Host "`nFetching RTNeural (pinned to commit $RTNEURAL_VERSION)..." -ForegroundColor Yellow
    git clone https://github.com/jatinchowdhury18/RTNeural.git $RTNEURAL_DIR
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Failed to clone RTNeural" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "RTNeural already present, verifying pinned commit..." -ForegroundColor Green
}
Push-Location $RTNEURAL_DIR
git fetch origin
git checkout $RTNEURAL_VERSION
Pop-Location
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to checkout RTNeural at $RTNEURAL_VERSION" -ForegroundColor Red
    exit 1
}
Write-Host "RTNeural is pinned to $RTNEURAL_VERSION." -ForegroundColor Green

# Inject a shim over RTNeural's bundled nlohmann/json so the project's
# authoritative copy (v3.11.3) is the only version compiled.
$RTNEURAL_JSON_SHIM = Join-Path $RTNEURAL_DIR "modules\json\json.hpp"
New-Item -ItemType Directory -Path (Split-Path $RTNEURAL_JSON_SHIM) -Force | Out-Null
@'
// RTNeural json.hpp shim -- auto-generated by scripts/setup_dependencies.ps1
// Redirects RTNeural's bundled nlohmann/json (v3.11.1) to the project's
// authoritative nlohmann/json (v3.11.3) to avoid dual inline-namespace ABI
// conflicts. Do not edit; re-run setup_dependencies.ps1 to regenerate.
#ifndef INCLUDE_NLOHMANN_JSON_HPP_
#define JSON_SKIP_LIBRARY_VERSION_CHECK
#endif
#include <nlohmann/json.hpp>
'@ | Set-Content -Encoding UTF8 -Path $RTNEURAL_JSON_SHIM
Write-Host "RTNeural json.hpp shim written." -ForegroundColor Green


Write-Host "`n=== Setup Complete ===" -ForegroundColor Cyan
if ($pa_found -and $sdl_found) {
    Write-Host "All dependencies found! You can now build with:" -ForegroundColor Green
    Write-Host "  mkdir build; cd build; cmake ..; cmake --build . --config Release" -ForegroundColor White
} else {
    Write-Host "Some dependencies are missing. Please install them before building." -ForegroundColor Yellow
}
