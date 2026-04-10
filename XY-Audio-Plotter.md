# XY Audio Lissajous Plotter Project

**A real-time stereo XY audio oscilloscope / Lissajous figure display**  
X = Left channel, Y = Right channel

## Project Constraints (Hard Requirements)

- **Display board**: Raspberry Pi 3B
- **Display output**: HDMI at 1024×768 resolution
- **Refresh rate**: **Exactly 24 FPS** (not higher, not lower)
- **Communication**: SPI at **exactly 16 MHz** (Pi 3B = Master, STM32 = Slave)
- **Wiring length**: Maximum **5 cm**
- **Acquisition**: Offloaded to STM32F103 (Blue Pill)
- **Audio**: Stereo passthrough (PC line-out → external amp) + sampling for XY plotting
- **Op-amp**: MCP6022 (dual rail-to-rail, 3.3 V single supply)

## System Architecture

- **PC** → 3.5 mm stereo line-out
- **Passive passthrough** → 3.5 mm output jack (to external amplifier/speakers)
- **Light tap** (680 Ω recommended) from input → MCP6022 buffering + biasing + gain
- **MCP6022** → biased & amplified signal to STM32F103 ADCs
- **STM32F103 (Blue Pill)**: Dual simultaneous ADC + DMA → prepares XY sample buffers → sends via 16 MHz SPI slave
- **Raspberry Pi 3B**: Receives buffers via spidev → draws Lissajous patterns at exactly 24 FPS using Cairo

## Analog Front-End (MCP6022)

**Gain**: **~1.65×** — maximum for 3.3 V single supply without clipping on ±1 V line-out

On a 3.3 V single supply with 1.65 V bias, the output can only swing ±1.65 V. PC line-out is ±1 V, so max linear gain = 1.65 V / 1 V = 1.65×. Higher gain clips the positive half.

**Circuit per channel (Left & Right)**:
- Input coupling capacitor: **100 nF** (blocks PC DC offset)
- Tap resistor from audio input: **680 Ω** in series to non-inverting input
- Bias: shared **10 kΩ / 10 kΩ** divider from 3.3 V → 1.65 V, connected between 680 Ω and non-inverting input; decoupled with **100 nF** to GND
- Non-inverting amplifier:
  - Rf (feedback) = **6.8 kΩ**
  - Rg (to GND) = **10 kΩ**
  - Gain = 1 + (6.8 / 10) = **1.68×**
- Output: **100 Ω** series resistor → STM32 ADC pin (current limiting / damp oscillation on capacitive ADC input)

**Decoupling**:
- 100 nF on MCP6022 supply pin to GND

**Passthrough**: purely passive — input jack wired directly to output jack, no active components in signal path

**ADC pin assignment** (for dual simultaneous mode):
- Left (X) → PA0 (ADC1 Channel 0)
- Right (Y) → PA1 (ADC2 Channel 1)

## STM32F103 (Blue Pill) Side

- Dual regular simultaneous ADC mode (ADC1 master + ADC2 slave)
- Timer-triggered sampling (recommended 96 kS/s or 192 kS/s per channel)
- Circular DMA buffer
- Every ~41.67 ms: copy latest buffer (1024 or 2048 XY pairs) and transmit via SPI1 slave + DMA at 16 MHz
- 12-bit samples, biased at 1.65 V (subtract offset in firmware or on Pi)

**Recommended buffer sizes**:
- 1024 XY pairs (~4.1 KB) — very comfortable at 16 MHz
- 2048 XY pairs (~8.2 KB) — still fine

## Raspberry Pi 3B Side

- HDMI forced to 1024×768 @ 60 Hz via `config.txt`
- `spidev` at exactly 16 MHz, mode 0, CE0 (GPIO 8)
- Cairo (pycairo) anti-aliased renderer, direct framebuffer output to `/dev/fb0`
- Precise timing loop using `time.perf_counter()` + `time.sleep()` to enforce **exactly 24 FPS**
- CRT effects: focus (line width), persistence (alpha fade), brightness

## Wiring (Pi 3B ↔ Blue Pill, ≤5 cm)

**SPI0 on Pi 3B**:
- GPIO 11 (SCLK) → PA5 (SCK)
- GPIO 10 (MOSI) → PA7 (MOSI)
- GPIO 9 (MISO)  → PA6 (MISO)
- GPIO 8 (CE0)   → PA4 (NSS/CS) or any GPIO
- GND

Add 22–33 Ω series resistors on SCLK and MOSI (Pi side) for clean edges at 16 MHz.

## Useful Online References

- SPI STM32 Slave to Raspberry Pi Master (YouTube + code): https://www.youtube.com/watch?v=fzBc2wmP3Dw
- STM32F103 SPI slave examples: https://learnbuildshare.wordpress.com/about/stm32/using-spi-as-slave/
- Dual ADC simultaneous mode examples on STM32F103
- General Blue Pill + Pi SPI projects on GitHub

## Next Steps / TODO

### Rendering
1. ~~Replace numpy point/line drawing with OpenVG~~ — dead end; replaced with Cairo
2. ~~Implement proper line segments~~ — done via Cairo anti-aliased stroke
3. ~~Verify full 1024×768 screen usage~~ — done
4. ~~Remove FPS counter print once rendering is confirmed smooth~~ — done
5. Adaptive display — detect dominant frequency and adjust sample window/zoom so the figure looks good across 20Hz–20kHz

### SPI / Data
6. ~~Wire Blue Pill to SPI CE0 (GPIO 8) and verify loopback test sketch works~~ — done; 10/10 PASS at 16 MHz
7. ~~Implement STM32 dual ADC + DMA firmware~~ — done; spi_xy_adc.ino v1.0.0
8. ~~Implement STM32 SPI slave DMA TX (send XY buffers every ~41ms)~~ — done
9. ~~Replace dummy Lissajous data with live SPI reads from Blue Pill~~ — done; 24 FPS confirmed with dummy data; real ADC firmware written, pending hardware

### Analog Front-End
10. Build MCP6022 buffering/biasing/gain circuit
11. Test signal levels and adjust gain/PC volume for best dynamic range

### Polish
12. Auto-start visualiser on boot
13. Clean up config.txt (remove unused lines)
14. ~~Update setup.sh to reflect Pi 3B as primary target~~ — done
15. Add features: persistence/fading, trigger, scale, grid options
16. Rotary encoder controls: Speed, Brightness, Persistence, Focus (trace sharpness/glow)

## Build Log

### April 2026
- Pi Zero 1 (original, **no WiFi**) and STM32 Blue Pill soldered to prototype board
- 3.3V power connected between boards
- Arduino IDE configured with STM32duino core 2.12.0 for Blue Pill firmware
- Raspberry Pi OS Lite 32-bit (Bookworm) flashed to SD card
- `config.txt` configured: 1024×768 HDMI, SPI enabled, TH WiFi region
- Pi access method: keyboard + HDMI + micro USB OTG adapter (no WiFi on this board)
- SD card bootfs access on Windows: assign drive letter via Disk Management
- ENC28J60 LAN module (Hanrun HR911105A RJ45) wired to SPI0: CE0 (GPIO 8) for CS, GPIO 25 (NT) for INT, CLK for SCK, RST tied to 3.3V; 12 MHz
- Blue Pill on CE1 (GPIO 7), shares CLK/MOSI/MISO with ENC28J60
- `enc28j60` kernel driver loaded and chip found; first module had a knocked-off inductor (no physical link) — replaced with working module
- Pi assigned 192.168.0.100 static lease; SSH access confirmed
- Pi OS updated via `apt update && apt upgrade`
- ENC28J60 MAC fixed to `b8:27:eb:00:00:01` via udev rule (overlay `mac-address` param not supported)
- `vc4-fkms-v3d` overlay enabled for framebuffer access (Pi Zero); Pi 3B uses default `vc4-kms-v3d` with `disable_fw_kms_setup=1` commented out
- Pi 3B framebuffer is 32-bit ARGB8888 — visualiser updated from RGB565 uint16 to ARGB8888 uint32
- Animated Lissajous visualiser running at 24 FPS, ~14ms frame time on Pi 3B
- Framebuffer direct write approach used (no Pygame/GPU dependency)
- Vectorised fixed-step line interpolation implemented — smooth continuous trace, no gaps
- EGL/GLES2 GPU path investigated but blocked: vc4-kms-v3d incompatible with monitor
- Cairo (pycairo 1.27.0) adopted as renderer: anti-aliased lines, 24 FPS at ~27ms worst case
- CRT effects implemented via Cairo: focus (line width), persistence (alpha fade), brightness
- `python3-cairo` added to setup.sh dependencies
- SD card: 16 GB, 12.2 GB available after OS update; filesystem expanded to fill card via `raspi-config --expand-rootfs`
- `python3-pygame` installed via apt
- Switched from Pi Zero to **Pi 3B** as primary target — built-in ethernet frees SPI CE0 for Blue Pill; ENC28J60 and udev MAC rule removed
- `setup.sh` updated for Pi 3B: ENC28J60 overlay removed, embedded visualiser updated to Cairo renderer, `python3-spidev` added to dependencies
- STM32 dummy XY sketch written (`STM_FIRMWARE/spi_xy_dummy/`): pre-computed animated Lissajous sent via SPI1 slave + DMA, 1024 XY pairs per frame, 4096 bytes big-endian uint16
- Pi SPI reader written (`PI_SETUP/spi_xy_read.py`): reads 4096 bytes from CE0, decodes to XY float arrays for visualiser
- Analog front-end design reviewed: gain corrected to **1.68×** (Rf=6.8 kΩ, Rg=10 kΩ) for 3.3 V single supply — original 4× gain would clip on positive half with ±1 V line-out
- Component list confirmed: 2× 100 nF input coupling caps, 2× 680 Ω series resistors, shared 10k/10k bias divider + 100 nF decoupling, 100 nF MCP6022 supply decoupling, 2× 100 Ω output resistors to STM32 ADC
- Pi 3B eth0 static IP set to 192.168.0.100 via NetworkManager; MAC b8:27:eb:53:fc:5a fixed in hardware — assigned static lease on router
- SPI link test sketches written: `STM_FIRMWARE/spi_loopback_ce0/` (Blue Pill) + `PI_SETUP/spi_link_test.py` (Pi); test file transferred to Pi
- SPI link test result: 10/10 PASS at 16 MHz; polling TX failed at 16 MHz — DMA TX+RX required
- `spi_xy_dummy.ino` v0.0.2 flashed: LUT-based animated Lissajous via SPI1 slave DMA, 1024 XY pairs per frame
- `visualiser.py` reading live STM32 dummy data via SPI: 24 FPS confirmed on HDMI
- Consolidated to single production app: all test/demo/dummy files removed
  - `PI_SETUP/visualiser.py`: SPI code inlined, self-contained
  - `STM_FIRMWARE/spi_xy_adc/spi_xy_adc.ino` v1.0.0: dual regular ADC (TIM3 96 kS/s trigger), ADC1 DMA 32-bit (both channels from ADC1->DR), SPI1 slave DMA TX
  - Real ADC firmware ready; awaiting MCP6022 analog front-end build

### Notes
- STM32 SPI slave uses direct register access throughout (STM32duino SPI slave API unreliable at 16 MHz)
- Polling SPI slave TX byte-by-byte always fails at 16 MHz — DMA TX+RX mandatory

---

**Project Status**: Active Build — firmware complete, analog hardware build next  
**Last Updated**: April 2026

This document contains all key decisions and specifications discussed so far. Feel free to expand it with code snippets, schematics, or test results as you build.