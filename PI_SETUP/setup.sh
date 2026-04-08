#!/bin/bash
# XY Audio Plotter - Pi setup script
# Run as the target user with sudo access
# Usage: bash setup.sh

set -e

PROJECT_DIR="/home/$USER/xy_plotter"
CONFIG="/boot/firmware/config.txt"
echo "=== XY Audio Plotter Pi Setup ==="
echo "User: $USER"
echo "Project dir: $PROJECT_DIR"
echo ""

# --- Expand filesystem ---
echo "[1/6] Expanding filesystem to fill SD card..."
sudo raspi-config nonint do_expand_rootfs

# --- System update ---
echo "[2/6] Updating system..."
sudo apt update -y
sudo apt upgrade -y

# --- Install dependencies ---
echo "[3/6] Installing dependencies..."
sudo apt install -y python3-numpy python3-cairo python3-spidev

# --- User groups ---
echo "[4/6] Adding $USER to video and render groups..."
sudo usermod -aG video,render "$USER"

# --- config.txt ---
echo "[5/6] Configuring /boot/firmware/config.txt..."

add_if_missing() {
    grep -qF "$1" "$CONFIG" || echo "$1" | sudo tee -a "$CONFIG" > /dev/null
}

# Comment out disable_fw_kms_setup=1 — causes HDMI dropout on KMS handoff
sudo sed -i 's/^disable_fw_kms_setup=1/#disable_fw_kms_setup=1/' "$CONFIG"

# Comment out display_auto_detect — we set mode explicitly
sudo sed -i 's/^display_auto_detect=1/#display_auto_detect=1/' "$CONFIG"

# Remove [all] and re-add at end to ensure our lines are before it
sudo sed -i '/^\[all\]/d' "$CONFIG"

add_if_missing "dtparam=spi=on"
add_if_missing "hdmi_group=2"
add_if_missing "hdmi_mode=17"
add_if_missing "hdmi_force_hotplug=1"
add_if_missing "disable_overscan=1"

echo "[all]" | sudo tee -a "$CONFIG" > /dev/null

# --- Project files ---
echo "[6/6] Creating project directory and visualiser..."
mkdir -p "$PROJECT_DIR"

cat > "$PROJECT_DIR/visualiser.py" << 'PYEOF'
#!/usr/bin/env python3
# XY Audio Lissajous visualiser — Cairo renderer
# Direct framebuffer output, no display server required

import cairo
import numpy as np
import math
import time
import sys

WIDTH, HEIGHT = 1024, 768
FPS = 24
SAMPLES = 1024
FB = '/dev/fb0'
SPEED = 0.05  # phase increment per frame

def generate_lissajous(t, a=1, b=2):
    angles = np.linspace(0, 2 * math.pi, SAMPLES)
    x = np.sin(a * angles + t)
    y = np.sin(b * angles)
    px = (x + 1) / 2 * (WIDTH - 1)
    py = (y + 1) / 2 * (HEIGHT - 1)
    return px, py

def draw_trace(ctx, px, py):
    ctx.move_to(px[0], py[0])
    for i in range(1, len(px)):
        ctx.line_to(px[i], py[i])
    ctx.stroke()

def main():
    try:
        fb = open(FB, 'wb')
    except Exception as e:
        print(f"Error opening framebuffer: {e}")
        return

    with open('/dev/tty1', 'w') as tty:
        tty.write("\033[?25l")

    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, WIDTH, HEIGHT)
    ctx = cairo.Context(surface)
    ctx.set_antialias(cairo.ANTIALIAS_GOOD)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    ctx.set_line_join(cairo.LINE_JOIN_ROUND)
    ctx.set_line_width(1.5)
    ctx.set_source_rgba(0, 1, 0, 1)

    blank = bytes(HEIGHT * WIDTH * 4)
    t = 0.0
    interval = 1.0 / FPS
    frame_count = 0
    fps_timer = time.perf_counter()

    try:
        while True:
            t0 = time.perf_counter()

            ctx.set_operator(cairo.OPERATOR_CLEAR)
            ctx.paint()
            ctx.set_operator(cairo.OPERATOR_OVER)
            ctx.set_source_rgba(0, 1, 0, 1)

            px, py = generate_lissajous(t)
            draw_trace(ctx, px, py)

            fb.seek(0)
            fb.write(surface.get_data())
            fb.flush()

            t += SPEED
            frame_count += 1
            elapsed = time.perf_counter() - t0
            if time.perf_counter() - fps_timer >= 1.0:
                print(f"FPS: {frame_count}  frame time: {elapsed*1000:.1f}ms")
                frame_count = 0
                fps_timer = time.perf_counter()

            sleep_time = interval - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    except KeyboardInterrupt:
        pass
    except Exception as e:
        print(f"Error: {e}")
    finally:
        try:
            fb.seek(0)
            fb.write(blank)
            fb.flush()
            fb.close()
        except Exception:
            pass
        try:
            with open('/dev/tty1', 'w') as tty:
                tty.write("\033[?25h\033[2J\033[H")
        except Exception:
            pass
        print("\nExited.")

if __name__ == '__main__':
    main()
PYEOF

echo ""
echo "=== Setup complete ==="
echo "Reboot required: sudo reboot"
echo "After reboot, verify framebuffer resolution: cat /sys/class/graphics/fb0/virtual_size"
echo "Update WIDTH, HEIGHT in $PROJECT_DIR/visualiser.py if needed, then run: python3 $PROJECT_DIR/visualiser.py"
echo ""
echo "NOTE: Log back in after reboot for group membership (video, render) to take effect."
