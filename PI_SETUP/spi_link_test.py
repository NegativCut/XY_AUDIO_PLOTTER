#!/usr/bin/env python3
# SPI link test — Pi 3B master, Blue Pill slave
# Reads 16 bytes from CE0 and validates against expected pattern.
# Run with: python3 spi_link_test.py

import spidev
import time

BUS      = 0
DEVICE   = 0   # CE0, GPIO 8
SPEED_HZ = 16_000_000
MODE     = 0
REPS     = 10

EXPECTED = [
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
]

def main():
    spi = spidev.SpiDev()
    spi.open(BUS, DEVICE)
    spi.max_speed_hz = SPEED_HZ
    spi.mode = MODE

    print(f"SPI link test — bus {BUS}, CE{DEVICE}, {SPEED_HZ//1_000_000} MHz, mode {MODE}")
    print(f"Expected: {[hex(b) for b in EXPECTED]}\n")

    passed = 0
    for i in range(REPS):
        rx = spi.readbytes(len(EXPECTED))
        ok = (rx == EXPECTED)
        status = "PASS" if ok else "FAIL"
        print(f"[{i+1:2d}/{REPS}] {status}  rx: {[hex(b) for b in rx]}")
        if ok:
            passed += 1
        time.sleep(0.1)

    spi.close()
    print(f"\n{passed}/{REPS} passed")

if __name__ == '__main__':
    main()
