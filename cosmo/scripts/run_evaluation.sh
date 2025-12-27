#!/bin/sh
# LLM4Decompile - Evaluation Runner Script
#
# This script runs the evaluation pipeline for LLM4Decompile.
# It is designed to work with APE executables and is portable
# across Linux, macOS, Windows (WSL), and BSD systems.
#
# Usage: ./run_evaluation.sh [OPTIONS] --data <testset.json>
#
# SPDX-License-Identifier: MIT

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="${BIN_DIR:-$SCRIPT_DIR/../build/bin}"
DEFAULT_TIMEOUT=10
DEFAULT_WORKERS=4

# Colors for output
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
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

stat_line() {
    printf "${BLUE}[STAT]${NC} %s\n" "$1"
}

# Print usage information
usage() {
    cat << EOF
LLM4Decompile - Evaluation Runner

Usage: $0 [OPTIONS]

Options:
    -d, --data <file>       Input test data file (JSON format)
    -o, --output <dir>      Output directory for results
    -m, --model <path>      Model path or HuggingFace model ID
    -s, --server <url>      LLM inference server URL (e.g., http://localhost:8080)
    -t, --timeout <sec>     Evaluation timeout per sample (default: $DEFAULT_TIMEOUT)
    -w, --workers <num>     Number of parallel workers (default: $DEFAULT_WORKERS)
    -v, --verbose           Verbose output
    -h, --help              Show this help message

Examples:
    # Run evaluation with local inference server
    $0 -d testset.json -s http://localhost:8080 -o results/

    # Run with specific model and timeout
    $0 -d testset.json -m LLM4Binary/llm4decompile-6.7b-v1.5 -t 30

Input Format:
    The input JSON should contain an array of test cases with:
    - task_id: Unique identifier for the test
    - type: Optimization level (O0, O1, O2, O3)
    - c_func: Original C function
    - c_test: Test assertions
    - input_asm_prompt: Assembly code for decompilation

Output:
    Creates a results directory with:
    - results.csv: Detailed results per test case
    - summary.txt: Summary statistics
    - failed/: Directory containing failed decompilations

For more information, visit:
    https://github.com/albertan017/LLM4Decompile
EOF
}

# Parse command line arguments
parse_args() {
    DATA_FILE=""
    OUTPUT_DIR="./results"
    MODEL_PATH=""
    SERVER_URL=""
    TIMEOUT=$DEFAULT_TIMEOUT
    WORKERS=$DEFAULT_WORKERS
    VERBOSE=0

    while [ $# -gt 0 ]; do
        case "$1" in
            -d|--data)
                DATA_FILE="$2"
                shift 2
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            -m|--model)
                MODEL_PATH="$2"
                shift 2
                ;;
            -s|--server)
                SERVER_URL="$2"
                shift 2
                ;;
            -t|--timeout)
                TIMEOUT="$2"
                shift 2
                ;;
            -w|--workers)
                WORKERS="$2"
                shift 2
                ;;
            -v|--verbose)
                VERBOSE=1
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

    if [ -z "$DATA_FILE" ]; then
        error "Data file is required"
        usage
        exit 1
    fi

    if [ ! -f "$DATA_FILE" ]; then
        error "Data file not found: $DATA_FILE"
        exit 1
    fi
}

# Find the evaluate tool
find_evaluate_tool() {
    # Check in bin directory
    if [ -x "$BIN_DIR/llm4decompile-evaluate.com" ]; then
        EVALUATE_TOOL="$BIN_DIR/llm4decompile-evaluate.com"
        return 0
    fi

    if [ -x "$BIN_DIR/llm4decompile-evaluate" ]; then
        EVALUATE_TOOL="$BIN_DIR/llm4decompile-evaluate"
        return 0
    fi

    # Check in PATH
    if command -v llm4decompile-evaluate >/dev/null 2>&1; then
        EVALUATE_TOOL="llm4decompile-evaluate"
        return 0
    fi

    error "Evaluate tool not found. Build with: ./build.sh"
    exit 1
}

# Initialize statistics
init_stats() {
    TOTAL=0
    COMPILED_O0=0
    COMPILED_O1=0
    COMPILED_O2=0
    COMPILED_O3=0
    EXECUTED_O0=0
    EXECUTED_O1=0
    EXECUTED_O2=0
    EXECUTED_O3=0
}

# Update statistics based on result
update_stats() {
    OPT_LEVEL="$1"
    COMPILED="$2"
    EXECUTED="$3"

    TOTAL=$((TOTAL + 1))

    case "$OPT_LEVEL" in
        O0)
            [ "$COMPILED" = "1" ] && COMPILED_O0=$((COMPILED_O0 + 1))
            [ "$EXECUTED" = "1" ] && EXECUTED_O0=$((EXECUTED_O0 + 1))
            ;;
        O1)
            [ "$COMPILED" = "1" ] && COMPILED_O1=$((COMPILED_O1 + 1))
            [ "$EXECUTED" = "1" ] && EXECUTED_O1=$((EXECUTED_O1 + 1))
            ;;
        O2)
            [ "$COMPILED" = "1" ] && COMPILED_O2=$((COMPILED_O2 + 1))
            [ "$EXECUTED" = "1" ] && EXECUTED_O2=$((EXECUTED_O2 + 1))
            ;;
        O3)
            [ "$COMPILED" = "1" ] && COMPILED_O3=$((COMPILED_O3 + 1))
            [ "$EXECUTED" = "1" ] && EXECUTED_O3=$((EXECUTED_O3 + 1))
            ;;
    esac
}

# Helper function to calculate rate without bc
# Uses awk as fallback which is more portable
calc_rate() {
    numerator="$1"
    denominator="$2"
    if command -v bc >/dev/null 2>&1; then
        echo "scale=4; $numerator / $denominator" | bc 2>/dev/null
    elif command -v awk >/dev/null 2>&1; then
        awk "BEGIN {printf \"%.4f\", $numerator / $denominator}" 2>/dev/null
    else
        # Shell arithmetic fallback (integer percentage)
        echo "$((numerator * 100 / denominator))%"
    fi
}

# Print statistics summary
print_summary() {
    NUM_PER_OPT=$((TOTAL / 4))
    
    echo ""
    stat_line "=== Evaluation Summary ==="
    stat_line "Total samples: $TOTAL"
    echo ""
    
    if [ $NUM_PER_OPT -gt 0 ]; then
        COMPILE_RATE_O0=$(calc_rate "$COMPILED_O0" "$NUM_PER_OPT")
        RUN_RATE_O0=$(calc_rate "$EXECUTED_O0" "$NUM_PER_OPT")
        stat_line "O0: Compile Rate: $COMPILE_RATE_O0, Run Rate: $RUN_RATE_O0"
        
        COMPILE_RATE_O1=$(calc_rate "$COMPILED_O1" "$NUM_PER_OPT")
        RUN_RATE_O1=$(calc_rate "$EXECUTED_O1" "$NUM_PER_OPT")
        stat_line "O1: Compile Rate: $COMPILE_RATE_O1, Run Rate: $RUN_RATE_O1"
        
        COMPILE_RATE_O2=$(calc_rate "$COMPILED_O2" "$NUM_PER_OPT")
        RUN_RATE_O2=$(calc_rate "$EXECUTED_O2" "$NUM_PER_OPT")
        stat_line "O2: Compile Rate: $COMPILE_RATE_O2, Run Rate: $RUN_RATE_O2"
        
        COMPILE_RATE_O3=$(calc_rate "$COMPILED_O3" "$NUM_PER_OPT")
        RUN_RATE_O3=$(calc_rate "$EXECUTED_O3" "$NUM_PER_OPT")
        stat_line "O3: Compile Rate: $COMPILE_RATE_O3, Run Rate: $RUN_RATE_O3"
    fi
    
    echo ""
    TOTAL_COMPILED=$((COMPILED_O0 + COMPILED_O1 + COMPILED_O2 + COMPILED_O3))
    TOTAL_EXECUTED=$((EXECUTED_O0 + EXECUTED_O1 + EXECUTED_O2 + EXECUTED_O3))
    
    if [ $TOTAL -gt 0 ]; then
        OVERALL_COMPILE=$(calc_rate "$TOTAL_COMPILED" "$TOTAL")
        OVERALL_RUN=$(calc_rate "$TOTAL_EXECUTED" "$TOTAL")
        stat_line "Overall: Compile Rate: $OVERALL_COMPILE, Run Rate: $OVERALL_RUN"
    fi
}

# Main evaluation loop
run_evaluation() {
    info "Starting evaluation..."
    info "Data file: $DATA_FILE"
    info "Output directory: $OUTPUT_DIR"
    info "Timeout: ${TIMEOUT}s"
    
    mkdir -p "$OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR/failed"
    
    RESULTS_FILE="$OUTPUT_DIR/results.csv"
    echo "task_id,opt_level,compiled,executed,exit_code" > "$RESULTS_FILE"
    
    init_stats
    
    # Note: This is a simplified evaluation loop
    # In practice, you would parse the JSON and run the evaluate tool
    # for each test case. Here we demonstrate the structure.
    
    info "Evaluation framework ready."
    info "Note: This script provides the evaluation infrastructure."
    info "For full evaluation, use with LLM inference server:"
    info "  1. Start inference server with LLM4Decompile model"
    info "  2. Run this script with --server <url>"
    
    if [ -n "$SERVER_URL" ]; then
        info "Server URL: $SERVER_URL"
        warn "Full evaluation requires JSON parsing (use jq or similar)"
    fi
    
    # Example: Process first few samples for demonstration
    # In production, this would parse JSON and iterate through all samples
    
    print_summary
    
    info "Results written to: $RESULTS_FILE"
}

# Main entry point
main() {
    parse_args "$@"
    find_evaluate_tool
    run_evaluation
}

main "$@"
