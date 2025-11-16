#!/bin/bash
# Script to run all fuzz targets for a specified duration

set -e

# Default duration in seconds (5 minutes)
DURATION=${1:-300}

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Find the build directory
BUILD_DIR="${PROJECT_ROOT}/out/build/Clang/Debug"
if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found: $BUILD_DIR"
    echo "Please build the project with fuzzing enabled:"
    echo "  cmake --preset linux-clang-debug -DENABLE_FUZZING=ON"
    echo "  cmake --build --preset linux-clang-debug"
    exit 1
fi

# Check if fuzz binaries exist
FUZZ_TARGETS=(
    "fuzz_analog_csv"
    "fuzz_point_csv"
    "fuzz_point_json"
)

BIN_DIR="${BUILD_DIR}/bin"
if [ ! -d "$BIN_DIR" ]; then
    echo "Binary directory not found: $BIN_DIR"
    exit 1
fi

echo "=========================================="
echo "Running fuzz tests for ${DURATION} seconds"
echo "=========================================="
echo ""

# Run each fuzz target
for target in "${FUZZ_TARGETS[@]}"; do
    TARGET_PATH="${BIN_DIR}/${target}"
    
    if [ ! -f "$TARGET_PATH" ]; then
        echo "Warning: Fuzz target not found: $target"
        continue
    fi
    
    # Determine corpus directory
    CORPUS_NAME="${target#fuzz_}"  # Remove 'fuzz_' prefix
    CORPUS_DIR="${PROJECT_ROOT}/tests/fuzz/corpus/${CORPUS_NAME}"
    
    echo "Running: $target"
    echo "Corpus: $CORPUS_DIR"
    echo "Duration: ${DURATION}s"
    echo ""
    
    # Create corpus directory if it doesn't exist
    mkdir -p "$CORPUS_DIR"
    
    # Run the fuzz target with timeout
    if [ -d "$CORPUS_DIR" ] && [ "$(ls -A "$CORPUS_DIR")" ]; then
        # Run with corpus
        "$TARGET_PATH" \
            -max_total_time="$DURATION" \
            -print_final_stats=1 \
            "$CORPUS_DIR" \
            || echo "Fuzz target $target finished with status $?"
    else
        # Run without corpus
        "$TARGET_PATH" \
            -max_total_time="$DURATION" \
            -print_final_stats=1 \
            || echo "Fuzz target $target finished with status $?"
    fi
    
    echo ""
    echo "----------------------------------------"
    echo ""
done

echo "=========================================="
echo "All fuzz tests completed"
echo "=========================================="
