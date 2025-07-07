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

# Test with dealer
echo "Testing dealer..."
./build/router_dealer_basic dealer

# Clean up
echo "Cleaning up..."
kill $ROUTER_PID 2>/dev/null || true
wait $ROUTER_PID 2>/dev/null || true

echo "Test completed."
