#!/bin/bash
# Script to profile all loading scenarios with heaptrack

set -e

PROFILER_BIN="/home/runner/work/WhiskerToolbox/WhiskerToolbox/out/build/Clang/Release/bin/profile_loader"
TEST_DATA_DIR="/home/runner/work/WhiskerToolbox/WhiskerToolbox/tools/profiling_loader/test_data"
OUTPUT_DIR="/home/runner/work/WhiskerToolbox/WhiskerToolbox/tools/profiling_loader/results"
HEAPTRACK_BIN="/home/runner/work/WhiskerToolbox/WhiskerToolbox/tools/heaptrack/build/bin/heaptrack"
HEAPTRACK_PRINT_BIN="/home/runner/work/WhiskerToolbox/WhiskerToolbox/tools/heaptrack/build/bin/heaptrack_print"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "========================================="
echo "Profiling Load Data Functions"
echo "========================================="
echo ""

# Array of test configurations
declare -a configs=(
    "config_analog_csv.json:Analog CSV (single channel)"
    "config_analog_binary_single.json:Analog Binary (single channel)"
    "config_analog_binary_multi.json:Analog Binary (multi channel)"
    "config_line_csv.json:Line CSV"
    "config_digital_event_csv.json:Digital Event CSV"
    "config_digital_interval_csv.json:Digital Interval CSV"
)

# Run each test scenario with heaptrack
for config_pair in "${configs[@]}"; do
    IFS=':' read -r config desc <<< "$config_pair"
    
    echo ""
    echo "========================================="
    echo "Testing: $desc"
    echo "Config: $config"
    echo "========================================="
    
    cd "$TEST_DATA_DIR"
    
    # Run with heaptrack
    output_file="$OUTPUT_DIR/heaptrack.$(basename $config .json)"
    
    echo "Running heaptrack..."
    "$HEAPTRACK_BIN" --output "$output_file" "$PROFILER_BIN" "$config"
    
    echo ""
    echo "Results saved to: ${output_file}.zst"
    
    # Generate text summary
    echo "Generating summary..."
    "$HEAPTRACK_PRINT_BIN" "${output_file}.zst" > "${output_file}.txt"
    
    echo "Summary saved to: ${output_file}.txt"
done

echo ""
echo "========================================="
echo "All profiling tests complete!"
echo "Results directory: $OUTPUT_DIR"
echo "========================================="
