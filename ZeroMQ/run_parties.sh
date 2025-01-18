#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <executable>"
    exit 1
fi

# Cleanup any existing processes with specific party IDs
pkill -f "$1 1"
pkill -f "$1 2"
pkill -f "$1 3"
sleep 5  # Increased delay to ensure ports are freed

# Launch party 1 with input value 10
$1 1 10 &
sleep 5  # Increased delay to ensure proper initialization

# Launch party 2 with input value 20
$1 2 20 &
sleep 5  # Increased delay to ensure proper initialization

# Launch party 3 with input value 30
$1 3 30 &

# Wait for all parties to complete
wait
