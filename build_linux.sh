#!/bin/bash
# Linux Build Script

# Install dependencies if needed (Standard for Ubuntu/Debian)
# sudo apt-get update
# sudo apt-get install build-essential cmake libx11-dev libxcursor-dev libxinerama-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev zenity

# Create build directory
mkdir -p build_linux
cd build_linux

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
if cmake --build . --config Release; then
    echo "Build complete! Output in build_linux/FntGenerator"

    # Ask to run with 5s timeout
    read -t 5 -p "Do you want to run FntGenerator now? (Y/n) (Auto-run in 5s): " choice
    if [ $? -ne 0 ]; then
        echo # Newline after timeout
        ./FntGenerator
    else
        if [[ "$choice" != "n" && "$choice" != "N" ]]; then
            ./FntGenerator
        fi
    fi
else
    echo "Build failed!"
    exit 1
fi
