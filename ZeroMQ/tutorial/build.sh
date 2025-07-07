#!/bin/bash

# Create build directory if it doesn't exist
mkdir -p build

# Navigate to build directory
cd build

# Run cmake and make
cmake .. && make

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "✅ Compilation successful!"
    echo "📁 Executable located at: build/router_dealer"
    echo "🚀 Run with: cd build && ./router_dealer"
else
    echo "❌ Compilation failed!"
    exit 1
fi
