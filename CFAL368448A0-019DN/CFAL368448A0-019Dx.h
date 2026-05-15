#ifndef CFAL368448A0_019DN_H
#define CFAL368448A0_019DN_H
//==============================================================================
//
//  CRYSTALFONTZ CFAL368448A0-019DN / CFAL368448A0-019DC
//  1.93" AMOLED Display, 368x448 pixels
//  Driver IC:  CH13620
//  Interface:  QSPI (bit-banged)
//
//  CFAL368448A0-019DN -- No touchscreen
//  CFAL368448A0-019DC -- Capacitive touchscreen (implementation pending)
//
//  Code written for Seeeduino v4.2 set to 3.3V (IMPORTANT!)
//    https://www.crystalfontz.com/product/cfapn15062
//
//  https://www.crystalfontz.com/product/cfal368448a0019dn
//  https://www.crystalfontz.com/product/cfal368448a0019dc
//
//==============================================================================
//
//  2025-05-14 Crystalfontz
//
//==============================================================================
//This is free and unencumbered software released into the public domain.
//
//Anyone is free to copy, modify, publish, use, compile, sell, or
//distribute this software, either in source code form or as a compiled
//binary, for any purpose, commercial or non-commercial, and by any means.
//
//In jurisdictions that recognize copyright laws, the author or authors
//of this software dedicate any and all copyright interest in the software
//to the public domain. We make this dedication for the benefit of the
//public at large and to the detriment of our heirs and successors. We
//intend this dedication to be an overt act of relinquishment in perpetuity
//of all present and future rights to this software under copyright law.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
//IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
//CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//For more information, please refer to <http://unlicense.org/>
//==============================================================================

//==============================================================================
// VARIANT SELECTION
//
// Set TOUCH_TYPE to match the display variant in hand:
//   TOUCH_TYPE_NONE  =>  CFAL368448A0-019DN (no touch)
//   TOUCH_TYPE_CAP   =>  CFAL368448A0-019DC (capacitive touch)
//
// Cap touch implementation is not yet included; setting TOUCH_TYPE_CAP
// reserves the compile-time flag so it can be filled in later.
//==============================================================================
#define TOUCH_TYPE_NONE  (0)
#define TOUCH_TYPE_CAP   (1)

#define TOUCH_TYPE  TOUCH_TYPE_NONE

//==============================================================================
// DEMO FEATURE FLAGS
//
// Set each flag to 1 to include that demo, 0 to exclude it.
//
// FLASH BUDGET -- ATmega328P has 32 KB total flash:
//   COLOR_DEMO     :  <1 KB
//   CIRCLES_DEMO   : ~2 KB
//   LINES_DEMO     : ~2 KB
//   CHECKER_DEMO   : ~1 KB
//   EXPANDING_DEMO : ~1 KB
//   LOGO_DEMO      : ~15 KB  (PROGMEM bitmap array in cfa_logo.h)
//   SD_DEMO        : ~10 KB  (SD + FAT libraries)
//
// LOGO_DEMO and SD_DEMO together consume ~25 KB, leaving very little room
// for drawing demos. Recommended combinations:
//
//   Drawing demos only:  COLOR=1 CIRCLES=1 LINES=1 CHECKER=1 EXPANDING=1
//                        LOGO=0 SD=0
//   Logo + colors:       COLOR=1 CIRCLES=0 LINES=0 CHECKER=0 EXPANDING=0
//                        LOGO=1 SD=0
//   SD card images:      COLOR=1 CIRCLES=0 LINES=0 CHECKER=0 EXPANDING=0
//                        LOGO=0 SD=1
//==============================================================================
#define COLOR_DEMO      (0)  // Solid color cycle: red, green, blue, white
#define CIRCLES_DEMO    (0)  // Multiple circles (Midpoint circle algorithm)
#define LINES_DEMO      (0)  // Line fan from center (Bresenham's algorithm)
#define CHECKER_DEMO    (0)  // 16x16 color checkerboard
#define EXPANDING_DEMO  (0)  // Row of expanding concentric circles
#define LOGO_DEMO       (0)  // CFA logo from flash  (requires cfa_logo.h)
#define SD_DEMO         (1)  // BMP images from micro SD card

// Milliseconds to pause between demo screens
#define WAIT_TIME  (2000)

//==============================================================================
// Enable the SD library whenever SD_DEMO is active.
#if (0 != SD_DEMO)
  #define BUILD_SD  (1)
#else
  #define BUILD_SD  (0)
#endif

//==============================================================================
// DISPLAY RESOLUTION
//==============================================================================
#define DISP_WIDTH   (368)
#define DISP_HEIGHT  (448)

//==============================================================================
// COLOR CONSTANTS  (RGB565, big-endian word)
//==============================================================================
#define COLOR_BLACK    (0x0000)
#define COLOR_RED      (0xF800)
#define COLOR_GREEN    (0x07E0)
#define COLOR_BLUE     (0x001F)
#define COLOR_CYAN     (0x07FF)
#define COLOR_MAGENTA  (0xF81F)
#define COLOR_YELLOW   (0xFFE0)
#define COLOR_WHITE    (0xFFFF)
#define COLOR_ORANGE   (0xFD20)

//==============================================================================
// PORT PIN MAPPING  (Seeeduino v4.2 / ATmega328P)
//
// All QSPI signal pins are on PORTC (analog header A0-A4).  Keeping them on
// a dedicated port means all four IO lines can be written in a single register
// assignment, and there is no conflict with the hardware SPI peripheral used
// by the SD library (which lives on PORTB: D11=MOSI, D12=MISO, D13=SCK).
//
//  Arduino | AVR  | Port | Signal
//  --------+------+------+-------------------------------------------
//    A0    |  PC0 |  C   | QSPI IO0 / SDI  (single-wire command mode)
//    A1    |  PC1 |  C   | QSPI IO1 / DCX
//    A2    |  PC2 |  C   | QSPI IO3 / D1
//    A3    |  PC3 |  C   | QSPI IO2 / D0
//    A4    |  PC4 |  C   | QSPI CLK
//  --------+------+------+-------------------------------------------
//    D11   |  PB3 |  B   | SD MOSI  (hardware SPI -- display not connected)
//    D12   |  PB4 |  B   | SD MISO  (hardware SPI -- display not connected)
//    D13   |  PB5 |  B   | SD SCK   (hardware SPI -- display not connected)
//  --------+------+------+-------------------------------------------
//    D4    |  PD4 |  D   | SD card chip select (active low)
//    D6    |  PD6 |  D   | Display chip select  (active low)
//    D7    |  PD7 |  D   | Display reset        (active low)
//
// Note: A4 (PC4) doubles as the I2C SDA pin.  If cap touch is added on a
// future variant using I2C, move the CLK signal to A5 and reassign
// accordingly.
//==============================================================================

// PORTC: set PC0-PC4 as outputs; leave PC5-PC7 alone.
#define PORTC_OUT_MASK  (0x1F)
// PORTD: set PD4, PD6, PD7 as outputs; leave all other PORTD bits alone.
#define PORTD_OUT_MASK  (0xF0)
// PORTB: set PB1, PB2, PB0 as outputs; leave all other PORTB bits alone.
#define PORTB_OUT_MASK  (0x07)

// PORTC bit masks  (QSPI signals)
#define SDI_MASK  (0x01)   // PC0, A0 -- QSPI IO0
#define DCX_MASK  (0x02)   // PC1, A1 -- QSPI IO1
#define D1_MASK   (0x04)   // PC2, A2 -- QSPI IO3
#define D0_MASK   (0x08)   // PC3, A3 -- QSPI IO2
#define CLK_MASK  (0x20)   // PC4, A4 -- QSPI CLK

// PORTD bit masks
#define SDCS_MASK  (0x04)  // PB2, D10 -- SD card chip select
#define CS_MASK    (0x02)  // PB1, D9  -- display chip select
#define RST_MASK   (0x01)  // PB0, D8  -- display reset

// Direct port manipulation macros
#define CLR_SDI   (PORTC &= ~SDI_MASK)
#define SET_SDI   (PORTC |=  SDI_MASK)
#define CLR_DCX   (PORTC &= ~DCX_MASK)
#define SET_DCX   (PORTC |=  DCX_MASK)
#define CLR_D0    (PORTC &= ~D0_MASK)
#define SET_D0    (PORTC |=  D0_MASK)
#define CLR_D1    (PORTC &= ~D1_MASK)
#define SET_D1    (PORTC |=  D1_MASK)
#define CLR_CLK   (PORTD &= ~CLK_MASK)
#define SET_CLK   (PORTD |=  CLK_MASK)
#define CLR_CS    (PORTB &= ~CS_MASK)
#define SET_CS    (PORTB |=  CS_MASK)
#define CLR_RST   (PORTB &= ~RST_MASK)
#define SET_RST   (PORTB |=  RST_MASK)
#define CLR_SDCS  (PORTB &= ~SDCS_MASK)
#define SET_SDCS  (PORTB |=  SDCS_MASK)

//==============================================================================
// HOW TO CREATE A COMPATIBLE BMP IMAGE FOR THE SD CARD
//
// Save the script below as "convert_bmp.py" and run it with Python 3.
// Requires the Pillow library:  pip install Pillow
//
// Usage:  python convert_bmp.py input.jpg
//   Scales and pads the input to 368x448 with black letterbox/pillarbox
//   borders, then writes IMAGE.BMP as a 16-bit RGB565 top-down BMP.
//   Copy IMAGE.BMP (and any other .BMP files you like) to the SD card root.
//
//--- convert_bmp.py ---------------------------------------------------
// import sys, struct
// from PIL import Image
//
// TARGET_W, TARGET_H = 368, 448
// OUT_FILE = "IMAGE.BMP"
//
// img = Image.open(sys.argv[1]).convert("RGB")
// img.thumbnail((TARGET_W, TARGET_H), Image.LANCZOS)
// canvas = Image.new("RGB", (TARGET_W, TARGET_H), (0, 0, 0))
// ox = (TARGET_W - img.width)  // 2
// oy = (TARGET_H - img.height) // 2
// canvas.paste(img, (ox, oy))
// pixels = canvas.load()
//
// stride = (TARGET_W * 2 + 3) & ~3       # row stride padded to 4 bytes
// data_offset = 14 + 40 + 12             # file + info + bitfield headers
// file_size = data_offset + stride * TARGET_H
//
// with open(OUT_FILE, "wb") as f:
//   # BITMAPFILEHEADER (14 bytes)
//   f.write(b"BM")
//   f.write(struct.pack("<I", file_size))
//   f.write(struct.pack("<HH", 0, 0))
//   f.write(struct.pack("<I", data_offset))
//   # BITMAPINFOHEADER (40 bytes)
//   f.write(struct.pack("<I", 40))
//   f.write(struct.pack("<i", TARGET_W))
//   f.write(struct.pack("<i", -TARGET_H)) # negative = top-down row order
//   f.write(struct.pack("<H", 1))
//   f.write(struct.pack("<H", 16))        # 16 bits per pixel
//   f.write(struct.pack("<I", 3))         # BI_BITFIELDS compression
//   f.write(struct.pack("<I", stride * TARGET_H))
//   f.write(struct.pack("<ii", 2835, 2835))
//   f.write(struct.pack("<II", 0, 0))
//   # RGB565 channel masks (12 bytes)
//   f.write(struct.pack("<I", 0xF800))    # red   mask
//   f.write(struct.pack("<I", 0x07E0))    # green mask
//   f.write(struct.pack("<I", 0x001F))    # blue  mask
//   # Pixel data (top-down, little-endian RGB565)
//   for y in range(TARGET_H):
//     row = b""
//     for x in range(TARGET_W):
//       r, g, b = pixels[x, y]
//       row += struct.pack("<H", ((r>>3)<<11)|((g>>2)<<5)|(b>>3))
//     while len(row) % 4: row += b"\x00"
//     f.write(row)
// print(f"Saved {OUT_FILE}")
//----------------------------------------------------------------------

#endif // CFAL368448A0_019DN_H
