#!/bin/bash
# XY Audio Plotter - Pi setup script
# Run as the target user with sudo access
# Usage: bash setup.sh

set -e

PROJECT_DIR="/home/$USER/xy_plotter"
CONFIG="/boot/firmware/config.txt"
MAC="b8:27:eb:00:00:01"

echo "=== XY Audio Plotter Pi Setup ==="
echo "User: $USER"
echo "Project dir: $PROJECT_DIR"
echo ""

# --- Expand filesystem ---
echo "[1/7] Expanding filesystem to fill SD card..."
sudo raspi-config nonint do_expand_rootfs

# --- System update ---
echo "[2/7] Updating system..."
sudo apt update -y
sudo apt upgrade -y

# --- Install dependencies ---
echo "[3/7] Installing dependencies..."
sudo apt install -y python3-numpy python3-cairo

# --- User groups ---
echo "[4/7] Adding $USER to video and render groups..."
sudo usermod -aG video,render "$USER"

# --- config.txt ---
echo "[5/7] Configuring /boot/firmware/config.txt..."

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
add_if_missing "dtoverlay=enc28j60,cs=0,int_pin=25,speed=12000000"
add_if_missing "hdmi_group=2"
add_if_missing "hdmi_mode=17"
add_if_missing "hdmi_force_hotplug=1"
add_if_missing "disable_overscan=1"

echo "[all]" | sudo tee -a "$CONFIG" > /dev/null

# --- udev rule for fixed MAC ---
echo "[6/7] Installing udev rule for fixed MAC ($MAC)..."
echo "SUBSYSTEM==\"net\", ACTION==\"add\", DRIVERS==\"enc28j60\", RUN+=\"/sbin/ip link set dev %k address $MAC\"" \
    | sudo tee /etc/udev/rules.d/70-enc28j60-mac.rules > /dev/null
sudo udevadm control --reload-rules

# --- Project files ---
echo "[7/7] Creating project directory and visualiser..."
mkdir -p "$PROJECT_DIR"

cat > "$PROJECT_DIR/visualiser.py" << 'PYEOF'
import numpy as np
import time
import math
import sys
import os

WIDTH, HEIGHT = 1024, 768
FPS = 24
SAMPLES = 1024
FB = '/dev/fb0'

# Framebuffer is 32-bit ARGB8888
def rgb32(r, g, b):
    return (0xFF << 24) | (r << 16) | (g << 8) | b

BLACK = rgb32(0, 0, 0)
CORE  = rgb32(0, 255, 0)
GLOW1 = rgb32(0, 120, 0)

# Pre-compute offset list for glow + thickness (3x3 block)
_OFFSETS_MID = [(dx, dy) for dx in range(-1, 2) for dy in range(-1, 2)]

def generate_lissajous(t, a=1, b=2):
    angles = np.linspace(0, 2 * math.pi, SAMPLES)
    x = np.sin(a * angles + t)
    y = np.sin(b * angles)
    px = ((x + 1) / 2 * (WIDTH - 1)).astype(int)
    py = ((y + 1) / 2 * (HEIGHT - 1)).astype(int)
    return px, py

def _plot_offsets(frame, px, py, offsets, colour):
    all_x = np.concatenate([px + dx for dx, dy in offsets])
    all_y = np.concatenate([py + dy for dx, dy in offsets])
    mask = (all_x >= 0) & (all_x < WIDTH) & (all_y >= 0) & (all_y < HEIGHT)
    frame[all_y[mask], all_x[mask]] = colour

def draw_trace(frame, px, py):
    _plot_offsets(frame, px, py, _OFFSETS_MID, GLOW1)  # glow + thickness
    mask = (px >= 0) & (px < WIDTH) & (py >= 0) & (py < HEIGHT)
    frame[py[mask], px[mask]] = CORE                   # core

def main():
    try:
        fb = open(FB, 'wb')
    except Exception as e:
        print(f"Error opening framebuffer: {e}")
        return

    # Hide cursor on local console (HDMI)
    with open('/dev/tty1', 'w') as tty:
        tty.write("\033[?25l")

    frame = np.zeros((HEIGHT, WIDTH), dtype=np.uint32)
    blank = np.zeros((HEIGHT, WIDTH), dtype=np.uint32)
    t = 0.0
    interval = 1.0 / FPS
    frame_count = 0
    fps_timer = time.perf_counter()

    try:
        while True:
            t0 = time.perf_counter()
            frame[:] = BLACK
            px, py = generate_lissajous(t)
            draw_trace(frame, px, py)
            fb.seek(0)
            fb.write(frame.tobytes())
            fb.flush()
            t += 0.05
            frame_count += 1
            elapsed = time.perf_counter() - t0
            if time.perf_counter() - fps_timer >= 1.0:
                print(f"FPS: {frame_count}  frame time: {elapsed*1000:.1f}ms")  # remove after benchmarking — print adds SSH latency
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
            fb.write(blank.tobytes())
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

if __name__ == "__main__":
    main()
PYEOF

echo ""
echo "=== Setup complete ==="
echo "Reboot required: sudo reboot"
echo "After reboot, verify framebuffer resolution: cat /sys/class/graphics/fb0/virtual_size"
echo "Update WIDTH, HEIGHT in $PROJECT_DIR/visualiser.py if needed, then run: python3 $PROJECT_DIR/visualiser.py"
echo ""
echo "NOTE: Log back in after reboot for group membership (video, render) to take effect."
