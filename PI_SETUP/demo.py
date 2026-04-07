#!/usr/bin/env python3
# CRT effects demo — Cairo renderer
# Cycles: sharp/no-trail → soft-beam/long-trail

import cairo
import numpy as np
import math
import time
import sys

WIDTH, HEIGHT = 1024, 768
FPS = 24
SAMPLES = 1024
FB = '/dev/fb0'
RAMP_SECS = 5  # seconds per ramp (up then down)

def generate_lissajous(t, a=1, b=2):
    angles = np.linspace(0, 2 * math.pi, SAMPLES)
    x = np.sin(a * angles + t)
    y = np.sin(b * angles)
    px = (x + 1) / 2 * (WIDTH - 1)
    py = (y + 1) / 2 * (HEIGHT - 1)
    return px, py

def draw_path(ctx, px, py):
    ctx.move_to(px[0], py[0])
    for i in range(1, len(px)):
        ctx.line_to(px[i], py[i])

def render_frame(ctx, px, py, brightness, focus, persistence):
    # Persistence: fade existing content with semi-transparent black
    ctx.set_operator(cairo.OPERATOR_OVER)
    ctx.set_source_rgba(0, 0, 0, 1.0 - persistence)
    ctx.paint()

    # Core trace
    draw_path(ctx, px, py)
    ctx.set_source_rgba(0, brightness, 0, 1.0)
    ctx.set_line_width(1.5 + focus * 6)
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

    # Initial clear
    ctx.set_operator(cairo.OPERATOR_CLEAR)
    ctx.paint()

    blank = bytes(HEIGHT * WIDTH * 4)
    t = 0.0
    interval = 1.0 / FPS
    ramp_frames = int(RAMP_SECS * FPS)
    total_phase_frames = ramp_frames * 2
    phase_frame = 0
    frame_count = 0
    fps_timer = time.perf_counter()

    try:
        while True:
            t0 = time.perf_counter()

            # Ramp 0→1→0
            ramp = phase_frame / ramp_frames if phase_frame < ramp_frames \
                   else 2.0 - phase_frame / ramp_frames
            ramp = max(0.0, min(1.0, ramp))

            brightness  = 1.0
            focus       = ramp
            persistence = ramp * 0.93

            px, py = generate_lissajous(t)
            render_frame(ctx, px, py, brightness, focus, persistence)

            fb.seek(0)
            fb.write(surface.get_data())
            fb.flush()

            t += 0.05
            phase_frame = (phase_frame + 1) % total_phase_frames
            frame_count += 1
            elapsed = time.perf_counter() - t0
            if time.perf_counter() - fps_timer >= 1.0:
                print(f"FPS: {frame_count}  frame time: {elapsed*1000:.1f}ms  focus: {focus:.2f}  persistence: {persistence:.2f}")
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
