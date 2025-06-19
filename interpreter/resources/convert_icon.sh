#!/bin/bash

# Check if ImageMagick is installed
if ! command -v convert &> /dev/null; then
    echo "ImageMagick is required. Please install it first."
    exit 1
fi

# Create temporary directory
mkdir -p temp

# Convert SVG to PNG in different sizes
convert -background none -size 16x16 resources/icon.svg temp/icon_16.png
convert -background none -size 32x32 resources/icon.svg temp/icon_32.png
convert -background none -size 48x48 resources/icon.svg temp/icon_48.png
convert -background none -size 64x64 resources/icon.svg temp/icon_64.png
convert -background none -size 128x128 resources/icon.svg temp/icon_128.png
convert -background none -size 256x256 resources/icon.svg temp/icon_256.png

# Combine PNGs into ICO
convert temp/icon_*.png resources/icon.ico

# Clean up
rm -rf temp

echo "Icon conversion completed successfully!" 