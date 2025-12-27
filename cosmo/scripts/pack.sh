#!/bin/sh
# LLM4Decompile - PKZIP Distribution Package Creator
#
# Creates a self-contained APE executable that is also a valid ZIP archive.
# The resulting file can be:
#   1. Executed directly on Linux, macOS, Windows, or BSD
#   2. Extracted with any ZIP tool to access bundled resources
#
# This replaces Docker-based distribution with a single portable file.
#
# Usage: ./pack.sh [OPTIONS]
#
# SPDX-License-Identifier: MIT

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ROOT_DIR="$(dirname "$PROJECT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
DIST_DIR="$PROJECT_DIR/dist"

# Output package name
PACKAGE_NAME="${PACKAGE_NAME:-llm4decompile}"
PACKAGE_VERSION="${PACKAGE_VERSION:-2.0.0}"

# Colors
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    RED='\033[0;31m'
    NC='\033[0m'
else
    GREEN=''
    YELLOW=''
    RED=''
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

# Print usage
usage() {
    cat << EOF
LLM4Decompile - PKZIP Distribution Package Creator

Usage: $0 [OPTIONS]

Options:
    -n, --name <name>       Package name (default: $PACKAGE_NAME)
    -v, --version <ver>     Package version (default: $PACKAGE_VERSION)
    -o, --output <dir>      Output directory (default: $DIST_DIR)
    -m, --minimal           Create minimal package (executables only)
    -f, --full              Create full package (with samples and docs)
    -h, --help              Show this help message

Output:
    Creates <name>-<version>.com - an Actually Portable Executable
    that is also a valid ZIP archive containing:
    
    Minimal package:
    - Main decompilation tools
    
    Full package:
    - Main decompilation tools
    - Sample code and test data
    - Documentation
    - Shell scripts for common tasks

The resulting file can be:
    - Executed directly: ./llm4decompile-2.0.0.com preprocess -i sample.c -o out
    - Extracted: unzip llm4decompile-2.0.0.com
    
Examples:
    $0                      # Create full package
    $0 -m                   # Create minimal package
    $0 -n mydecompiler -v 1.0.0

This replaces Docker with a single portable executable that works on:
    - Linux (x86_64, ARM64)
    - macOS (x86_64, Apple Silicon)
    - Windows (x86_64, via APE loader)
    - FreeBSD, OpenBSD, NetBSD

EOF
}

# Parse arguments
MINIMAL=0
FULL=1
OUTPUT_DIR="$DIST_DIR"

while [ $# -gt 0 ]; do
    case "$1" in
        -n|--name)
            PACKAGE_NAME="$2"
            shift 2
            ;;
        -v|--version)
            PACKAGE_VERSION="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -m|--minimal)
            MINIMAL=1
            FULL=0
            shift
            ;;
        -f|--full)
            FULL=1
            MINIMAL=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Check prerequisites
check_prereqs() {
    if ! command -v zip >/dev/null 2>&1; then
        error "zip command not found. Please install zip."
        exit 1
    fi
    
    if [ ! -f "$BUILD_DIR/bin/llm4decompile.com" ]; then
        error "Executables not found. Build first with: ./build.sh"
        exit 1
    fi
}

# Create the package
create_package() {
    PACKAGE_FILE="${PACKAGE_NAME}-${PACKAGE_VERSION}.com"
    PACKAGE_PATH="$OUTPUT_DIR/$PACKAGE_FILE"
    
    info "Creating distribution package: $PACKAGE_FILE"
    
    mkdir -p "$OUTPUT_DIR"
    
    # Start with the main executable
    cp "$BUILD_DIR/bin/llm4decompile.com" "$PACKAGE_PATH"
    
    # Create a temporary directory for assets to bundle
    TEMP_DIR=$(mktemp -d)
    trap "rm -rf $TEMP_DIR" EXIT
    
    # Create directory structure for bundled content
    mkdir -p "$TEMP_DIR/bin"
    mkdir -p "$TEMP_DIR/scripts"
    
    # Copy additional executables
    if [ -f "$BUILD_DIR/bin/llm4decompile-preprocess.com" ]; then
        cp "$BUILD_DIR/bin/llm4decompile-preprocess.com" "$TEMP_DIR/bin/"
    fi
    if [ -f "$BUILD_DIR/bin/llm4decompile-evaluate.com" ]; then
        cp "$BUILD_DIR/bin/llm4decompile-evaluate.com" "$TEMP_DIR/bin/"
    fi
    
    # Copy shell scripts
    cp "$SCRIPT_DIR"/*.sh "$TEMP_DIR/scripts/" 2>/dev/null || true
    
    if [ $FULL -eq 1 ]; then
        info "Creating full package with samples and documentation..."
        
        # Copy samples
        if [ -d "$ROOT_DIR/samples" ]; then
            mkdir -p "$TEMP_DIR/samples"
            cp "$ROOT_DIR/samples/sample.c" "$TEMP_DIR/samples/" 2>/dev/null || true
        fi
        
        # Copy test data (subset for distribution)
        if [ -d "$ROOT_DIR/legacy-test" ]; then
            mkdir -p "$TEMP_DIR/data"
            # Only include a small subset for distribution
            head -100 "$ROOT_DIR/legacy-test/decompile-eval-executable-gcc-obj.json" \
                > "$TEMP_DIR/data/sample-testset.json" 2>/dev/null || true
        fi
        
        # Copy documentation
        cp "$ROOT_DIR/README.md" "$TEMP_DIR/" 2>/dev/null || true
        cp "$ROOT_DIR/LICENSE" "$TEMP_DIR/" 2>/dev/null || true
        
        # Create quick start guide
        cat > "$TEMP_DIR/QUICKSTART.txt" << 'QUICKSTART_EOF'
LLM4Decompile - Quick Start Guide
==================================

This is an Actually Portable Executable (APE) - it runs on Linux, macOS,
Windows, and BSD systems without modification.

RUNNING THE EXECUTABLE
----------------------
On Unix-like systems:
    chmod +x llm4decompile-*.com
    ./llm4decompile-*.com --help

On Windows:
    .\llm4decompile-*.com --help
    (Or rename to .exe: copy llm4decompile-*.com llm4decompile.exe)

EXTRACTING BUNDLED RESOURCES
-----------------------------
This file is also a valid ZIP archive. Extract with:
    unzip llm4decompile-*.com -d extracted/

BASIC USAGE
-----------
1. Preprocess C code for decompilation:
   ./llm4decompile-*.com preprocess -i sample.c -o output -f func_name

2. The output .asm file contains assembly with prompts for the LLM.

3. Send to LLM4Decompile model for decompilation.

4. Evaluate decompiled code:
   ./llm4decompile-*.com evaluate -i decompiled.c -r tests.c

For more information, see README.md or visit:
https://github.com/albertan017/LLM4Decompile
QUICKSTART_EOF
    else
        info "Creating minimal package..."
        
        cat > "$TEMP_DIR/QUICKSTART.txt" << 'MINIMAL_EOF'
LLM4Decompile - Minimal Package

This is an Actually Portable Executable. Run with:
    ./llm4decompile-*.com --help

For full documentation, visit:
https://github.com/albertan017/LLM4Decompile
MINIMAL_EOF
    fi
    
    # Append content to the APE file using zip
    # APE files support appending ZIP data at the end
    cd "$TEMP_DIR"
    zip -r -q "$PACKAGE_PATH" . || {
        error "Failed to append ZIP data to package"
        exit 1
    }
    cd - > /dev/null
    
    info "Package created: $PACKAGE_PATH"
    info ""
    info "Package contents:"
    unzip -l "$PACKAGE_PATH" 2>/dev/null | head -20 || true
    info ""
    
    # Print file size
    SIZE=$(ls -lh "$PACKAGE_PATH" | awk '{print $5}')
    info "Package size: $SIZE"
    info ""
    info "Usage:"
    info "  Run directly: ./$PACKAGE_FILE --help"
    info "  Extract:      unzip $PACKAGE_FILE -d extracted/"
}

# Main
main() {
    info "LLM4Decompile PKZIP Distribution Builder"
    info "========================================="
    
    check_prereqs
    create_package
    
    info ""
    info "Distribution package ready!"
    info "This single file replaces Docker containers for distribution."
}

main
