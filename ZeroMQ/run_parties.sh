#!/bin/bash

# Usage function
usage() {
    echo "Usage: $0 <mode> [num_parties]"
    echo "Modes: reqrep, dealerrouter"
    echo "Default number of parties: 3"
    exit 1
}

# Check if mode is provided
if [ $# -lt 1 ]; then
    usage
fi

MODE=$1
NUM_PARTIES=${2:-3}  # Default to 3 parties if not specified

if [ "$MODE" != "reqrep" ] && [ "$MODE" != "dealerrouter" ]; then
    echo "Invalid mode: $MODE"
    usage
fi

echo "Running $NUM_PARTIES parties in mode: $MODE"

# Cleanup any existing processes
# Comment out or remove this section if it’s terminating new processes:
# for ((i=1; i<=$NUM_PARTIES; i++)); do
#     pkill -f "$MODE $i"
# done
# sleep 2

# Clean ports
PORTS=()
for ((i=0; i<$NUM_PARTIES; i++)); do
    PORTS+=($((5555 + i)))
done

# Function to clean ports
clean_ports() {
    for port in "${PORTS[@]}"
    do
        PIDS=$(lsof -t -i :"$port")
        if [ ! -z "$PIDS" ]; then
            for PID in $PIDS; do
                echo "Port $port is in use by PID $PID. Terminating..."
                kill -9 "$PID" 2>/dev/null
            done
        else
            echo "Port $port is free."
        fi
    done
}

# Clean the ports
echo "Cleaning specified ports..."
clean_ports

# Start all parties
PIDS=()
for ((i=1; i<=$NUM_PARTIES; i++)); do
    # Each party starts with input value = party_id * 10
    INPUT_VALUE=$((i * 10))
    ./netiomp_test $MODE $i $NUM_PARTIES $INPUT_VALUE &
    PIDS+=($!)
    sleep 1
done

# Wait for all parties to finish
for pid in "${PIDS[@]}"; do
    wait $pid
done
