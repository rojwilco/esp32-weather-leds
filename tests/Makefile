# Convenience wrapper around CMake.
# Usage:
#   make          — configure (if needed) + build + run all tests
#   make build    — configure + build only
#   make test     — run tests (must build first)
#   make clean    — remove build directory

BUILD_DIR := build

.PHONY: all build test clean

all: build test

build: $(BUILD_DIR)/Makefile
	cmake --build $(BUILD_DIR) -j$$(nproc)

$(BUILD_DIR)/Makefile:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

test:
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)
