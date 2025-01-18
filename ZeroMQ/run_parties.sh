#!/bin/bash

# Usage function
usage() {
    echo "Usage: $0 <mode>"
    echo "Modes: reqrep, dealerrouter"
    exit 1
}

# Check if mode is provided
if [ $# -ne 1 ]; then
    usage
fi

MODE=$1

if [ "$MODE" != "reqrep" ] && [ "$MODE" != "dealerrouter" ]; then
    echo "Invalid mode: $MODE"
    usage
fi

echo "Running parties in mode: $MODE"

# Cleanup any existing processes with specific party IDs
pkill -f "$1 1"
pkill -f "$1 2"
pkill -f "$1 3"
sleep 5  # Increased delay to ensure ports are freed

# Define the ports to clean
PORTS=(5555 5556 5557)

# Function to clean ports
clean_ports() {
    for port in "${PORTS[@]}"
    do
        # Retrieve all PIDs using the current port
        PIDS=$(lsof -t -i :"$port")
        
        if [ ! -z "$PIDS" ]; then
            for PID in $PIDS; do
                echo "Port $port is in use by PID $PID. Terminating the process."
                kill -9 "$PID" 2>/dev/null
                if [ $? -eq 0 ]; then
                    echo "Successfully terminated process $PID on port $port."
                else
                    echo "Failed to terminate process $PID on port $port."
                fi
            done
        else
            echo "Port $port is free."
        fi
    done
}

# Clean the ports before starting the parties
echo "Cleaning specified ports to ensure they are free."
clean_ports

# Start Party 1
./netiomp_test $MODE 1 10 &
PID1=$!
sleep 1

# Start Party 2
./netiomp_test $MODE 2 20 &
PID2=$!
sleep 1

# Start Party 3
./netiomp_test $MODE 3 30 &
PID3=$!

# Wait for all parties to finish
wait $PID1
wait $PID2
wait $PID3
