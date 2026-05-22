# Makefile wrapper for Guitar Amp Simulator
# This wraps CMake for convenience. Use `make help` to see available targets.

BUILD_DIR := build
BUILD_TYPE ?= Release
CMAKE_FLAGS ?=
NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

.PHONY: all build clean rebuild install uninstall setup run help

all: build

setup:
	@echo "=== Setting up dependencies ==="
	@chmod +x scripts/setup_dependencies.sh 2>/dev/null || true
	@scripts/setup_dependencies.sh

$(BUILD_DIR)/Makefile:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_FLAGS) ..

build: $(BUILD_DIR)/Makefile
	@echo "=== Building Guitar Amp Simulator ($(BUILD_TYPE)) ==="
	@cmake --build $(BUILD_DIR) --config $(BUILD_TYPE) -j$(NPROC)
	@echo "=== Build complete ==="
	@echo "Binary: $(BUILD_DIR)/amplitron"

clean:
	@echo "=== Cleaning build directory ==="
	@rm -rf $(BUILD_DIR)

rebuild: clean build

install: build
	@echo "=== Installing ==="
	@cmake --install $(BUILD_DIR) --prefix /usr/local

uninstall:
	@echo "=== Uninstalling ==="
	@rm -f /usr/local/bin/amplitron
	@rm -rf /usr/local/share/amplitron
	@rm -rf /usr/local/bin/assets

run: build
	@echo "=== Running Guitar Amp Simulator ==="
	@./$(BUILD_DIR)/amplitron

debug:
	@$(MAKE) BUILD_TYPE=Debug build

help:
	@echo "Guitar Amp Simulator - Build Targets"
	@echo "======================================"
	@echo "  make setup     - Install dependencies (Linux/macOS)"
	@echo "  make build     - Build the project (Release)"
	@echo "  make debug     - Build the project (Debug)"
	@echo "  make clean     - Remove build directory"
	@echo "  make rebuild   - Clean + build"
	@echo "  make install   - Install to /usr/local/bin"
	@echo "  make uninstall - Remove installed binary"
	@echo "  make run       - Build and run"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_TYPE=Debug|Release  (default: Release)"
	@echo "  CMAKE_FLAGS=...           (extra CMake flags)"
