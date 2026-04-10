#!/usr/bin/env python3
# SPI XY reader — replaces dummy Lissajous with live Blue Pill data
# Reads 1024 XY pairs (4096 bytes) from SPI CE1 at 16 MHz
# Format from STM32: [X_hi, X_lo, Y_hi, Y_lo] × 1024, 12-bit big-endian uint16

import spidev
import struct
import numpy as np

SAMPLES  = 1024
BUF_SIZE = SAMPLES * 4   # bytes

_spi = None

def open_spi():
    global _spi
    _spi = spidev.SpiDev()
    _spi.open(0, 0)              # bus 0, CE0 (GPIO 8)
    _spi.max_speed_hz = 16_000_000
    _spi.mode = 0
    _spi.bits_per_word = 8

def read_xy():
    """Return (px, py) arrays of float pixel coordinates, or None on error."""
    raw = _spi.readbytes(BUF_SIZE)
    if len(raw) != BUF_SIZE:
        return None
    data = np.frombuffer(bytes(raw), dtype=np.dtype('>u2'))  # big-endian uint16
    xi = data[0::2].astype(np.float32)   # even indices = X
    yi = data[1::2].astype(np.float32)   # odd  indices = Y
    return xi, yi

def close_spi():
    if _spi:
        _spi.close()

if __name__ == '__main__':
    import sys
    import time
    N = int(sys.argv[1]) if len(sys.argv) > 1 else 24
    open_spi()
    print(f"SPI XY read test — {N} frames, CE0, 16 MHz, mode 0")
    try:
        for i in range(1, N + 1):
            result = read_xy()
            if result is None:
                print(f"[{i:3}/{N}] FAIL  bad read length")
            else:
                xi, yi = result
                print(f"[{i:3}/{N}] OK    X min={xi.min():.0f} max={xi.max():.0f} "
                      f"Y min={yi.min():.0f} max={yi.max():.0f}")
            time.sleep(1 / 24)
    finally:
        close_spi()
