#!/usr/bin/env python3
import numpy as np
from picamera2 import Picamera2

with open('/sys/class/graphics/fb0/virtual_size') as f:
    w, h = map(int, f.read().strip().split(','))
with open('/sys/class/graphics/fb0/bits_per_pixel') as f:
    bpp = int(f.read().strip())

picam2 = Picamera2()
config = picam2.create_preview_configuration(main={"size": (w, h), "format": "RGB888"},
                                             controls={"FrameRate": 30})
picam2.configure(config)
picam2.start()

fb = open('/dev/fb0', 'wb')

try:
    while True:
        frame = picam2.capture_array()
        if bpp == 16:
            r = (frame[:, :, 0] >> 3).astype(np.uint16)
            g = (frame[:, :, 1] >> 2).astype(np.uint16)
            b = (frame[:, :, 2] >> 3).astype(np.uint16)
            rgb565 = (r << 11) | (g << 5) | b
            fb.seek(0)
            fb.write(rgb565.tobytes())
        else:
            xrgb = np.zeros((h, w, 4), dtype=np.uint8)
            xrgb[:, :, 0] = frame[:, :, 0]  # R
            xrgb[:, :, 1] = frame[:, :, 1]  # G
            xrgb[:, :, 2] = frame[:, :, 2]  # B
            fb.seek(0)
            fb.write(xrgb.tobytes())
finally:
    fb.close()
    picam2.stop()
