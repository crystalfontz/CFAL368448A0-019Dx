# CFAL368448A0-019Dx Arduino Demo

Demo code for the Crystalfontz CFAL368448A0-019Dx 1.93" AMOLED displays,
targeting a [Seeeduino v4.2](https://www.crystalfontz.com/product/cfapn15062)
(ATmega328P) with a bit-banged QSPI interface.

## Products

| Part Number | Description | Link |
| --- | --- | --- |
| CFAL368448A0-019DN | 1.93" AMOLED, 368×448, no touch | https://www.crystalfontz.com/product/cfal368448a0019dn |
| CFAL368448A0-019DC | 1.93" AMOLED, 368×448, capacitive touch (CST816) | https://www.crystalfontz.com/product/cfal368448a0019dc |
| CFAPN15062 | Seeeduino v4.2 (recommended dev board) | https://www.crystalfontz.com/product/cfapn15062 |

## Overview

- **Display:** 368×448 pixels, RGB565 color
- **Driver IC:** CH13620
- **Interface:** QSPI (bit-banged, all signals on PORTC/PORTD/PORTB)
- **MCU:** ATmega328P — tested on Seeeduino v4.2 **set to 3.3 V** (required)

## Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) 1.8 or 2.x
- Board set to **Seeeduino v4.2** (or Arduino Uno) at **3.3 V** — this is critical; 5 V will damage the display
- For `SD_DEMO`: Arduino SD library (bundled with the IDE), micro SD card formatted FAT32
- For `LOGO_DEMO`: `cfa_logo.h` must be present in the sketch folder
- For image conversion: Python 3 with Pillow (`pip install Pillow`)

## Wiring

All QSPI signals share PORTC so all four data lines can be written in a single
register operation. The SD card (optional) uses the hardware SPI peripheral on
PORTB and does not conflict.

### Display (all variants)

| Arduino Pin | AVR | Signal | Direction |
| --- | --- | --- | --- |
| A0 | PC0 | QSPI IO0 / SDI | Out |
| A1 | PC1 | QSPI IO1 / DCX | Out |
| A2 | PC2 | QSPI IO2 | Out |
| A3 | PC3 | QSPI IO3 | Out |
| D5 | PD5 | QSPI CLK | Out |
| D8 | PB0 | Display Reset (active low) | Out |
| D9 | PB1 | Display CS (active low) | Out |

### SD Card (SD_DEMO only)

| Arduino Pin | AVR | Signal | Direction |
| --- | --- | --- | --- |
| D10 | PB2 | SD CS (active low) | Out |
| D11 | PB3 | SD MOSI (hardware SPI) | Out |
| D12 | PB4 | SD MISO (hardware SPI) | In |
| D13 | PB5 | SD SCK (hardware SPI) | Out |

### Capacitive Touch (CFAL368448A0-019DC only)

| Arduino Pin | AVR | Signal | Direction |
| --- | --- | --- | --- |
| D6 | PD6 | CST816 Reset (active low) | Out |
| D7 | PD7 | CST816 INT (active low) | In |
| A4 | PC4 | I2C SDA (hardware TWI) | In/Out |
| A5 | PC5 | I2C SCL (hardware TWI) | Out |

## Quick Start

1. Clone or download this repository.
2. Open `CFAL368448A0-019Dx/CFAL368448A0-019Dx.ino` in the Arduino IDE.
3. In `CFAL368448A0-019Dx.h`, set `TOUCH_TYPE` to match your hardware:

   ```c
   #define TOUCH_TYPE  TOUCH_TYPE_NONE   // CFAL368448A0-019DN (no touch)
   #define TOUCH_TYPE  TOUCH_TYPE_CAP    // CFAL368448A0-019DC (capacitive touch)
   ```

4. Enable or disable demos using the feature flags (see below).
5. Select **Seeeduino v4.2** (or Arduino Uno) as the board, confirm the board
   voltage is set to **3.3 V**, select the correct COM port, and upload.

## Demo Feature Flags

Edit the flags near the top of `CFAL368448A0-019Dx.h`. The ATmega328P has
32 KB of flash — watch the budget when enabling multiple demos.

| Flag | Default | Flash | Description |
| --- | --- | --- | --- |
| `COLOR_DEMO` | 1 | <1 KB | Solid color cycle: red, green, blue, white |
| `CIRCLES_DEMO` | 1 | ~2 KB | Concentric circles (midpoint circle algorithm) |
| `LINES_DEMO` | 0 | ~2 KB | Line fan from center (Bresenham's algorithm) |
| `CHECKER_DEMO` | 1 | ~1 KB | 16×16 color checkerboard |
| `EXPANDING_DEMO` | 1 | ~1 KB | Row of expanding concentric circles |
| `LOGO_DEMO` | 0 | ~15 KB | Crystalfontz logo from PROGMEM (needs `cfa_logo.h`) |
| `SD_DEMO` | 0 | ~10 KB | BMP images from micro SD card |

`WAIT_TIME` (default 2000 ms) controls the pause between demo screens.

### Recommended Combinations

```
Drawing demos only   COLOR=1  CIRCLES=1  LINES=1  CHECKER=1  EXPANDING=1  LOGO=0  SD=0
Logo + colors        COLOR=1  CIRCLES=0  LINES=0  CHECKER=0  EXPANDING=0  LOGO=1  SD=0
SD card images       COLOR=1  CIRCLES=0  LINES=0  CHECKER=0  EXPANDING=0  LOGO=0  SD=1
```

## SD Card Image Preparation

`SD_DEMO` reads 16-bit RGB565 BMP files from the root of a FAT32-formatted
micro SD card. Use the included `convert_bmp.py` script to prepare images.

Requirements: Python 3, Pillow (`pip install Pillow`)

Convert a single image:

```
python convert_bmp.py photo.jpg photo.bmp
```

Convert all JPGs and PNGs in a folder (Windows):

```
convert_bmp.bat
```

The script scales and letterboxes the source image to 368×448 with black
borders, then writes a top-down, little-endian RGB565 BMP. Copy the resulting
`.bmp` files to the root of the SD card.

## Touch Variant (CFAL368448A0-019DC)

Set `#define TOUCH_TYPE TOUCH_TYPE_CAP` in the header to enable the CST816
driver. The touch controller communicates over I2C at address `0x15`. A small
close-button overlay is drawn in the top-right corner of each demo screen;
tap it to advance to the next demo.

## License

This software is released into the public domain under [The Unlicense](LICENSE).
