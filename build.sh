#!/usr/bin/env bash
set -e

echo "=== WiiU-Bypass Builder ==="
echo ""

# Check if devkitPPC is installed locally
if [ -n "$DEVKITPPC" ] && [ -d "$DEVKITPPC" ]; then
    echo "[*] Using local devkitPPC at: $DEVKITPPC"
    make -j$(sysctl -n hw.logicalcpu 2>/dev/null || nproc 2>/dev/null || echo 4)
    echo ""
    echo "[+] Build complete! Output: wiiu-bypass.wps"
    echo "[+] Copy to SD: cp wiiu-bypass.wps /path/to/sd/wiiu/environments/aroma/plugins/"
    exit 0
fi

# Fallback: use Docker
echo "[*] devkitPPC not found locally, using Docker..."
echo "[*] Building image (first time only)..."
docker build -t wiiu-bypass-builder .

echo "[*] Building plugin..."
docker run --rm -v "$(pwd):/project" wiiu-bypass-builder

echo ""
echo "[+] Build complete! Output: wiiu-bypass.wps"
echo "[+] Copy to SD: cp wiiu-bypass.wps /path/to/sd/wiiu/environments/aroma/plugins/"
