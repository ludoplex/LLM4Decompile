# LLM4Decompile - Cosmopolitan C Build System
# Uses jart/cosmopolitan to create Actually Portable Executables (APE)
#
# Prerequisites:
#   - Download cosmocc from https://cosmo.zip/pub/cosmocc/
#   - Set COSMOCC_DIR to the cosmocc directory, or place cosmocc in PATH
#
# Usage:
#   make all        - Build all executables
#   make clean      - Remove build artifacts
#   make pack       - Create PKZIP distributable package
#   make install    - Install to PREFIX (default: /usr/local)

# Cosmopolitan toolchain configuration
COSMOCC ?= cosmocc
COSMOCXX ?= cosmoc++
COSMO_AR ?= cosmoar

# Build directories
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
DIST_DIR := dist

# Source directories
SRC_DIR := src
INC_DIR := include

# Compiler flags for Cosmopolitan
CFLAGS := -O2 -Wall -Wextra -I$(INC_DIR)
LDFLAGS := -static

# Installation prefix
PREFIX ?= /usr/local

# Source files
PREPROCESS_SRCS := $(SRC_DIR)/preprocess.c $(SRC_DIR)/common.c
EVALUATE_SRCS := $(SRC_DIR)/evaluate.c $(SRC_DIR)/common.c
DECOMPILE_SRCS := $(SRC_DIR)/decompile.c $(SRC_DIR)/common.c

# Object files
PREPROCESS_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/preprocess_%.o,$(PREPROCESS_SRCS))
EVALUATE_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/evaluate_%.o,$(EVALUATE_SRCS))
DECOMPILE_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/decompile_%.o,$(DECOMPILE_SRCS))

# Output executables (APE format)
PREPROCESS_BIN := $(BIN_DIR)/llm4decompile-preprocess.com
EVALUATE_BIN := $(BIN_DIR)/llm4decompile-evaluate.com
DECOMPILE_BIN := $(BIN_DIR)/llm4decompile.com

# All binaries
BINS := $(PREPROCESS_BIN) $(EVALUATE_BIN) $(DECOMPILE_BIN)

.PHONY: all clean pack install uninstall help

all: $(BINS)
	@echo "Build complete. APE executables are in $(BIN_DIR)/"
	@echo "These executables run on Linux, macOS, Windows, FreeBSD, OpenBSD, and NetBSD."

# Create build directories
$(OBJ_DIR) $(BIN_DIR) $(DIST_DIR):
	mkdir -p $@

# Compile source files to object files
$(OBJ_DIR)/preprocess_%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(COSMOCC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/evaluate_%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(COSMOCC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/decompile_%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(COSMOCC) $(CFLAGS) -c $< -o $@

# Link executables
$(PREPROCESS_BIN): $(PREPROCESS_OBJS) | $(BIN_DIR)
	$(COSMOCC) $(LDFLAGS) $^ -o $@
	@echo "Built: $@ (preprocess binary for assembly extraction)"

$(EVALUATE_BIN): $(EVALUATE_OBJS) | $(BIN_DIR)
	$(COSMOCC) $(LDFLAGS) $^ -o $@
	@echo "Built: $@ (evaluation runner for decompiled code)"

$(DECOMPILE_BIN): $(DECOMPILE_OBJS) | $(BIN_DIR)
	$(COSMOCC) $(LDFLAGS) $^ -o $@
	@echo "Built: $@ (main decompilation orchestrator)"

# Create PKZIP distributable package
# APE executables are valid ZIP files, so we can append data to them
pack: all | $(DIST_DIR)
	@echo "Creating PKZIP distribution package..."
	cp $(DECOMPILE_BIN) $(DIST_DIR)/llm4decompile.com
	cd .. && zip -r cosmo/$(DIST_DIR)/llm4decompile.com \
		samples/sample.c \
		legacy-test/decompile-eval-executable-gcc-obj.json \
		README.md \
		LICENSE
	@echo "Distribution package created: $(DIST_DIR)/llm4decompile.com"
	@echo "This file is both an executable AND a ZIP archive."
	@echo "Run it directly, or unzip to extract bundled resources."

# Install executables
install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(PREPROCESS_BIN) $(DESTDIR)$(PREFIX)/bin/llm4decompile-preprocess
	install -m 755 $(EVALUATE_BIN) $(DESTDIR)$(PREFIX)/bin/llm4decompile-evaluate
	install -m 755 $(DECOMPILE_BIN) $(DESTDIR)$(PREFIX)/bin/llm4decompile
	@echo "Installed to $(DESTDIR)$(PREFIX)/bin/"

# Uninstall executables
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/llm4decompile-preprocess
	rm -f $(DESTDIR)$(PREFIX)/bin/llm4decompile-evaluate
	rm -f $(DESTDIR)$(PREFIX)/bin/llm4decompile
	@echo "Uninstalled from $(DESTDIR)$(PREFIX)/bin/"

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)
	@echo "Cleaned build artifacts"

# Help target
help:
	@echo "LLM4Decompile - Cosmopolitan C Build System"
	@echo ""
	@echo "This build system creates Actually Portable Executables (APE) using"
	@echo "jart/cosmopolitan. The resulting binaries run natively on multiple"
	@echo "operating systems without modification."
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build all executables (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  pack      - Create PKZIP distributable package"
	@echo "  install   - Install to PREFIX (default: /usr/local)"
	@echo "  uninstall - Remove installed files"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  COSMOCC   - Path to cosmocc compiler (default: cosmocc)"
	@echo "  PREFIX    - Installation prefix (default: /usr/local)"
	@echo "  DESTDIR   - Staging directory for packaging"
	@echo ""
	@echo "Prerequisites:"
	@echo "  Download cosmocc from https://cosmo.zip/pub/cosmocc/"
	@echo "  Extract and add to PATH, or set COSMOCC variable"
