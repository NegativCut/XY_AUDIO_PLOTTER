# XY Audio Lissajous Plotter Project

**A real-time stereo XY audio oscilloscope / Lissajous figure display**  
X = Left channel, Y = Right channel

## Project Constraints (Hard Requirements)

- **Display board**: Raspberry Pi Zero (1st generation, single-core 1 GHz)
- **Display output**: HDMI at 1024×768 resolution
- **Refresh rate**: **Exactly 24 FPS** (not higher, not lower)
- **Communication**: SPI at **exactly 16 MHz** (Pi Zero = Master, STM32 = Slave)
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
- **Raspberry Pi Zero**: Receives buffers via spidev → draws Lissajous patterns at exactly 24 FPS using Pygame

## Analog Front-End (MCP6022)

**Recommended gain**: **4×** (sweet spot for typical PC line-out ~1–2 Vpp)

**Circuit per channel (Left & Right)**:
- Input coupling capacitor: 100 nF
- Bias: 10 kΩ / 10 kΩ divider from 3.3 V → 1.65 V (decoupled with 100 nF)
- Tap resistor from audio input: **680 Ω**
- Non-inverting amplifier:
  - Rf (feedback) = **15 kΩ** + **12 nF** parallel (≈30 kHz low-pass)
  - Rg (to GND) = **4.7 kΩ**
  - Gain ≈ 4.19×
- Output: 100 Ω series resistor → STM32 ADC pin

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

## Raspberry Pi Zero Side

- HDMI forced to 1024×768 @ 60 Hz via `config.txt`
- `spidev` at exactly 16 MHz, mode 0
- Pygame with double buffering + partial/dirty rect updates
- Precise timing loop using `time.perf_counter()` + `time.sleep()` to enforce **exactly 24 FPS**
- Static background (grid + axes) blitted every frame
- Draw trace with `pygame.draw.lines()` or point plotting for classic Lissajous look

## Wiring (Pi Zero ↔ Blue Pill, ≤5 cm)

**SPI0 on Pi Zero**:
- GPIO 11 (SCLK) → PA5 (SCK)
- GPIO 10 (MOSI) → PA7 (MOSI)
- GPIO 9 (MISO)  → PA6 (MISO)
- GPIO 8 (CE0)   → PA4 (NSS/CS) or any GPIO
- Multiple GND wires (strongly recommended)

Add 22–33 Ω series resistors on SCLK and MOSI (Pi side) for clean edges at 16 MHz.

## Useful Online References

- SPI STM32 Slave to Raspberry Pi Master (YouTube + code): https://www.youtube.com/watch?v=fzBc2wmP3Dw
- STM32F103 SPI slave examples: https://learnbuildshare.wordpress.com/about/stm32/using-spi-as-slave/
- Dual ADC simultaneous mode examples on STM32F103
- General Blue Pill + Pi SPI projects on GitHub

## Next Steps / TODO

- Build analog passthrough + MCP6022 board
- Implement STM32 dual ADC + DMA + SPI slave DMA TX
- Implement Pi Zero Pygame exact 24 FPS receiver + Lissajous renderer
- Test signal levels and adjust gain/PC volume for best dynamic range
- Add features: persistence/fading, trigger, scale, grid options

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
- SD card: 16 GB, 12.2 GB available after OS update; filesystem expanded to fill card via `raspi-config --expand-rootfs`
- `python3-pygame` installed via apt

### Notes
- Pi Zero (no W) has no WiFi — all setup and deployment must be done via direct HDMI/keyboard or by editing SD card on PC
- STM32 SPI slave sketch uses direct register setup (STM32duino SPI slave API unreliable) — see `STM_FIRMWARE/`

---

**Project Status**: Active Build  
**Last Updated**: April 2026

This document contains all key decisions and specifications discussed so far. Feel free to expand it with code snippets, schematics, or test results as you build.