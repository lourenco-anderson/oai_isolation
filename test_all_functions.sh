#!/bin/bash

# Test script to verify all functions work correctly after new imports/implementations
# Run from /home/anderson/dev/oai_isolation/build directory

echo "======================================"
echo "Testing All OAI Isolation Functions"
echo "======================================"
echo ""

FUNCTIONS=(
    "nr_scramble"
    "nr_crc"
    "nr_ofdm_modulation"
    "nr_precoding"
    "nr_modulation_test"
    "nr_layermapping"
    "nr_ldpc"
    "nr_ofdm_demo"
    "nr_ch_estimation"
    "nr_descrambling"
    "nr_layer_demapping_test"
    "nr_crc_check"
    "nr_soft_demod"
    "nr_mmse_eq"
    "nr_ldpc_dec"
)

PASSED=0
FAILED=0
ERRORS=""

for func in "${FUNCTIONS[@]}"; do
    echo "Testing: $func..."
    
    # Update main.c to call this function
    sed -i "s/^    [a-z_]*();$/    $func();/" /home/anderson/dev/oai_isolation/src/main.c
    
    # Recompile
    cd /home/anderson/dev/oai_isolation/build
    make -j4 >/dev/null 2>&1
    
    if [ $? -ne 0 ]; then
        echo "  ❌ COMPILATION FAILED"
        FAILED=$((FAILED + 1))
        ERRORS="$ERRORS\n  - $func: Compilation failed"
        continue
    fi
    
    # Run the function
    ./oai_isolation >/dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        echo "  ✅ PASSED"
        PASSED=$((PASSED + 1))
    else
        echo "  ❌ EXECUTION FAILED"
        FAILED=$((FAILED + 1))
        ERRORS="$ERRORS\n  - $func: Execution failed"
    fi
done

echo ""
echo "======================================"
echo "Test Results Summary"
echo "======================================"
echo "Total Functions: ${#FUNCTIONS[@]}"
echo "Passed: $PASSED ✅"
echo "Failed: $FAILED ❌"
echo ""

if [ $FAILED -gt 0 ]; then
    echo "Failed tests:"
    echo -e "$ERRORS"
    exit 1
else
    echo "All tests passed! ✅"
    exit 0
fi
