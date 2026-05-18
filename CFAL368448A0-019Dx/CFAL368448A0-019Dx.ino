//==============================================================================
//
//  CRYSTALFONTZ CFAL368448A0-019DN / CFAL368448A0-019DC
//  1.93" AMOLED Display, 368x448 pixels
//  Driver IC:  CH13620
//  Interface:  QSPI (bit-banged)
//
//  CFAL368448A0-019DN - No touchscreen
//  CFAL368448A0-019DC - Capacitive touchscreen 
//
//  Code written for Seeeduino v4.2 set to 3.3V (IMPORTANT!)
//    https://www.crystalfontz.com/product/cfapn15062
//
//  https://www.crystalfontz.com/product/cfal368448a0019dn
//  https://www.crystalfontz.com/product/cfal368448a0019dc
//
//==============================================================================
//
//  2026-05-18 Crystalfontz
//
//==============================================================================
// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the software
// to the public domain. We make this dedication for the benefit of the
// public at large and to the detriment of our heirs and successors. We
// intend this dedication to be an overt act of relinquishment in perpetuity
// of all present and future rights to this software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <http://unlicense.org/>
//==============================================================================

// Display FPC to Crystalfontz CFA10102 breakout to Seeeduino v4.2:
//
// |  FPC Pin       | Seeeduino | Signal                            |
// |----------------+-----------+-----------------------------------|
// |  1  (D3/IO3)   |    A2     | QSPI IO3                          |
// |  2  (D2/IO1)   |    A1     | QSPI IO1 / DCX                    |
// |  3  (D0/IO0)   |    A0     | QSPI IO0 / SDI (single-wire cmds) |
// |  4  (RST)      |    D8     | Display reset (active low)        |
// |  5  (CLK)      |    D5     | QSPI clock                        |
// |  6  (D1/IO2)   |    A3     | QSPI IO2                          |
// |  7  (CS)       |    D9     | Display chip select (active low)  |
// |  8 / 19 (GND)  |    GND    | Ground                            |
// | 10  (VBAT)     |    3.3V   | Power (via CFA10102 LV jumper)    |
// | 11  (VCI_EN)   |    3.3V   | Power (via CFA10102 LV jumper)    |
//
// microSD card -- use CFA10112 adapter (https://www.crystalfontz.com/product/cfa10112):
//
// | microSD Pin    | Seeeduino | Signal          |
// |----------------+-----------+-----------------|
// |  CS            |    D10    | SD chip select  |
// |  MOSI          |    D11    | SD MOSI         |
// |  MISO          |    D12    | SD MISO         |
// |  SCK           |    D13    | SD clock        |
// |  VCC           |    3.3V   | Power           |
// |  GND           |    GND    | Ground          |
//
// The display QSPI bus (A0-A3 / PORTC, D5 / PORTD) and the SD hardware SPI bus
// (D11-D13 / PORTB) are on separate ports with no shared pins.
//
// Cap touch (CST816) -- CFAL368448A0-019DC only:
// | Touch Pin      | Seeeduino | Signal                            |
// |----------------+-----------+-----------------------------------|
// |  RST           |    D6     | CST816 reset (active low)         |
// |  INT           |    D7     | CST816 interrupt (active low)     |
// |  SDA           |    A4     | I2C data (hardware TWI)           |
// |  SCL           |    A5     | I2C clock (hardware TWI)          |
//
//==============================================================================

#include <Arduino.h>
#include <avr/pgmspace.h>
#include "CFAL368448A0-019Dx.h"

#if (0 != LOGO_DEMO)
#include "cfa_logo.h" // PROGMEM RGB565 array; defines LOGO_WIDTH, LOGO_HEIGHT
#endif

#if BUILD_SD
#include <SD.h>
#endif

#if (TOUCH_TYPE == TOUCH_TYPE_CAP)
#include <Wire.h>
#endif

//==============================================================================
// LOW-LEVEL BIT-BANG SPI
//==============================================================================

//------------------------------------------------------------------------------
// Send one byte MSB-first on SDI only (single-wire command/address phase).
//
// The CH13620 uses a modified SPI protocol: command and address bytes travel
// on IO0 (SDI) alone in single-wire mode, while pixel data is clocked out
// across all four IO lines in quad mode.  DCX, D0, and D1 must remain LOW
// throughout single-wire transfers so the controller sees a clean signal.
//------------------------------------------------------------------------------
void spiSendByte(uint8_t data)
{
  for (int8_t i = 7; i >= 0; i--)
  {
    if ((data >> i) & 0x01)
      SET_SDI;
    else
      CLR_SDI;
    SET_CLK;
    CLR_CLK;
  }
}

//------------------------------------------------------------------------------
// Send one byte in QSPI quad mode (two 4-bit nibbles, high nibble first).
//
// Four IO lines carry one nibble per clock edge, so one byte needs two clocks.
//
// IO line to PORTC bit mapping:
//   IO0 (SDI) = PC0    IO1 (DCX) = PC1    IO2 (D0) = PC3    IO3 (D1) = PC2
//
// Nibble bit-to-IO assignment (matches CH13620 QSPI bit order):
//   data bit 7 -> IO3 (PC2)    data bit 6 -> IO2 (PC3)
//   data bit 5 -> IO1 (PC1)    data bit 4 -> IO0 (PC0)  [high nibble]
//   data bit 3 -> IO3 (PC2)    data bit 2 -> IO2 (PC3)
//   data bit 1 -> IO1 (PC1)    data bit 0 -> IO0 (PC0)  [low nibble]
//
// CLK is PD5 (D5), separate from PORTC.  The 0xF0 mask preserves PC4-PC7
// while writing all four IO lines in a single register assignment.
//------------------------------------------------------------------------------
void qspiSendByte(uint8_t data)
{
  uint8_t portval;

  // High nibble: data[7:4] -> PC[3,2,1,0]
  portval = ((data >> 4) & 0x0f);
  PORTC = (PORTC & 0xF0) | portval;
  SET_CLK;
  CLR_CLK;

  // Low nibble: data[3:0] -> PC[3,2,1,0]
  portval = (data & 0x0f);
  PORTC = (PORTC & 0xF0) | portval;
  SET_CLK;
  CLR_CLK;
}

//==============================================================================
// CH13620 COMMAND HELPERS
//
// The CH13620 QSPI command protocol wraps every transfer in a 4-byte header:
//   Byte 0: 0x02 (write command)  or 0x32 (write data/pixels)
//   Byte 1: 0x00 (address high)
//   Byte 2: command / register byte
//   Byte 3: 0x00 (address low / dummy)
// followed by zero or more data bytes, all clocked on SDI in single-wire mode.
// Pixel data uses the 0x32 header and is clocked in quad mode.
//==============================================================================
void writeCommand(uint8_t cmd)
{
  PORTC &= 0xF0;  // clear IO0-IO3 before CS asserts
  CLR_CS;
  spiSendByte(0x02);
  spiSendByte(0x00);
  spiSendByte(cmd);
  spiSendByte(0x00);
  SET_CS;
}

void writeCommandParam(uint8_t cmd, uint8_t param)
{
  PORTC &= 0xF0;  // clear IO0-IO3 before CS asserts
  CLR_CS;
  spiSendByte(0x02);
  spiSendByte(0x00);
  spiSendByte(cmd);
  spiSendByte(0x00);
  spiSendByte(param);
  SET_CS;
}

void writeCommandData(uint8_t cmd, const uint8_t *data, uint8_t len)
{
  PORTC &= 0xF0;  // clear IO0-IO3 before CS asserts
  CLR_CS;
  spiSendByte(0x02);
  spiSendByte(0x00);
  spiSendByte(cmd);
  spiSendByte(0x00);
  for (uint8_t i = 0; i < len; i++)
    spiSendByte(data[i]);
  SET_CS;
}

//==============================================================================
// Set the display write window (Column Address Set / Row Address Set).
// Coordinates are 0-based inclusive: x0..x1, y0..y1.
//==============================================================================
void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  uint8_t col[] = {(uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
                   (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)};
  writeCommandData(0x2A, col, 4);

  uint8_t row[] = {(uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
                   (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)};
  writeCommandData(0x2B, row, 4);
}

//==============================================================================
// Hardware reset: toggle RST low for 20 ms then wait for the controller to
// complete its internal power-on sequence (120 ms minimum).
//==============================================================================
void hardwareReset()
{
  SET_RST;
  delay(50);
  CLR_RST;
  delay(20);
  SET_RST;
  delay(120);
}

//==============================================================================
// CH13620 full initialization sequence
//
// The CH13620 uses vendor-specific "page" registers (0xF0) to select its
// internal register banks (pages 0x50-0x55).  The values below were supplied
// by the panel manufacturer and set up all analog, power, timing, and gamma
// parameters for the 368x448 AMOLED panel.
//
// The well-known MIPI DCS commands -- 0x2A (CASET), 0x2B (RASET), 0x35
// (TEON), 0x3A (COLMOD), 0x11 (SLPOUT), 0x13 (NORON), 0x29 (DISPON) --
// are called out with comments. All other register values are panel-specific
// and should not be altered without guidance from the manufacturer.
//==============================================================================
void initDisplay()
{
  // -----------------------------------------------------------------------
  // Page 0x50: Interface timing, display control, and gate/source driver
  // -----------------------------------------------------------------------
  writeCommandParam(0xF0, 0x50);

  // Interface timing control
  writeCommandData(0xB1, (const uint8_t[]){0x58, 0x72}, 2);

  // 2Ah (CASET): initialise full-width column window (0..367)
  writeCommandData(0x2A, (const uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4);

  // 2Bh (RASET): initialise full-height row window (0..447)
  writeCommandData(0x2B, (const uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4);

  // 35h (TEON): Tearing Effect Line ON -- V-blanking signal only
  writeCommandParam(0x35, 0x00);

  // 3Ah (COLMOD): Interface Pixel Format -- 0x05 = 16-bit RGB565
  writeCommandParam(0x3A, 0x05);

  writeCommandParam(0x64, 0x00);
  writeCommandParam(0x67, 0x01);
  writeCommandParam(0x68, 0x01);
  writeCommandParam(0xF0, 0x50);
  writeCommandData(0xB0, (const uint8_t[]){0x08, 0x08, 0x1E}, 3);
  writeCommandParam(0xB4, 0x00);
  writeCommandParam(0xB5, 0x00);
  writeCommandParam(0xB6, 0xAE);
  writeCommandData(0xB8, (const uint8_t[]){0x00, 0x00}, 2);
  writeCommandParam(0xBE, 0x91);
  writeCommandData(0xD6, (const uint8_t[]){0x01, 0x43, 0x01, 0x43}, 4);
  writeCommandData(0xB7, (const uint8_t[]){0x00, 0x50}, 2);
  writeCommandData(0xBB, (const uint8_t[]){0xAA, 0x88}, 2);
  writeCommandData(0xBC, (const uint8_t[]){0x44, 0x20, 0x10, 0x44, 0x20}, 5);
  writeCommandData(0xC0, (const uint8_t[]){0x06, 0x00, 0x23, 0x00, 0x02, 0x17, 0x04, 0x17, 0x04, 0x17, 0x04, 0x17, 0x04, 0x17, 0x04, 0x17}, 16);
  writeCommandData(0xC1, (const uint8_t[]){0x56, 0x34, 0x12, 0x65, 0x43, 0x21}, 6);
  writeCommandData(0xC2, (const uint8_t[]){0x65, 0x43, 0x21, 0x56, 0x34, 0x12}, 6);
  writeCommandData(0xC3, (const uint8_t[]){0x36, 0x63, 0x25, 0x52, 0x14, 0x41}, 6);
  writeCommandData(0xC4, (const uint8_t[]){0x63, 0x36, 0x52, 0x25, 0x41, 0x14}, 6);
  writeCommandParam(0xC5, 0x90);
  writeCommandData(0xC6, (const uint8_t[]){0x06, 0x00, 0x23, 0x00, 0x02, 0x17, 0x04, 0x17, 0x04, 0x17, 0x04, 0x17, 0x04, 0x17, 0x04, 0x17}, 16);
  writeCommandParam(0xD0, 0x80);
  writeCommandData(0xD8, (const uint8_t[]){0x43, 0x05}, 2);

  // -----------------------------------------------------------------------
  // Page 0x51: Power supply, VCOM, and pixel amplifier settings
  // -----------------------------------------------------------------------
  writeCommandParam(0xF0, 0x51);
  writeCommandData(0xB3, (const uint8_t[]){0x20, 0x20}, 2);
  writeCommandParam(0xB4, 0x66);
  writeCommandParam(0xB5, 0x66);
  writeCommandData(0xB9, (const uint8_t[]){0x47, 0x3B}, 2);
  writeCommandData(0xBA, (const uint8_t[]){0x13, 0x13, 0x13}, 3);
  writeCommandData(0xBB, (const uint8_t[]){0x83, 0x13}, 2);
  writeCommandData(0xBC, (const uint8_t[]){0x03, 0x03}, 2);
  writeCommandData(0xBD, (const uint8_t[]){0x15, 0x15}, 2);
  writeCommandParam(0xBE, 0x10);
  writeCommandParam(0xC0, 0x1F);
  writeCommandData(0xC1, (const uint8_t[]){0x30, 0x30, 0x30}, 3);
  writeCommandParam(0xC2, 0x09);
  writeCommandData(0xC3, (const uint8_t[]){0x07, 0xBB}, 2);
  writeCommandParam(0xC4, 0x00);
  writeCommandData(0xC9, (const uint8_t[]){0xFF, 0xFF, 0x01, 0x01}, 4);
  writeCommandParam(0xCB, 0x23);
  writeCommandData(0xCC, (const uint8_t[]){0x00, 0x22, 0x70, 0x94, 0x94, 0x00, 0x00}, 7);
  writeCommandData(0xCD, (const uint8_t[]){0x00, 0x00, 0x00, 0x14}, 4);
  writeCommandData(0xCF, (const uint8_t[]){0x1E, 0x22}, 2);
  writeCommandData(0xD0, (const uint8_t[]){0x04, 0x09, 0x0E, 0x09, 0x0E, 0x13, 0x13, 0x18, 0x18, 0x01}, 10);
  writeCommandData(0xD1, (const uint8_t[]){0x0F, 0x2F}, 2);
  writeCommandData(0xD2, (const uint8_t[]){0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}, 7);
  writeCommandData(0xD3, (const uint8_t[]){0x73, 0x65}, 2);
  writeCommandData(0xD4, (const uint8_t[]){0x2F, 0x1F, 0x1F, 0x2F, 0x1F, 0x1F, 0x1F, 0x22, 0x01}, 9);
  writeCommandData(0xD6, (const uint8_t[]){0xA0, 0x55, 0x55, 0x00}, 4);
  writeCommandData(0xD7, (const uint8_t[]){0xA0, 0x55, 0x55, 0x00}, 4);
  writeCommandData(0xD8, (const uint8_t[]){0xA8, 0xAA, 0x55, 0x00}, 4);

  // -----------------------------------------------------------------------
  // Page 0x52: Gamma correction tables (R, G, B positive and negative curves)
  // -----------------------------------------------------------------------
  writeCommandParam(0xF0, 0x52);
  writeCommandData(0xB0, (const uint8_t[]){0x01, 0x4A, 0xB5}, 3);
  writeCommandData(0xBA, (const uint8_t[]){0xAA, 0xAE, 0xDA, 0xDA, 0xDA}, 5);
  writeCommandData(0xB9, (const uint8_t[]){0x55, 0x93, 0xA7, 0xB8, 0xD8, 0xA9, 0xF4, 0x0E, 0x27, 0x3E, 0xAA, 0x55, 0x6C, 0x82, 0x99}, 15);
  writeCommandData(0xB8, (const uint8_t[]){0x00, 0x00, 0xD3, 0xD7, 0xE2, 0x54, 0xF1, 0x0C, 0x23, 0x37, 0x55, 0x48, 0x58, 0x66, 0x7E}, 15);
  writeCommandData(0xB7, (const uint8_t[]){0xAA, 0x5D, 0x84, 0x84, 0x84}, 5);
  writeCommandData(0xB6, (const uint8_t[]){0x55, 0x60, 0x72, 0x82, 0x9F, 0x55, 0xB9, 0xD0, 0xE6, 0xFB, 0xAA, 0x0F, 0x24, 0x36, 0x4A}, 15);
  writeCommandData(0xB5, (const uint8_t[]){0x00, 0x00, 0xCE, 0xD0, 0xD4, 0x40, 0xDB, 0xEC, 0xFD, 0x0E, 0x55, 0x1D, 0x2B, 0x37, 0x4D}, 15);
  writeCommandData(0xB4, (const uint8_t[]){0xAA, 0x6F, 0x9B, 0x9B, 0x9B}, 5);
  writeCommandData(0xB3, (const uint8_t[]){0x55, 0x64, 0x76, 0x86, 0xA4, 0x95, 0xBE, 0xD7, 0xEE, 0x04, 0xAA, 0x1A, 0x30, 0x45, 0x5B}, 15);
  writeCommandData(0xB2, (const uint8_t[]){0x00, 0x00, 0x9D, 0xA2, 0xB2, 0x40, 0xC3, 0xE2, 0xF9, 0x0D, 0x55, 0x1D, 0x2B, 0x38, 0x4F}, 15);

  // -----------------------------------------------------------------------
  // Page 0x53: TCON (timing controller) sequencing
  // -----------------------------------------------------------------------
  writeCommandParam(0xF0, 0x53);
  writeCommandData(0xC1, (const uint8_t[]){0x90, 0x09, 0x88, 0x30, 0x73, 0x07, 0x66, 0x20, 0x02}, 9);
  writeCommandData(0xC2, (const uint8_t[]){0x22, 0x30, 0x03, 0x44, 0x00, 0x77, 0x60, 0x06, 0x55}, 9);
  writeCommandData(0xB1, (const uint8_t[]){0xA3, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00}, 8);
  writeCommandData(0xB2, (const uint8_t[]){0x22, 0x00, 0x00, 0x01, 0x00, 0x00, 0xA7, 0xA7}, 8);
  writeCommandData(0xB5, (const uint8_t[]){0x23, 0x80, 0x00, 0x01, 0x00, 0x3F, 0x30}, 7);
  writeCommandData(0xB6, (const uint8_t[]){0x21, 0x80, 0x00, 0x01, 0x00, 0x3F, 0x30}, 7);
  writeCommandData(0xB7, (const uint8_t[]){0x22, 0x80, 0x00, 0x00, 0xA7, 0x00, 0x11}, 7);
  writeCommandData(0xB8, (const uint8_t[]){0x21, 0x80, 0x00, 0x00, 0xA7, 0x00, 0x11}, 7);

  // -----------------------------------------------------------------------
  // Page 0x54: Source and gate driver output levels
  // -----------------------------------------------------------------------
  writeCommandParam(0xF0, 0x54);
  writeCommandData(0xB8, (const uint8_t[]){0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x40, 0x00}, 9);
  writeCommandData(0xB9, (const uint8_t[]){0x00, 0x40, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00}, 9);
  writeCommandData(0xBA, (const uint8_t[]){0x00, 0x20, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x20, 0x00}, 9);
  writeCommandData(0xBB, (const uint8_t[]){0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x40, 0x00}, 9);
  writeCommandData(0xBC, (const uint8_t[]){0x00, 0x40, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00}, 9);
  writeCommandData(0xBD, (const uint8_t[]){0x00, 0x20, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x20, 0x00}, 9);
  writeCommandData(0xBE, (const uint8_t[]){0x20, 0x04, 0x02}, 3);
  writeCommandData(0xBF, (const uint8_t[]){0x12, 0x14, 0x00}, 3);
  writeCommandData(0xC0, (const uint8_t[]){0x00, 0x24, 0x02}, 3);
  writeCommandData(0xC1, (const uint8_t[]){0x22, 0x04, 0x00}, 3);
  writeCommandData(0xC2, (const uint8_t[]){0x10, 0x14, 0x02}, 3);
  writeCommandData(0xC3, (const uint8_t[]){0x02, 0x24, 0x00}, 3);

  // -----------------------------------------------------------------------
  // Page 0x55: Additional display and power settings
  // -----------------------------------------------------------------------
  writeCommandParam(0xF0, 0x55);
  writeCommandData(0xB0, (const uint8_t[]){0x00, 0x00, 0x61, 0x00, 0x62, 0x00, 0x11, 0x5E, 0x62, 0xBF, 0x01, 0x00}, 12);
  writeCommandData(0xB1, (const uint8_t[]){0x15, 0x09}, 2);
  writeCommandData(0xB2, (const uint8_t[]){0x10, 0x43, 0x65, 0x87, 0xA9, 0xCB, 0xDC, 0xED, 0xEE, 0xFF, 0x1F, 0x53, 0x97, 0xCB, 0xFE, 0xFF}, 16);
  writeCommandData(0xB3, (const uint8_t[]){0xAD, 0x58, 0x03, 0xDF, 0x7B, 0x04, 0xCF, 0x48, 0xF0, 0x5A, 0xF0, 0x4A, 0xEF, 0x18, 0xAF, 0xF2}, 16);
  writeCommandData(0xB4, (const uint8_t[]){0x3B, 0xBF, 0xF2, 0x19, 0x6E, 0xCF, 0xF2, 0xF7, 0x2C, 0x5E, 0x9F, 0xBF, 0xD0, 0xE2, 0xE3, 0xF4}, 16);
  writeCommandData(0xB5, (const uint8_t[]){0xF4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 16);
  writeCommandData(0xB6, (const uint8_t[]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 16);
  writeCommandData(0xB7, (const uint8_t[]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 16);
  writeCommandData(0xB8, (const uint8_t[]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 16);
  writeCommandData(0xB9, (const uint8_t[]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 12);

  // -----------------------------------------------------------------------
  // Wake-up and enable display output
  // -----------------------------------------------------------------------

  // 11h (SLPOUT): Sleep Out -- enables DC/DC converter, oscillator, and scan
  writeCommand(0x11);
  delay(120); // datasheet minimum 120 ms before next command

  // 13h (NORON): Normal Display Mode On
  writeCommand(0x13);
  delay(10);

  // 29h (DISPON): Display On -- gate output enabled, image visible
  writeCommand(0x29);
  delay(20);
}



//==============================================================================
// DRAWING PRIMITIVES
//==============================================================================

// putPixel, LCD_Circle, Fast_Horizontal_Line, and LCD_Line are only compiled
// when at least one demo that calls them is enabled.  Guarding them saves
// roughly 1.8 KB of flash when all three drawing demos are disabled.
#if (0 != CIRCLES_DEMO) || (0 != LINES_DEMO) || (0 != EXPANDING_DEMO) || (TOUCH_TYPE != TOUCH_CAP)

//------------------------------------------------------------------------------
// Write a single pixel at (x, y) in RGB565 color.
//
// The CH13620 requires a minimum RAMWR payload larger than one pixel; a 1x1
// window write is silently ignored.  The workaround is to open a 2x2 window
// and fill all four pixels with the same color.  Circle and line outlines are
// 2 pixels wide as a result, which is acceptable at this display density.
//------------------------------------------------------------------------------
void putPixel(uint16_t x, uint16_t y, uint16_t color)
{
  uint8_t hi = color >> 8;
  uint8_t lo = color & 0xFF;
  setWindow(x, y, x + 1, y + 1);
  CLR_CS;
  spiSendByte(0x32); spiSendByte(0x00); spiSendByte(0x2C); spiSendByte(0x00);
  qspiSendByte(hi); qspiSendByte(lo);
  qspiSendByte(hi); qspiSendByte(lo);
  qspiSendByte(hi); qspiSendByte(lo);
  qspiSendByte(hi); qspiSendByte(lo);
  SET_CS;
}

#endif // CIRCLES_DEMO || LINES_DEMO || EXPANDING_DEMO

//------------------------------------------------------------------------------
// Fill the entire display with a solid RGB565 color.
//------------------------------------------------------------------------------
void fillScreen(uint16_t color)
{
  setWindow(0, 0, DISP_WIDTH - 1, DISP_HEIGHT - 1);
  uint8_t hi = color >> 8;
  uint8_t lo = color & 0xFF;
  CLR_CS;
  spiSendByte(0x32);
  spiSendByte(0x00);
  spiSendByte(0x2C);
  spiSendByte(0x00);
  for (uint32_t i = 0; i < (uint32_t)DISP_WIDTH * DISP_HEIGHT; i++)
  {
    qspiSendByte(hi);
    qspiSendByte(lo);
  }
  SET_CS;
}

//------------------------------------------------------------------------------
// Fill a rectangular region with a solid RGB565 color.
// x, y: top-left corner.  w, h: width and height in pixels.
//------------------------------------------------------------------------------
void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  if (!w || !h)
    return;
  setWindow(x, y, x + w - 1, y + h - 1);
  uint8_t hi = color >> 8;
  uint8_t lo = color & 0xFF;
  CLR_CS;
  spiSendByte(0x32);
  spiSendByte(0x00);
  spiSendByte(0x2C);
  spiSendByte(0x00);
  for (uint32_t i = 0; i < (uint32_t)w * h; i++)
  {
    qspiSendByte(hi);
    qspiSendByte(lo);
  }
  SET_CS;
}

//------------------------------------------------------------------------------
// Draw a PROGMEM RGB565 bitmap at position (x, y).
// Used for the LOGO_DEMO; bmpW x bmpH must fit within the display.
//------------------------------------------------------------------------------
#if (0 != LOGO_DEMO)
void drawBitmap(uint16_t x, uint16_t y,
                const uint16_t *bitmap, uint16_t bmpW, uint16_t bmpH)
{
  setWindow(x, y, x + bmpW - 1, y + bmpH - 1);
  CLR_CS;
  spiSendByte(0x32);
  spiSendByte(0x00);
  spiSendByte(0x2C);
  spiSendByte(0x00);
  uint32_t total = (uint32_t)bmpW * bmpH;
  for (uint32_t i = 0; i < total; i++)
  {
    uint16_t px = pgm_read_word(&bitmap[i]);
    qspiSendByte(px >> 8);
    qspiSendByte(px & 0xFF);
  }
  SET_CS;
}
#endif // LOGO_DEMO

//==============================================================================
// DRAWING ALGORITHMS
//==============================================================================

// Swap helper and line primitives -- only compiled when LINES_DEMO is active
#if (0 != LINES_DEMO || TOUCH_TYPE != TOUCH_CAP)
#define mSwap(a, b, t) \
  {                    \
    t = a;             \
    a = b;             \
    b = t;             \
  }

//------------------------------------------------------------------------------
// Draw a horizontal line from (x0, y) to (x1, y).
// Faster than calling putPixel() per pixel because the write window is set
// once and all pixels are streamed in a single QSPI transaction.
//------------------------------------------------------------------------------
void Fast_Horizontal_Line(uint16_t x0, uint16_t y, uint16_t x1, uint16_t color)
{
  uint16_t t;
  if (x1 < x0)
    mSwap(x0, x1, t);
  setWindow(x0, y, x1, y);
  uint8_t hi = color >> 8;
  uint8_t lo = color & 0xFF;
  CLR_CS;
  spiSendByte(0x32);
  spiSendByte(0x00);
  spiSendByte(0x2C);
  spiSendByte(0x00);
  for (uint16_t i = x0; i <= x1; i++)
  {
    qspiSendByte(hi);
    qspiSendByte(lo);
  }
  SET_CS;
}

//------------------------------------------------------------------------------
// Draw a line from (x0, y0) to (x1, y1) using Bresenham's algorithm.
// Horizontal lines are routed through Fast_Horizontal_Line for efficiency.
// Reference: http://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm
//------------------------------------------------------------------------------
void LCD_Line(uint16_t x0, uint16_t y0,
              uint16_t x1, uint16_t y1,
              uint16_t color)
{
  if (y0 == y1)
  {
    Fast_Horizontal_Line(x0, y0, x1, color);
    return;
  }

  int16_t dx = abs((int16_t)x1 - (int16_t)x0);
  int16_t sx = x0 < x1 ? 1 : -1;
  int16_t dy = abs((int16_t)y1 - (int16_t)y0);
  int16_t sy = y0 < y1 ? 1 : -1;
  int16_t err = (dx > dy ? dx : -dy) / 2;
  int16_t e2;

  for (;;)
  {
    putPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = err;
    if (e2 > -dx)
    {
      err -= dy;
      x0 = (uint16_t)((int16_t)x0 + sx);
    }
    if (e2 < dy)
    {
      err += dx;
      y0 = (uint16_t)((int16_t)y0 + sy);
    }
  }
}

#endif // LINES_DEMO

#if (0 != CIRCLES_DEMO) || (0 != EXPANDING_DEMO)
//------------------------------------------------------------------------------
// Draw a circle outline at (x0, y0) with the given radius using the Midpoint
// circle algorithm.
// Reference: http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
//------------------------------------------------------------------------------
void LCD_Circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
  uint16_t x = radius;
  uint16_t y = 0;
  int16_t re = 1 - (int16_t)x;

  while (x >= y)
  {
    putPixel(x0 + x, y0 + y, color); // 2 o'clock
    putPixel(x0 + y, y0 + x, color); // 1 o'clock
    putPixel(x0 - y, y0 + x, color); // 11 o'clock
    putPixel(x0 - x, y0 + y, color); // 10 o'clock
    putPixel(x0 - x, y0 - y, color); // 8 o'clock
    putPixel(x0 - y, y0 - x, color); // 7 o'clock
    putPixel(x0 + y, y0 - x, color); // 5 o'clock
    putPixel(x0 + x, y0 - y, color); // 4 o'clock
    y++;
    if (re < 0)
      re += 2 * (int16_t)y + 1;
    else
    {
      x--;
      re += 2 * ((int16_t)y - (int16_t)x + 1);
    }
  }
}

#endif // CIRCLES_DEMO || EXPANDING_DEMO

//==============================================================================
// SD CARD IMAGE DISPLAY
//==============================================================================
#if BUILD_SD

//------------------------------------------------------------------------------
// Load and display a 16-bit RGB565 BMP from the SD card, centered on screen.
//
// Supported:  16-bit RGB565, standard (bottom-up) or top-down row order.
//             Images smaller than the display are centered with black borders.
//             Images larger than the display are clipped to display size.
//
// Returns true on success, false if the file cannot be opened or is not a
// valid 16-bit BMP.
//
// To create a compatible image use the Python script in the header file, or
// export from GIMP as BMP with "16 bits R5 G6 B5" color depth selected.
//
// Design note: the entire image is sent inside a single RAMWR transaction
// (display CS held LOW throughout).  SD reads use hardware SPI on PORTB while
// the display QSPI lives on PORTC -- the two buses are electrically independent
// so SD activity cannot corrupt the open display transaction.
//------------------------------------------------------------------------------
bool drawBitmapFromSD(const char *filename)
{
  File f = SD.open(filename);
  if (!f)
  {
    Serial.print(F("SD: cannot open "));
    Serial.println(filename);
    return false;
  }

  // Read the first 34 bytes -- covers the BITMAPFILEHEADER (14 bytes) and
  // enough of the BITMAPINFOHEADER to reach bits-per-pixel at offset 28.
  // Declared static to keep this off the call stack.
  static uint8_t hdr[34];
  if (f.read(hdr, sizeof(hdr)) != sizeof(hdr))
  {
    Serial.println(F("SD: header read failed"));
    f.close();
    return false;
  }

  // Validate BM signature
  if (hdr[0] != 'B' || hdr[1] != 'M')
  {
    Serial.println(F("SD: not a BMP file"));
    f.close();
    return false;
  }

  // Only 16-bit RGB565 is supported
  uint16_t bpp = hdr[28] | (hdr[29] << 8);
  if (bpp != 16)
  {
    Serial.println(F("SD: BMP must be 16-bit RGB565"));
    f.close();
    return false;
  }

  // Parse header fields (BMP is little-endian)
  uint32_t dataOffset = hdr[10] | (hdr[11] << 8) | (hdr[12] << 16) | (hdr[13] << 24);

  int32_t imgW = hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24);

  int32_t imgH = hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24);

  // Output some info about the image to the serial console for debugging
  Serial.print(F("SD: "));
  Serial.print(filename);
  Serial.print(F("  "));
  Serial.print(imgW);
  Serial.print(F("x"));
  Serial.print(imgH < 0 ? -imgH : imgH);
  Serial.print(F("  "));
  Serial.print(bpp);
  Serial.print(F("bpp  "));
  Serial.println(imgH < 0 ? F("top-down") : F("bottom-up"));

  // Negative height indicates top-down row order
  bool topDown = (imgH < 0);
  if (topDown)
    imgH = -imgH;

  // Row stride in the file (each row padded to a 4-byte boundary)
  uint32_t fileRowBytes = (imgW * 2 + 3) & ~3UL;

  // Clamp to display dimensions
  uint16_t drawW = (imgW > DISP_WIDTH) ? DISP_WIDTH : imgW;
  uint16_t drawH = (imgH > DISP_HEIGHT) ? DISP_HEIGHT : imgH;

  // Center the image on the display
  uint16_t xOff = (DISP_WIDTH - drawW) / 2;
  uint16_t yOff = (DISP_HEIGHT - drawH) / 2;

  // Set the write window once for the entire image, then open one RAMWR
  // transaction and stream all rows into it.  Display CS stays LOW while SD
  // reads happen; that is safe because QSPI (PORTC) and SD SPI (PORTB) are
  // on separate ports with no shared pins.
  setWindow(xOff, yOff, xOff + drawW - 1, yOff + drawH - 1);
  CLR_CS;
  spiSendByte(0x32);
  spiSendByte(0x00);
  spiSendByte(0x2C);
  spiSendByte(0x00);

  // Row buffer: 184 bytes = 92 pixels per chunk.  Static to stay off the stack.
  static uint8_t rowBuf[DISP_WIDTH / 2];    // 184 bytes = 92 pixels
  const uint16_t CHUNK_PX = DISP_WIDTH / 4; // 92 pixels per chunk

  for (uint16_t row = 0; row < drawH; row++)
  {
    uint32_t srcRow = topDown ? row : (imgH - 1 - row);
    f.seek(dataOffset + srcRow * fileRowBytes);

    uint16_t pixelsDone = 0;
    while (pixelsDone < drawW)
    {
      uint16_t chunkPx = drawW - pixelsDone;
      if (chunkPx > CHUNK_PX)
        chunkPx = CHUNK_PX;

      f.read(rowBuf, chunkPx * 2);

      // BMP stores RGB565 little-endian (lo byte first); display wants big-endian
      for (uint16_t i = 0; i < chunkPx * 2; i += 2)
      {
        qspiSendByte(rowBuf[i + 1]); // high byte
        qspiSendByte(rowBuf[i]);     // low byte
      }

      pixelsDone += chunkPx;
    }
  }

  SET_CS;
  f.close();
  return true;
}

//------------------------------------------------------------------------------
// Scan the SD card root directory and display every 16-bit RGB565 BMP found.
// Files are shown in FAT directory order; the scan repeats from the beginning
// each time show_BMPs_in_root() is called (i.e. each pass through loop()).
//------------------------------------------------------------------------------
void show_BMPs_in_root(void)
{
  File root_dir = SD.open("/");
  if (!root_dir)
  {
    Serial.println(F("show_BMPs_in_root: cannot open root"));
    return;
  }

  // 8.3 filename: up to 12 characters + null terminator
  char fname[13];

  while (1)
  {
    File bmp_file = root_dir.openNextFile();
    if (!bmp_file)
      break; // end of directory

    bool is_bmp = !bmp_file.isDirectory() && (strstr(bmp_file.name(), ".BMP") != NULL || strstr(bmp_file.name(), ".bmp") != NULL);

    if (is_bmp)
    {
      // Copy name before closing the handle
      strncpy(fname, bmp_file.name(), sizeof(fname) - 1);
      fname[sizeof(fname) - 1] = '\0';
    }
    bmp_file.close();

    if (is_bmp)
    {
      Serial.print(F("SD: showing "));
      Serial.println(fname);
      if (!drawBitmapFromSD(fname))
      {
        // File opened but was not a valid 16-bit BMP -- show grey placeholder
        fillRect(DISP_WIDTH / 4, DISP_HEIGHT / 4,
                 DISP_WIDTH / 2, DISP_HEIGHT / 2, 0x39C7);
      }
      delay(WAIT_TIME);
    }
  }

  root_dir.close();
}

#endif // BUILD_SD

//==============================================================================
// CAP TOUCH  (CFAL368448A0-019DC)
// Controller: CST816  (I2C 0x15)
// Reset: D6 (active low)    INT: D7 (active low)
// SDA: A4    SCL: A5  (ATmega328P hardware TWI)
//
// Register burst from 0x01 (6 bytes):
//   [0] GestureID  [1] FingerNum
//   [2] XposH (bits[3:0] = X[11:8])  [3] XposL
//   [4] YposH (bits[3:0] = Y[11:8])  [5] YposL
//==============================================================================
#if (TOUCH_TYPE == TOUCH_TYPE_CAP)

void touchInit()
{
  pinMode(TOUCH_RST_PIN, OUTPUT);
  pinMode(TOUCH_INT_PIN, INPUT);
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(20);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(50);
  Wire.begin();
}

// Returns true and fills *x/*y when at least one finger is down.
bool touchRead(uint16_t *x, uint16_t *y)
{
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0)
    return false;
  Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)6);
  if (Wire.available() < 6)
    return false;
  Wire.read();                   // GestureID (unused)
  uint8_t fingers = Wire.read(); // FingerNum
  uint8_t xh = Wire.read();      // XposH
  uint8_t xl = Wire.read();      // XposL
  uint8_t yh = Wire.read();      // YposH
  uint8_t yl = Wire.read();      // YposL
  if (fingers == 0)
    return false;
  *x = ((uint16_t)(xh & 0x0F) << 8) | xl;
  *y = ((uint16_t)(yh & 0x0F) << 8) | yl;
  return true;
}

// Close button: 44x44 red box centered at the top of the screen.
// A white X is drawn as two 2-pixel-wide diagonal stripes with 8px margins.
#define CLOSE_BTN_W 44
#define CLOSE_BTN_H 44
#define CLOSE_BTN_X ((DISP_WIDTH - CLOSE_BTN_W) / 2) // 162
#define CLOSE_BTN_Y 5

void drawCloseButton()
{
  fillRect(CLOSE_BTN_X, CLOSE_BTN_Y, CLOSE_BTN_W, CLOSE_BTN_H, COLOR_RED);
  LCD_Line(CLOSE_BTN_X + 8, CLOSE_BTN_Y + 8,
           CLOSE_BTN_X + CLOSE_BTN_W - 8, CLOSE_BTN_Y + CLOSE_BTN_H - 8,
           COLOR_WHITE);
  LCD_Line(CLOSE_BTN_X + 8, CLOSE_BTN_Y + CLOSE_BTN_H - 8,
           CLOSE_BTN_X + CLOSE_BTN_W - 8, CLOSE_BTN_Y + 8,
           COLOR_WHITE);
}

#endif // TOUCH_TYPE_CAP

//==============================================================================
// SETUP
//==============================================================================
void setup()
{
  Serial.begin(9600);
  Serial.println(F("CFAL368448A0-019DN/DC"));

  // Drive CS and RST high BEFORE setting pin directions so the display never
  // sees a spurious chip-select assertion during the transition.
  PORTB |= (CS_MASK | RST_MASK | SDCS_MASK);

  // Configure pin directions via direct port registers
  DDRB |= PORTB_OUT_MASK; // PB0-PB2 (D8-D10) as outputs
  DDRC |= PORTC_OUT_MASK; // PC0-PC3 (A0-A3) as outputs for QSPI
  DDRD |= PORTD_OUT_MASK; // PD5 (D5) as output for QSPI CLK

  // Idle state: all QSPI data and clock lines low, CS/RST/SDCS high
  PORTC &= ~PORTC_OUT_MASK;
  PORTB |= (CS_MASK | RST_MASK | SDCS_MASK);

#if BUILD_SD
  // SD CS is already deasserted above.  Initialize the SD library.
  // SD.begin() starts hardware SPI internally; that is safe here because
  // the display CS is high and the display is not yet driving the bus.
  if (!SD.begin(10)) // Arduino D10 = SD chip select
    Serial.println(F("SD init failed -- check card and wiring"));
  else
    Serial.println(F("SD card ready"));
#endif

#if (TOUCH_TYPE == TOUCH_TYPE_CAP)
  touchInit();
  Serial.println(F("Touch controller ready"));
#endif

  Serial.println(F("Resetting display..."));
  hardwareReset();

  Serial.println(F("Initializing display..."));
  initDisplay();

  Serial.println(F("Ready."));
}

//==============================================================================
// MAIN LOOP
//==============================================================================
void loop()
{
  Serial.println(F("Starting demo sequence from the top..."));
// ---------- CAPACITIVE TOUCH -------------------------------------------
// state  0: first entry -- clears screen and draws the close button
// state  1: active -- draws cyan dots; tap the red X to exit
// state -1: exited -- falls through to other demos
#if (TOUCH_TYPE == TOUCH_TYPE_CAP)
  {
    static int8_t state = 0;
    if (state == 0)
    {
      fillScreen(COLOR_BLACK);
      drawCloseButton();
      state = 1;
      Serial.println(F("Touch the screen  (tap X to exit)"));
    }
    while (state > 0)
    {
      if (digitalRead(TOUCH_INT_PIN) == LOW)
      {
        uint16_t tx, ty;
        if (touchRead(&tx, &ty))
        {
          if (tx >= CLOSE_BTN_X && tx < (CLOSE_BTN_X + CLOSE_BTN_W) &&
              ty >= CLOSE_BTN_Y && ty < (CLOSE_BTN_Y + CLOSE_BTN_H))
          {
            state = 0; // Change to -1 to skip the touch demo every loop after the first loop
            Serial.println(F("Touch demo exit"));
          }
          else
          {
            Serial.print(F("Touch ("));
            Serial.print(tx);
            Serial.print(F(", "));
            Serial.print(ty);
            Serial.println(F(")"));
            if (tx < DISP_WIDTH && ty < DISP_HEIGHT)
              fillRect(tx > 2 ? tx - 3 : 0,
                       ty > 2 ? ty - 3 : 0,
                       6, 6, COLOR_CYAN);
          }
        }
      }
    }
  }
#endif

// ---------- SOLID COLOR CYCLE ------------------------------------------
#if (0 != COLOR_DEMO)
  Serial.println(F("Color: red"));
  fillScreen(COLOR_RED);
  delay(WAIT_TIME);

  Serial.println(F("Color: green"));
  fillScreen(COLOR_GREEN);
  delay(WAIT_TIME);

  Serial.println(F("Color: blue"));
  fillScreen(COLOR_BLUE);
  delay(WAIT_TIME);

  Serial.println(F("Color: white"));
  fillScreen(COLOR_WHITE);
  delay(WAIT_TIME);
#endif

// ---------- CIRCLES (Midpoint circle algorithm) -------------------------
#if (0 != CIRCLES_DEMO)
  Serial.println(F("Circles"));
  fillScreen(COLOR_BLUE);

  // Large outer ring
  LCD_Circle(DISP_WIDTH / 2, DISP_HEIGHT / 2, 180, COLOR_CYAN);
  // Mid-size circles at compass points
  LCD_Circle(DISP_WIDTH / 2, DISP_HEIGHT / 2, 80, COLOR_WHITE);
  LCD_Circle(DISP_WIDTH / 2 - 90, DISP_HEIGHT / 2, 60, COLOR_GREEN);
  LCD_Circle(DISP_WIDTH / 2 + 90, DISP_HEIGHT / 2, 60, COLOR_RED);
  LCD_Circle(DISP_WIDTH / 2, DISP_HEIGHT / 2 - 110, 50, COLOR_ORANGE);
  LCD_Circle(DISP_WIDTH / 2, DISP_HEIGHT / 2 + 110, 50, COLOR_MAGENTA);

  delay(WAIT_TIME);
#endif

// ---------- LINE FAN (Bresenham's algorithm) ----------------------------
// Lines fan out from the center to all four edges.  The RGB bytes cycle
// freely as uint8_t counters; they are converted to RGB565 at each call.
// RGB565 packing: R[15:11] = r>>3, G[10:5] = g>>2, B[4:0] = b>>3
#if (0 != LINES_DEMO)
  {
    uint8_t r, g, b;
    uint16_t x, y;

    Serial.println(F("Lines"));
    fillScreen(COLOR_BLACK);

    r = 0x00;
    g = 0x0F;
    b = 0xA0;
    for (x = 0; x < DISP_WIDTH; x++)
    {
      LCD_Line(DISP_WIDTH / 2, DISP_HEIGHT / 2, x, 0,
               ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3));
      r++;
      g--;
      b += 2;
    }
    for (y = 0; y < DISP_HEIGHT; y++)
    {
      LCD_Line(DISP_WIDTH / 2, DISP_HEIGHT / 2, DISP_WIDTH - 1, y,
               ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3));
      r++;
      g += 4;
      b += 2;
    }
    for (x = DISP_WIDTH - 1; x > 0; x--)
    {
      LCD_Line(DISP_WIDTH / 2, DISP_HEIGHT / 2, x, DISP_HEIGHT - 1,
               ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3));
      r -= 3;
      g -= 2;
      b--;
    }
    for (y = DISP_HEIGHT - 1; y > 0; y--)
    {
      LCD_Line(DISP_WIDTH / 2, DISP_HEIGHT / 2, 0, y,
               ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3));
      r -= 3;
      g--;
      b++;
    }

    delay(WAIT_TIME);
  } // LINES_DEMO block
#endif

// ---------- CHECKERBOARD -----------------------------------------------
#if (0 != CHECKER_DEMO)
  // The 368x448 display divides evenly into 23x28 blocks of 16x16 pixels.
  // fillRect() is used for each block so the whole pattern renders quickly.
  Serial.println(F("Checkerboard"));
  for (uint8_t cx = 0; cx < (DISP_WIDTH / 16); cx++)
  {
    for (uint8_t cy = 0; cy < (DISP_HEIGHT / 16); cy++)
    {
      uint16_t color;
      if ((cx ^ cy) & 1)
        color = COLOR_BLACK;
      else
        // Vary the hue across the board: red from column, green from row
        color = (uint16_t)((cx * 2) & 0x1F) << 11 | (uint16_t)((cy * 2) & 0x3F) << 5 | 0x1F; // full blue component
      fillRect((uint16_t)cx * 16, (uint16_t)cy * 16, 16, 16, color);
    }
  }
  delay(WAIT_TIME);
#endif

// ---------- EXPANDING CIRCLES ------------------------------------------
// Circles grow from left to right across the display.  The color shifts
// from blue-green toward red as each circle gets larger.
#if (0 != EXPANDING_DEMO)
  {
    Serial.println(F("Expanding circles"));
    fillScreen(COLOR_BLACK);
    for (uint8_t i = 2; i < DISP_WIDTH / 2; i += 2)
    {
      uint8_t rb = (uint8_t)((uint16_t)i << 2); // wraps intentionally
      uint8_t gb = 0xFF - rb;
      uint16_t color = ((uint16_t)(rb >> 3) << 11) | ((uint16_t)(gb >> 2) << 5) | 0x1F; // full blue
      LCD_Circle(i + 2, DISP_HEIGHT / 2, i, color);
    }
    delay(WAIT_TIME);
  } // EXPANDING_DEMO block
#endif

// ---------- LOGO FROM FLASH --------------------------------------------
#if (0 != LOGO_DEMO)
  Serial.println(F("Logo"));
  fillScreen(COLOR_BLACK);
  drawBitmap((DISP_WIDTH - LOGO_WIDTH) / 2,
             (DISP_HEIGHT - LOGO_HEIGHT) / 2,
             cfa_logo, LOGO_WIDTH, LOGO_HEIGHT);
  delay(WAIT_TIME * 2);
#endif

// ---------- IMAGES FROM SD CARD ----------------------------------------
#if (0 != SD_DEMO)
  Serial.println(F("SD images"));
  show_BMPs_in_root();
#endif

} // loop()
//==============================================================================
