#!/usr/bin/env python3
# XY Audio Lissajous visualiser
# Pi 3B, HDMI 1024x768, Cairo renderer, 24 FPS
# Reads 512 XY pairs (2048 bytes) per frame from STM32 via SPI CE0 at 16 MHz
# Format: [X_hi, X_lo, Y_hi, Y_lo] x 512  big-endian uint16, 12-bit (0-4095)

import cairo
import numpy as np
import spidev
import time

WIDTH, HEIGHT = 1024, 768
FPS = 24
FB = '/dev/fb0'
SAMPLES = 512
BUF_SIZE = SAMPLES * 4   # bytes
N_BANDS = 16             # colour quantisation bands
PERSISTENCE = 0.15       # alpha of black fade overlay per frame (higher = faster decay)
SCALE_SMOOTH = 0.97      # exponential smoothing for auto-scale (higher = slower response)
MARGIN = 0.90            # fraction of screen to fill at full scale

_spi = None
_scale = 1.0
_cx = 2047.5
_cy = 2047.5

# Colour ramp: blue -> cyan -> green -> yellow -> white
def _make_colours(n):
    colours = []
    stops = [
        (0.00, (0.4, 0.0, 0.0)),  # deep red
        (0.33, (1.0, 0.3, 0.0)),  # orange
        (0.67, (1.0, 0.8, 0.0)),  # amber/yellow
        (1.00, (1.0, 1.0, 0.9)),  # warm white
    ]
    for i in range(n):
        v = i / (n - 1)
        for j in range(len(stops) - 1):
            v0, c0 = stops[j]
            v1, c1 = stops[j + 1]
            if v0 <= v <= v1:
                t = (v - v0) / (v1 - v0)
                r = c0[0] + t * (c1[0] - c0[0])
                g = c0[1] + t * (c1[1] - c0[1])
                b = c0[2] + t * (c1[2] - c0[2])
                colours.append((r, g, b, 1.0))
                break
    return colours

COLOURS = _make_colours(N_BANDS)

def open_spi():
    global _spi
    _spi = spidev.SpiDev()
    _spi.open(0, 0)               # bus 0, CE0 (GPIO 8)
    _spi.max_speed_hz = 20_000_000
    _spi.mode = 0
    _spi.bits_per_word = 8

def read_xy():
    raw = _spi.readbytes(BUF_SIZE)
    if len(raw) != BUF_SIZE:
        return None
    data = np.frombuffer(bytes(raw), dtype=np.dtype('>u2'))
    return data[0::2].astype(np.float32), data[1::2].astype(np.float32)

def close_spi():
    if _spi:
        _spi.close()

def auto_scale(px, py):
    global _scale, _cx, _cy
    cx = (px.max() + px.min()) / 2
    cy = (py.max() + py.min()) / 2
    r  = max(px.max() - px.min(), py.max() - py.min(), 1.0)
    target = min(WIDTH, HEIGHT) * MARGIN / r
    _scale = _scale * SCALE_SMOOTH + target * (1 - SCALE_SMOOTH)
    _cx    = _cx    * SCALE_SMOOTH + cx     * (1 - SCALE_SMOOTH)
    _cy    = _cy    * SCALE_SMOOTH + cy     * (1 - SCALE_SMOOTH)
    return (px - _cx) * _scale + WIDTH  / 2, \
           (py - _cy) * _scale + HEIGHT / 2

def draw_trace(ctx, px, py):
    dx = np.diff(px)
    dy = np.diff(py)
    vel = np.sqrt(dx*dx + dy*dy)
    vmax = vel.max()
    vel_norm = np.sqrt(vel / vmax) if vmax > 0 else vel
    bands = (vel_norm * (N_BANDS - 1)).astype(int).clip(0, N_BANDS - 1)

    for b in range(N_BANDS):
        indices = np.where(bands == b)[0]
        if len(indices) == 0:
            continue
        ctx.set_source_rgba(*COLOURS[b])
        ctx.new_path()
        for i in indices:
            ctx.move_to(float(px[i]),   float(py[i]))
            ctx.line_to(float(px[i+1]), float(py[i+1]))
        ctx.stroke()

def main():
    try:
        fb = open(FB, 'wb')
    except Exception as e:
        print(f"Error opening framebuffer: {e}")
        return

    open_spi()

    try:
        with open('/dev/tty1', 'w') as tty:
            tty.write("\033[?25l")
    except Exception:
        pass

    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, WIDTH, HEIGHT)
    ctx = cairo.Context(surface)
    ctx.set_antialias(cairo.ANTIALIAS_GOOD)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    ctx.set_line_join(cairo.LINE_JOIN_ROUND)
    ctx.set_line_width(1.5)

    blank = bytes(HEIGHT * WIDTH * 4)
    interval = 1.0 / FPS

    try:
        while True:
            t0 = time.perf_counter()

            ctx.set_operator(cairo.OPERATOR_OVER)
            ctx.set_source_rgba(0, 0, 0, PERSISTENCE)
            ctx.paint()

            result = read_xy()
            if result is not None:
                px, py = result
                px, py = auto_scale(px, py)
                draw_trace(ctx, px, py)

            fb.seek(0)
            fb.write(surface.get_data())
            fb.flush()

            sleep_time = interval - (time.perf_counter() - t0)
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
        close_spi()
        try:
            with open('/dev/tty1', 'w') as tty:
                tty.write("\033[?25h\033[2J\033[H")
        except Exception:
            pass

if __name__ == '__main__':
    main()
