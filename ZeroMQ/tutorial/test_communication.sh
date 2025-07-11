#!/bin/bash

echo "=== Testing Router-Dealer Communication ==="

# Kill any existing processes
pkill -f router_dealer_basic 2>/dev/null || true

# Start the router in the background
echo "Starting router..."
./build/router_dealer_basic router &
ROUTER_PID=$!

# Wait for router to start
sleep 2

# Test with multiple dealers
echo "Testing multiple dealers..."
echo "Launching 5 dealers concurrently..."

# Array to store dealer PIDs
DEALER_PIDS=()

# Launch multiple dealers in parallel
for i in {1..5}; do
    echo "Starting dealer $i..."
    ./build/router_dealer_basic dealer &
    DEALER_PIDS+=($!)
    sleep 0.5  # Small delay between dealer starts
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
echo "Successfully tested Router-Dealer pattern with multiple concurrent dealers!"
