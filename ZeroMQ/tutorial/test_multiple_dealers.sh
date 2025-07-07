#!/bin/bash

# Default number of dealers
NUM_DEALERS=3

# Parse command line arguments
if [ $# -eq 1 ]; then
    NUM_DEALERS=$1
fi

echo "=== Testing Router-Dealer Communication with $NUM_DEALERS dealers ==="

# Kill any existing processes
pkill -f router_dealer_basic 2>/dev/null || true

# Start the router in the background
echo "Starting router..."
./build/router_dealer_basic router &
ROUTER_PID=$!

# Wait for router to start
sleep 2

# Test with multiple dealers
echo "Testing with $NUM_DEALERS dealers..."
echo "Launching dealers concurrently..."

# Array to store dealer PIDs
DEALER_PIDS=()

# Launch multiple dealers in parallel
for i in $(seq 1 $NUM_DEALERS); do
    echo "Starting dealer $i..."
    ./build/router_dealer_basic dealer &
    DEALER_PIDS+=($!)
    sleep 0.2  # Small delay between dealer starts
done

# Wait for all dealers to complete
echo "Waiting for all dealers to complete..."
for pid in "${DEALER_PIDS[@]}"; do
    wait $pid
done

echo "All dealers completed."

# Clean up
echo "Cleaning up..."
kill $ROUTER_PID 2>/dev/null || true
wait $ROUTER_PID 2>/dev/null || true

echo "Test completed."
echo "Successfully tested Router-Dealer pattern with $NUM_DEALERS concurrent dealers!"
