#!/bin/bash

# Usage function
usage() {
    echo "Usage: $0 <num_mpc_parties> [mode] [operation]"
    echo "Modes: reqrep, dealerrouter (default: dealerrouter)"
    echo "Default number of MPC parties: 3"
    echo "Default operation: add"
    echo "We automatically create two additional parties (IDs = NUM_PARTIES+1, NUM_PARTIES+2) holding secrets."
    exit 1
}

# Check if mode is provided
if [ $# -lt 1 ]; then
    usage
fi

NUM_MPC_PARTIES=${1:-3}  # Default to 3 parties if not specified
MODE=${2:-dealerrouter}  # Default to dealerrouter if not specified
OPERATION=${3:-add}  # Default operation is "add" if not specified

# The total parties = MPC parties + 2 secret parties
TOTAL_PARTIES=$((NUM_MPC_PARTIES + 2))

echo "Launching $NUM_MPC_PARTIES MPC parties + 2 secret parties = $TOTAL_PARTIES total."
echo "Mode: $MODE, Operation: $OPERATION"

# Clean ports
PORTS=()
for ((i=0; i<$TOTAL_PARTIES; i++)); do
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

# Start non-secret parties first
PIDS=()
for ((i=1; i<=$NUM_MPC_PARTIES; i++)); do
    INPUT_VALUE=$((i * 10))
    ./netiomp_test "$MODE" "$i" "$TOTAL_PARTIES" "$INPUT_VALUE" 0 "$OPERATION" &
    PIDS+=($!)
    sleep 1
done

# Now launch the secret parties
SECRET_PARTY_1=$((NUM_MPC_PARTIES + 1))
SECRET_PARTY_2=$((NUM_MPC_PARTIES + 2))

for sp in $SECRET_PARTY_1 $SECRET_PARTY_2; do
    INPUT_VALUE=$((sp * 10))
    ./netiomp_test "$MODE" "$sp" "$TOTAL_PARTIES" "$INPUT_VALUE" 1 "$OPERATION" &
    PIDS+=($!)
    sleep 1
done

# Wait for all parties to finish
for pid in "${PIDS[@]}"; do
    wait $pid
done
