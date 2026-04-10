#!/usr/bin/env python3
# XY Audio Lissajous visualiser — Cairo renderer
# Direct framebuffer output, no display server required

import cairo
import numpy as np
import time
import sys
import spi_xy_read

WIDTH, HEIGHT = 1024, 768
FPS = 24
FB = '/dev/fb0'

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

    spi_xy_read.open_spi()

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

            result = spi_xy_read.read_xy()
            if result is not None:
                px, py = result
                px = px / 4095.0 * (WIDTH - 1)
                py = py / 4095.0 * (HEIGHT - 1)
                draw_trace(ctx, px, py)

            fb.seek(0)
            fb.write(surface.get_data())
            fb.flush()

            frame_count += 1
            elapsed = time.perf_counter() - t0
            if time.perf_counter() - fps_timer >= 1.0:
                print(f"FPS: {frame_count}  frame time: {elapsed*1000:.1f}ms")  # remove after benchmarking
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
        spi_xy_read.close_spi()
        try:
            with open('/dev/tty1', 'w') as tty:
                tty.write("\033[?25h\033[2J\033[H")
        except Exception:
            pass
        print("\nExited.")

if __name__ == '__main__':
    main()
