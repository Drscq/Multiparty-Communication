#!/bin/bash

# Start a new tmux session
tmux new-session -d -s multiparty

# Launch party 1 with input value 10 in the first window
tmux send-keys -t multiparty:0 "./netiomp_test 1 10" C-m

# Wait for a short period to ensure party 1 is ready
sleep 2

# Create a new window for party 2 and launch it with input value 20
tmux new-window -t multiparty:1
tmux send-keys -t multiparty:1 "./netiomp_test 2 20" C-m

# Wait for a short period to ensure party 2 is ready
sleep 2

# Create a new window for party 3 and launch it with input value 30
tmux new-window -t multiparty:2
tmux send-keys -t multiparty:2 "./netiomp_test 3 30" C-m

# Attach to the tmux session
tmux attach-session -t multiparty
