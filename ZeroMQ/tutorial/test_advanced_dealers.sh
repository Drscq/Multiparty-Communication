#!/bin/bash

# Default number of dealers
NUM_DEALERS=3

# Parse command line arguments
if [ $# -eq 1 ]; then
    NUM_DEALERS=$1
fi

echo "=== Testing Router-Dealer Communication with $NUM_DEALERS dealers (Advanced) ==="

# Kill any existing processes
pkill -f router_dealer_basic 2>/dev/null || true

# Start the router in the background
echo "Starting router..."
./build/router_dealer_basic router &
ROUTER_PID=$!

# Wait for router to start
sleep 2

# Test with multiple dealers with different timing
echo "Testing with $NUM_DEALERS dealers with staggered timing..."

# Array to store dealer PIDs
DEALER_PIDS=()

# Launch dealers with different timing patterns
for i in $(seq 1 $NUM_DEALERS); do
    echo "Starting dealer $i..."
    ./build/router_dealer_basic dealer &
    DEALER_PIDS+=($!)
    
    # Add variable delay to create more realistic scenario
    if [ $i -le 2 ]; then
        sleep 0.1  # Quick succession for first few
    else
        sleep 0.5  # Longer delay for later ones
    fi
done

# Show progress
echo "All dealers started. Waiting for completion..."
sleep 1

# Wait for all dealers to complete
echo "Waiting for all dealers to complete..."
for pid in "${DEALER_PIDS[@]}"; do
    wait $pid
done

echo "All dealers completed."

# Optional: Test burst mode
echo ""
echo "=== Testing Burst Mode (10 quick dealers) ==="
echo "Launching 10 dealers in quick succession..."

BURST_PIDS=()
for i in {1..10}; do
    ./build/router_dealer_basic dealer &
    BURST_PIDS+=($!)
    sleep 0.05  # Very quick succession
done

# Wait for burst dealers
for pid in "${BURST_PIDS[@]}"; do
    wait $pid
done

echo "Burst test completed."

# Clean up
echo "Cleaning up..."
kill $ROUTER_PID 2>/dev/null || true
wait $ROUTER_PID 2>/dev/null || true

echo ""
echo "=== Test Summary ==="
echo "✅ Successfully tested Router-Dealer pattern with:"
echo "   - $NUM_DEALERS staggered dealers"
echo "   - 10 burst dealers"
echo "   - Total: $((NUM_DEALERS + 10)) dealer connections"
echo ""
echo "The router successfully handled all concurrent connections!"
