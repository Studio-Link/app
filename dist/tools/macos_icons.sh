#!/bin/bash -e
#
# Regenerate the macOS app icon set and the menu bar icon from the SVGs in
# src/standalone/macos/icon (needs rsvg-convert, librsvg)
#
#   AppIcon.svg            -> StudioLink.iconset/*.png (iconutil at build)
#   studio-link-mark.svg   -> MenuBarIcon.pdf (template image, 16pt high)
#
cd "$(dirname "$0")/../../src/standalone/macos"

for size in 16 32 128 256 512; do
    rsvg-convert -w $size -h $size icon/AppIcon.svg \
        -o StudioLink.iconset/icon_${size}x${size}.png
    rsvg-convert -w $((size * 2)) -h $((size * 2)) icon/AppIcon.svg \
        -o StudioLink.iconset/icon_${size}x${size}@2x.png
done

# vector PDF in points (72 dpi), black + alpha for NSImage.isTemplate
rsvg-convert -f pdf --dpi-x 72 --dpi-y 72 -h 16 icon/studio-link-mark.svg \
    -o MenuBarIcon.pdf
