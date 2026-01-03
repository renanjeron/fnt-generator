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
cmake --build . --config Release

echo "Build complete! Output in build_linux/FontExporter"

# Ask to run
read -p "Do you want to run Font Exporter now? (y/n): " choice
if [[ "$choice" == "y" || "$choice" == "Y" ]]; then
    ./FontExporter
fi
