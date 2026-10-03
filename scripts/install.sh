#!/bin/bash
set -e

echo "Building Passman in Release mode..."
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

echo "Installing Passman to ~/.local/bin..."
cmake --install build-release --prefix "$HOME/.local"

echo "------------------------------------------------------------------"
echo "Installation complete!"
echo "Ensure ~/.local/bin is in your PATH to use 'passman' globally."
echo "Add this to your .zshrc or .bashrc if not already present:"
echo 'export PATH="$HOME/.local/bin:$PATH"'
echo "------------------------------------------------------------------"
