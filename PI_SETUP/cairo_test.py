#!/usr/bin/env python3
# Cairo line drawing performance test

import cairo
import numpy as np
import math
import time

WIDTH, HEIGHT = 1024, 768
FPS = 24
SAMPLES = 1024
FB = '/dev/fb0'

def generate_lissajous(t, a=1, b=2):
    angles = np.linspace(0, 2 * math.pi, SAMPLES)
    x = np.sin(a * angles + t)
    y = np.sin(b * angles)
    px = ((x + 1) / 2 * (WIDTH - 1))
    py = ((y + 1) / 2 * (HEIGHT - 1))
    return px, py

def main():
    try:
        fb = open(FB, 'wb')
    except Exception as e:
        print(f"Error opening framebuffer: {e}")
        return

    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, WIDTH, HEIGHT)
    ctx = cairo.Context(surface)
    ctx.set_antialias(cairo.ANTIALIAS_GOOD)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    ctx.set_line_join(cairo.LINE_JOIN_ROUND)

    blank = np.zeros(HEIGHT * WIDTH * 4, dtype=np.uint8)
    t = 0.0
    interval = 1.0 / FPS
    frame_count = 0
    fps_timer = time.perf_counter()

    try:
        while True:
            t0 = time.perf_counter()

            # Clear
            ctx.set_operator(cairo.OPERATOR_CLEAR)
            ctx.paint()
            ctx.set_operator(cairo.OPERATOR_OVER)

            # Draw trace
            px, py = generate_lissajous(t)
            ctx.set_source_rgba(0, 1, 0, 1)
            ctx.set_line_width(2)
            ctx.move_to(px[0], py[0])
            for i in range(1, len(px)):
                ctx.line_to(px[i], py[i])
            ctx.stroke()

            # Write to framebuffer
            buf = surface.get_data()
            fb.seek(0)
            fb.write(buf)
            fb.flush()

            t += 0.05
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
    finally:
        fb.seek(0)
        fb.write(bytes(blank))
        fb.close()
        print("\nExited.")

if __name__ == '__main__':
    main()
