#!/bin/bash
# MacOS Build Script

# Create build directory
mkdir -p build_macos
cd build_macos

# Configure
# For Universal Binary (Intel + Apple Silicon):
# cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

echo "Build complete! Output in build_macos/FntGenerator.app"

# Ask to run
read -p "Do you want to run FntGenerator now? (y/n): " choice
if [[ "$choice" == "y" || "$choice" == "Y" ]]; then
    open FntGenerator.app
fi
