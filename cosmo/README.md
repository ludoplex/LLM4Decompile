# LLM4Decompile - Cosmopolitan C Implementation

This directory contains a portable C implementation of LLM4Decompile using
[jart/cosmopolitan](https://github.com/jart/cosmopolitan) to create Actually
Portable Executables (APE) that run on multiple operating systems.

## Overview

The Cosmopolitan implementation replaces the Python scripts, Bash scripts, and
Dockerfile with:

- **C source code** compiled with Cosmopolitan Libc for cross-platform portability
- **APE shell scripts** that work across Unix-like systems
- **PKZIP packaging** - the executables are also valid ZIP archives

### Why Cosmopolitan?

Traditional approaches require:
- Python interpreter + dependencies
- Docker containers
- Platform-specific builds

With Cosmopolitan APE:
- Single executable runs on Linux, macOS, Windows, and BSD
- No runtime dependencies
- No containers needed
- File can be both executed AND extracted as a ZIP archive

## Directory Structure

```
cosmo/
├── Makefile              # Build system
├── README.md             # This file
├── include/
│   └── common.h          # Shared definitions
├── src/
│   ├── common.c          # Shared utilities
│   ├── preprocess.c      # Binary preprocessing tool
│   ├── evaluate.c        # Evaluation runner
│   └── decompile.c       # Main CLI
├── scripts/
│   ├── build.sh          # Build script (downloads cosmocc)
│   ├── run_evaluation.sh # Evaluation runner script
│   └── pack.sh           # PKZIP distribution creator
├── build/                # Build output (created by make)
└── dist/                 # Distribution packages (created by pack.sh)
```

## Quick Start

### Building

```bash
# Build all executables
./scripts/build.sh

# Or manually with make (if cosmocc is in PATH)
make all
```

The build script will:
1. Download the Cosmopolitan toolchain (if not present)
2. Compile the C source files
3. Create APE executables in `build/bin/`

### Running

The executables work on any supported platform:

```bash
# Linux/macOS/BSD
chmod +x build/bin/llm4decompile.com
./build/bin/llm4decompile.com --help

# Windows (PowerShell)
.\build\bin\llm4decompile.com --help

# Or rename to .exe on Windows
copy build\bin\llm4decompile.com llm4decompile.exe
.\llm4decompile.exe --help
```

### Usage

#### Preprocess C code for decompilation

```bash
# Single optimization level
./llm4decompile.com preprocess -i sample.c -o output -f func0 -O 0

# All optimization levels (O0-O3)
./llm4decompile.com preprocess -i sample.c -o output -f func0 -a
```

This creates `.asm` files containing assembly with LLM prompts.

#### Evaluate decompiled code

```bash
./llm4decompile.com evaluate -i decompiled.c -r tests.c -o results.csv
```

#### Get information

```bash
./llm4decompile.com info
./llm4decompile.com version
```

## Creating Distribution Package

Create a self-contained distributable:

```bash
./scripts/pack.sh
```

This creates a single file that is BOTH:
1. An executable that runs on Linux, macOS, Windows, and BSD
2. A ZIP archive containing samples, documentation, and tools

```bash
# Run directly
./dist/llm4decompile-2.0.0.com preprocess -i sample.c -o out -f func0

# Or extract contents
unzip dist/llm4decompile-2.0.0.com -d extracted/
```

## Supported Platforms

The APE executables run natively on:

| Platform | Architecture | Notes |
|----------|-------------|-------|
| Linux | x86_64, ARM64 | Full support |
| macOS | x86_64, ARM64 | Full support |
| Windows | x86_64 | Via APE loader |
| FreeBSD | x86_64 | Full support |
| OpenBSD | x86_64 | Full support |
| NetBSD | x86_64 | Full support |

## Comparison with Docker

| Aspect | Docker | Cosmopolitan APE |
|--------|--------|------------------|
| File size | ~2GB+ | ~1MB |
| Dependencies | Docker runtime | None |
| Startup time | Seconds | Instant |
| Portability | Linux (native), others via VM | Native everywhere |
| Distribution | Registry or tar | Single file |
| Offline use | Requires pull | Works offline |

## Tools

### llm4decompile.com

Main CLI with subcommands:
- `preprocess` - Compile and extract assembly
- `evaluate` - Test decompiled code
- `info` - Show build information

### llm4decompile-preprocess.com

Standalone preprocessing tool:
```bash
llm4decompile-preprocess.com -i input.c -o output -f func_name [-a]
```

### llm4decompile-evaluate.com

Standalone evaluation tool:
```bash
llm4decompile-evaluate.com -d decompiled.c -t tests.c [-o results.csv]
```

## Building from Source

### Prerequisites

- curl (for downloading cosmocc)
- unzip
- make

### Manual Build

```bash
# Download Cosmopolitan toolchain
curl -LO https://cosmo.zip/pub/cosmocc/cosmocc-3.3.2.zip
unzip cosmocc-3.3.2.zip -d ~/.cosmocc

# Add to PATH
export PATH="$HOME/.cosmocc/bin:$PATH"

# Build
cd cosmo
make all
```

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `COSMOCC` | Path to cosmocc compiler | `cosmocc` |
| `COSMO_DIR` | Cosmopolitan installation directory | `$HOME/.cosmocc` |
| `PREFIX` | Installation prefix | `/usr/local` |

## Integration with LLM4Decompile Models

This C implementation handles preprocessing and evaluation. For actual
decompilation, you need an LLM inference server running an LLM4Decompile model.

Recommended approaches:

1. **llama.cpp** - Also uses Cosmopolitan, perfect companion
   ```bash
   ./llama-server -m llm4decompile-6.7b.gguf --port 8080
   ```

2. **vLLM** - High-performance Python server
   ```bash
   vllm serve LLM4Binary/llm4decompile-6.7b-v1.5
   ```

3. **HuggingFace Transformers** - Direct Python usage

## License

MIT License - See LICENSE file in the repository root.

## References

- [Cosmopolitan Libc](https://github.com/jart/cosmopolitan)
- [Actually Portable Executable](https://justine.lol/ape.html)
- [LLM4Decompile Paper](https://arxiv.org/abs/2403.05286)
- [LLM4Decompile Models](https://huggingface.co/LLM4Binary)
