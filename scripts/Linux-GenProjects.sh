#!/usr/bin/env bash

set -e

cd "$(dirname "$0")/.."

echo "Generating GNU Make project files..."
premake5 gmake

echo
echo "Done."
echo "Build with:"
echo "   make config=debug -j2"
echo
echo "To refresh clangd's compilation database:"
echo "   make config=debug clean"
echo "   bear -- make config=debug -j2"
