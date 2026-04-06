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
GLOW2 = rgb32(0, 40, 0)

# Pre-compute offset lists
_OFFSETS_GLOW = [(dx, dy) for dx in range(-3, 4) for dy in range(-3, 4) if 2 < (dx**2+dy**2)**0.5 <= 4]
_OFFSETS_MID  = [(dx, dy) for dx in range(-1, 2) for dy in range(-1, 2)]

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
        # Clear screen and restore cursor on exit
        try:
            fb.seek(0)
            fb.write(blank.tobytes())
            fb.flush()
            fb.close()
        except Exception:
            pass
        # Restore cursor and console on HDMI
        try:
            with open('/dev/tty1', 'w') as tty:
                tty.write("\033[?25h\033[2J\033[H")

        except Exception:
            pass
        print("\nExited.")

if __name__ == "__main__":
    main()
