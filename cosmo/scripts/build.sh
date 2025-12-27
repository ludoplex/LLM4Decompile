#!/bin/sh
# LLM4Decompile - Build Script for Cosmopolitan APE Binaries
#
# This script downloads the Cosmopolitan toolchain and builds
# the LLM4Decompile tools as Actually Portable Executables.
#
# Usage: ./build.sh [clean|all|pack|install]
#
# SPDX-License-Identifier: MIT

set -e

# Configuration
COSMO_VERSION="3.3.2"
COSMO_URL="https://cosmo.zip/pub/cosmocc/cosmocc-${COSMO_VERSION}.zip"
COSMO_DIR="${COSMO_DIR:-$HOME/.cosmocc}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors for output (if terminal supports it)
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
    NC=''
fi

info() {
    printf "${GREEN}[INFO]${NC} %s\n" "$1"
}

warn() {
    printf "${YELLOW}[WARN]${NC} %s\n" "$1"
}

error() {
    printf "${RED}[ERROR]${NC} %s\n" "$1" >&2
}

# Check for required tools
check_requirements() {
    for cmd in curl unzip make; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            error "Required tool not found: $cmd"
            exit 1
        fi
    done
}

# Download and install Cosmopolitan toolchain
install_cosmocc() {
    if [ -d "$COSMO_DIR" ] && [ -x "$COSMO_DIR/bin/cosmocc" ]; then
        info "Cosmopolitan toolchain already installed at $COSMO_DIR"
        return 0
    fi

    info "Downloading Cosmopolitan toolchain v${COSMO_VERSION}..."
    mkdir -p "$COSMO_DIR"
    
    TEMP_ZIP=$(mktemp)
    curl -L -o "$TEMP_ZIP" "$COSMO_URL" || {
        error "Failed to download Cosmopolitan toolchain"
        rm -f "$TEMP_ZIP"
        exit 1
    }

    info "Extracting toolchain..."
    unzip -q -o "$TEMP_ZIP" -d "$COSMO_DIR" || {
        error "Failed to extract Cosmopolitan toolchain"
        rm -f "$TEMP_ZIP"
        exit 1
    }
    rm -f "$TEMP_ZIP"

    info "Cosmopolitan toolchain installed to $COSMO_DIR"
}

# Build the project
build() {
    install_cosmocc

    info "Building LLM4Decompile with Cosmopolitan..."
    cd "$PROJECT_DIR"
    
    export PATH="$COSMO_DIR/bin:$PATH"
    export COSMOCC="$COSMO_DIR/bin/cosmocc"
    
    make -j"$(nproc 2>/dev/null || echo 4)" "$@" || {
        error "Build failed"
        exit 1
    }

    info "Build complete!"
    info "Executables are in: $PROJECT_DIR/build/bin/"
    ls -la "$PROJECT_DIR/build/bin/"/*.com 2>/dev/null || true
}

# Clean build artifacts
clean() {
    info "Cleaning build artifacts..."
    cd "$PROJECT_DIR"
    make clean
    info "Clean complete"
}

# Create distribution package
pack() {
    build
    
    info "Creating distribution package..."
    cd "$PROJECT_DIR"
    
    export PATH="$COSMO_DIR/bin:$PATH"
    make pack || {
        error "Packaging failed"
        exit 1
    }

    info "Distribution package created!"
    info "The file is both an executable AND a ZIP archive."
    ls -la "$PROJECT_DIR/dist/"
}

# Install to system
install_bins() {
    build
    
    PREFIX="${PREFIX:-/usr/local}"
    info "Installing to $PREFIX..."
    
    cd "$PROJECT_DIR"
    make install PREFIX="$PREFIX" || {
        error "Installation failed"
        exit 1
    }

    info "Installation complete!"
}

# Print usage
usage() {
    cat << EOF
LLM4Decompile - Cosmopolitan Build Script

Usage: $0 [COMMAND]

Commands:
    all         Build all executables (default)
    clean       Remove build artifacts
    pack        Create PKZIP distribution package
    install     Install to system (default: /usr/local)
    toolchain   Install Cosmopolitan toolchain only
    help        Show this help message

Environment Variables:
    COSMO_DIR   Installation directory for Cosmopolitan toolchain
                (default: \$HOME/.cosmocc)
    PREFIX      Installation prefix for 'install' command
                (default: /usr/local)

Examples:
    $0              # Build all executables
    $0 clean        # Clean build artifacts
    $0 pack         # Create distributable package
    $0 install      # Install to /usr/local
    PREFIX=/opt $0 install  # Install to /opt

For more information, visit:
    https://github.com/albertan017/LLM4Decompile
    https://github.com/jart/cosmopolitan
EOF
}

# Main entry point
main() {
    check_requirements

    case "${1:-all}" in
        all|build)
            build
            ;;
        clean)
            clean
            ;;
        pack|package|dist)
            pack
            ;;
        install)
            install_bins
            ;;
        toolchain|setup)
            install_cosmocc
            ;;
        help|-h|--help)
            usage
            ;;
        *)
            error "Unknown command: $1"
            usage
            exit 1
            ;;
    esac
}

main "$@"
