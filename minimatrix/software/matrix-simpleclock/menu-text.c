/*
    Menudesigner (file was already part of my eariler Gamebox project)
    Copyright (C) 2004 - 2016  by Malte Marwedel
    m DOT talk AT marwedels DOT de

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* This function writes single chars to the display.
Currently 5x5, 7x5 and 8x15 pixel fonts are supported.
Every char is encoded as 5 byte (5x5 and 5x7 fonts).
For the 8x15 font, its 16 bytes.

The 5x7 font is based of a font from:
http://overcode.yak.net/12
The 8x15 font is based on Droid Sans Mono. See Apache license.


Date of last changes:
  2016-06-18: Completely rework renderer, adding utf-8 support, including 8x15 font
  2015-12-10: Add tab for 5x5 font, same size as digits of font 13 and 14
  2014-04-27: Add support for 5x5 font (font 13, 14, 15), add degree symbol
  2010-05-15: Prevent overflow of chars to the left side
  2010-02-10: Add german umlauts, fix & and ~ sign, translate some comments
  2009-09-10: Use SCREENPOS datatype to support 16 bit screen resolution
  2009-05-24: Changed in black/white char renderer
  2005-07-19: Character "P" was wrong
  2005-07-23: draw_tinynumber(), draw_tinydigit() added
*/

#include "menu-interpreter.h"

//the following file might define MENU_USE_UTF8
#include "menu-interpreter-config.h"

#include <string.h>

// === defines, adapt to your needs ==========================

//enable if you dont want to switch to utf-8
//#define MENU_ENABLE_SPECIAL_CHARS

//make pc compatible
#ifndef AVR_BUILD

#define PROGMEM
#define COPYFUNCTION memcpy
#define COPY4BYTES(X) *(X)

#else

#include <avr/pgmspace.h>

#define COPYFUNCTION memcpy_P
//you might want to use pgm_read_dword on small (<= 64KB flash) AVRs
#define COPY4BYTES pgm_read_dword_far

#endif


//#define DEBUG

#ifdef DEBUG
#include "stdio.h"
//use printf for debug messages
#define MENU_DEBUGMSG printf

#else
//ignore debug messages
#define MENU_DEBUGMSG(...)

#endif


// === end of defines which should be modified by the user ====

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

//for backward compatibility, warn users
#if defined(ENABLE_GERMAN_UMLAUTS)
#error "Update to UTF-8"
#endif

#if defined(MENU_ENABLE_SPECIAL_CHARS)
#warning "MENU_ENABLE_SPECIAL_CHARS is deprecated and might be removed in future versions, use UTF-8 instead"
#define MENU_USE_UTF8
#endif

#define MENU_TEXT_5X7_BYTES 5
#define MENU_TEXT_5X5_BYTES 5
#define MENU_TEXT_8X15_BYTES 16
#define MENU_TEXT_5X7_WIDTH 5
#define MENU_TEXT_5X5_WIDTH 5
#define MENU_TEXT_8X15_WIDTH 8


#if defined(MENU_USE_FONT_0) || defined(MENU_USE_FONT_1) || defined(MENU_USE_FONT_2) || defined(MENU_USE_FONT_3)
#define MENU_TEXT_ENABLE_5X7
#define MENU_TEXT_5X7_CNT 1
#else
#define MENU_TEXT_5X7_CNT 0
#endif

#if defined(MENU_USE_FONT_4) || defined(MENU_USE_FONT_5) || defined(MENU_USE_FONT_6) || defined(MENU_USE_FONT_7)
#define MENU_TEXT_ENABLE_8X15

#define MENU_TEXT_8X15_CNT 1
#else
#define MENU_TEXT_8X15_CNT 0
#endif

#if defined(MENU_USE_FONT_12) || defined(MENU_USE_FONT_13) || defined(MENU_USE_FONT_14) || defined(MENU_USE_FONT_15)
#define MENU_TEXT_ENABLE_5X5
#define MENU_TEXT_5X5_CNT 1
#else
#define MENU_TEXT_5X5_CNT 0
#endif

//this define gives the different types of fonts used, to optimize code later
#define MENU_TEXT_FONT_CLASSES (MENU_TEXT_5X7_CNT + MENU_TEXT_8X15_CNT + MENU_TEXT_5X5_CNT)

//see extra directory for a 8x15 renderer which uses font numbers 3...6
#if defined(MENU_USE_FONT_8) || defined(MENU_USE_FONT_9) || \
    defined(MENU_USE_FONT_10) || defined(MENU_USE_FONT_11) || \
    (MENU_USE_FONT_MAX > 15)

#error "Menu uses a font for which no renderer is implemented right now"

#endif

// some macros to optimize the source code without let him look way too messy

#ifdef MENU_TEXT_ENABLE_5X5
	#if (MENU_TEXT_FONT_CLASSES > 1)
		#define MENU_CHECK_FONT_5X5 ((font >= 12) && (font <= 15))
	#else
		#define MENU_CHECK_FONT_5X5 (1)
	#endif
#else
	#define MENU_CHECK_FONT_5X5 (0)
#endif

#ifdef MENU_TEXT_ENABLE_5X7
	#if (MENU_TEXT_FONT_CLASSES > 1)
		#define MENU_CHECK_FONT_5X7 (font <= 3)
	#else
		#define MENU_CHECK_FONT_5X7 (1)
	#endif
#else
	#define MENU_CHECK_FONT_5X7 (0)
#endif

#ifdef MENU_TEXT_ENABLE_8X15
	#if (MENU_TEXT_FONT_CLASSES > 1)
		#define MENU_CHECK_FONT_8X15 ((font >= 4) && (font <= 8))
	#else
		#define MENU_CHECK_FONT_8X15 (1)
	#endif
#else
	#define MENU_CHECK_FONT_8X15 (0)
#endif

#if defined(MENU_TEXT_ENABLE_5X7) || defined(MENU_TEXT_ENABLE_8X15)
//only valid for font 2, 3, 6, 7
	#define MENU_CHECK_FONT_UNDERLINED ((font & 0xFA) == 0x02)
#else
	#define MENU_CHECK_FONT_UNDERLINED (0)
#endif

//note all utf8CharXxY_t must have the bitmap right after the utf8Code. Otherwise optimizations in menu_get_pBitmap wont work as expected
typedef struct {
	unsigned long utf8Code; //should be 32bit datatype
	char bitmap[MENU_TEXT_5X5_BYTES];
} utf8Char5x5_t;

typedef struct {
	unsigned long utf8Code; //should be 32bit datatype
	char bitmap[MENU_TEXT_8X15_BYTES];
} utf8Char8x15_t;

typedef utf8Char5x5_t utf8Char5x7_t;

//ASCII characters defined for every font
#define MENU_CHARACTERS 95
#define MENU_CHARACTER_TABLE_OFFSET 32

#define WEAK __attribute__((weak))

/*
BUG:
  Defining extern weak global variables here and replacing them by actual
  arrays on demand:
  works on Linux with gcc 4.9.2 (64Bit)
  works on Windows Vista with with Cygwin and gcc 5.4.0 (32bit)
  fails on Windows 7 with Cygwin and gcc 5.4.0 (64bit) for the characters_5x7_utf8 array
     (other program data found under the array address) but works for the characters_5x7 array...
  So we remove the defines here if the real array exists, otherwise all testcase for special
     chars will fail under the one platform...
*/

#ifndef MENU_TEXT_ENABLE_5X5
extern const char           WEAK characters_5x5[MENU_CHARACTERS * MENU_TEXT_5X5_BYTES];
extern const utf8Char5x5_t  WEAK characters_5x5_utf8[];
#endif

#ifndef MENU_TEXT_ENABLE_5X7
extern const char           WEAK characters_5x7[MENU_CHARACTERS * MENU_TEXT_5X7_BYTES];
extern const utf8Char5x7_t  WEAK characters_5x7_utf8[];
#endif


#ifndef MENU_TEXT_ENABLE_8X15
extern const char           WEAK characters_8x15[MENU_CHARACTERS * MENU_TEXT_8X15_BYTES];
extern const utf8Char8x15_t WEAK characters_8x15_utf8[];
#endif



#ifdef MENU_TEXT_ENABLE_5X7

const char characters_5x7[MENU_CHARACTERS * MENU_TEXT_5X7_BYTES] PROGMEM = {
0x00,0x00,0x00,0x00,0x00, //Space
0x00,0x00,0x5F,0x00,0x00, //Exclamation mark
0x00,0x03,0x00,0x03,0x00,
0x14,0x7F,0x14,0x7F,0x14,
0x24,0x2A,0x7F,0x2A,0x12,
0x26,0x16,0x08,0x34,0x32,
0x36,0x49,0x55,0x22,0x50, //&
0x00,0x00,0x03,0x00,0x00,
0x00,0x1C,0x22,0x41,0x00,
0x00,0x41,0x22,0x1C,0x00,
0x22,0x14,0x7F,0x14,0x22,
0x08,0x08,0x3E,0x08,0x08,
0x00,0x00,0x60,0x30,0x00,
0x08,0x08,0x08,0x08,0x08,
0x00,0x00,0x60,0x60,0x00,
0x20,0x10,0x08,0x04,0x02,
0x3E,0x41,0x41,0x41,0x3E,
0x00,0x42,0x7F,0x40,0x00,
0x42,0x61,0x51,0x49,0x46,
0x22,0x41,0x49,0x49,0x36,
0x0C,0x0A,0x49,0x7F,0x48,
0x2F,0x49,0x49,0x49,0x31,
0x3E,0x49,0x49,0x49,0x32,
0x01,0x71,0x09,0x05,0x03,
0x36,0x49,0x49,0x49,0x36,
0x26,0x49,0x49,0x49,0x3E,
0x00,0x00,0x36,0x36,0x00,
0x00,0x00,0x56,0x36,0x00,
0x08,0x14,0x22,0x41,0x00,
0x14,0x14,0x14,0x14,0x14,
0x41,0x22,0x14,0x08,0x00,
0x02,0x01,0x51,0x09,0x06,
0x3E,0x41,0x5D,0x55,0x1E,
0x7E,0x09,0x09,0x09,0x7E, //A
0x7F,0x49,0x49,0x49,0x36,
0x3E,0x41,0x41,0x41,0x22,
0x7F,0x41,0x41,0x41,0x3E,
0x7F,0x49,0x49,0x49,0x41,
0x7F,0x09,0x09,0x09,0x09, //F
0x3E,0x41,0x49,0x49,0x3A,
0x7F,0x08,0x08,0x08,0x7F, //H
0x00,0x41,0x7F,0x41,0x00, //I
0x30,0x40,0x41,0x3F,0x01,
0x7F,0x08,0x08,0x14,0x63,
0x00,0x7F,0x40,0x40,0x40,
0x7F,0x04,0x08,0x04,0x7F,
0x7F,0x02,0x0C,0x10,0x7F,
0x3E,0x41,0x41,0x41,0x3E,
0x7F,0x09,0x09,0x09,0x06, //P
0x3E,0x41,0x51,0x61,0x7E,
0x7F,0x09,0x19,0x29,0x46,
0x26,0x49,0x49,0x49,0x32,
0x01,0x01,0x7F,0x01,0x01,
0x3F,0x40,0x40,0x40,0x3F,
0x07,0x18,0x60,0x18,0x07,
0x1F,0x60,0x18,0x60,0x1F,
0x63,0x14,0x08,0x14,0x63,
0x03,0x04,0x78,0x04,0x03,
0x61,0x51,0x49,0x45,0x43,
0x00,0x7F,0x41,0x41,0x00,
0x02,0x04,0x08,0x10,0x20,
0x00,0x41,0x41,0x7F,0x00,
0x04,0x02,0x01,0x02,0x04,
0x40,0x40,0x40,0x40,0x40,
0x00,0x01,0x02,0x00,0x00,
0x30,0x4A,0x4A,0x4A,0x7C,
0x00,0x7F,0x44,0x44,0x38,
0x38,0x44,0x44,0x44,0x00,
0x00,0x38,0x44,0x44,0x7F,
0x38,0x54,0x54,0x54,0x18,
0x08,0x7E,0x09,0x01,0x00,
0x0C,0x52,0x52,0x52,0x3C,
0x7F,0x04,0x04,0x04,0x78,
0x00,0x00,0x7A,0x00,0x00,
0x20,0x40,0x3A,0x00,0x00,
0x7F,0x10,0x28,0x44,0x00,
0x00,0x00,0x7F,0x00,0x00,
0x7C,0x04,0x18,0x04,0x78,
0x00,0x78,0x04,0x04,0x78,
0x38,0x44,0x44,0x44,0x38,
0x7C,0x14,0x24,0x24,0x18,
0x18,0x24,0x24,0x14,0x7C,
0x7C,0x08,0x04,0x04,0x00,
0x48,0x54,0x54,0x54,0x24,
0x00,0x02,0x3F,0x42,0x00,
0x3C,0x40,0x40,0x40,0x3C,
0x1C,0x20,0x40,0x20,0x1C,
0x3C,0x40,0x30,0x40,0x3C,
0x44,0x28,0x10,0x28,0x44,
0x0C,0x50,0x50,0x50,0x3C,
0x44,0x64,0x54,0x4C,0x00,
0x08,0x36,0x41,0x41,0x00,
0x00,0x00,0x7F,0x00,0x00,
0x41,0x41,0x36,0x08,0x00,
0x08,0x04,0x08,0x10,0x08 //~
};
#ifdef MENU_USE_UTF8
#define MENU_TEXT_5X7_UTF8CHARS 8
const utf8Char5x7_t characters_5x7_utf8[MENU_TEXT_5X7_UTF8CHARS] PROGMEM = {
{0xC2B0, {0x00, 0x02, 0x05, 0x02, 0x00}}, // °
{0xC384, {0x79, 0x14, 0x12, 0x14, 0x79}}, // Ä
{0xC396, {0x39, 0x44, 0x44, 0x44, 0x39}}, // Ö
{0xC39C, {0x3D, 0x40, 0x40, 0x40, 0x3D}}, // Ü
{0xC39F, {0x7E, 0x01, 0x49, 0x49, 0x36}}, // ß
{0xC3A4, {0x31, 0x48, 0x48, 0x48, 0x7D}}, // ä
{0xC3B6, {0x39, 0x44, 0x44, 0x44, 0x39}}, // ö
{0xC3BC, {0x3D, 0x40, 0x40, 0x40, 0x3D}} // ü
};

#else

#define MENU_TEXT_5X7_UTF8CHARS 0

#endif
#else

#define MENU_TEXT_5X7_UTF8CHARS 0

#endif

#ifdef MENU_TEXT_ENABLE_5X5
const char characters_5x5[MENU_CHARACTERS * MENU_TEXT_5X5_BYTES] PROGMEM = {
0x00, 0x00, 0x00, 0x00, 0x00, //
0x00, 0x00, 0x17, 0x00, 0x00, // !
0x00, 0x03, 0x00, 0x03, 0x00, // "
0x0A, 0x1F, 0x0A, 0x1F, 0x0A, // #
0x02, 0x15, 0x1F, 0x15, 0x08, // $
0x13, 0x0B, 0x04, 0x1A, 0x19, // %
0x1A, 0x15, 0x09, 0x12, 0x00, // &
0x00, 0x00, 0x03, 0x00, 0x00, // '
0x00, 0x00, 0x0E, 0x11, 0x00, // (
0x00, 0x11, 0x0E, 0x00, 0x00, // )
0x11, 0x0A, 0x1F, 0x0A, 0x11, // *
0x04, 0x04, 0x1F, 0x04, 0x04, // +
0x00, 0x10, 0x08, 0x00, 0x00, // ,
0x04, 0x04, 0x04, 0x04, 0x04, // -
0x00, 0x00, 0x10, 0x00, 0x00, // .
0x00, 0x10, 0x0E, 0x01, 0x00, // /
0x1F, 0x11, 0x11, 0x1F, 0x00, // 0
0x00, 0x02, 0x1F, 0x00, 0x00, // 1
0x1D, 0x15, 0x15, 0x17, 0x00, // 2
0x15, 0x15, 0x15, 0x1F, 0x00, // 3
0x07, 0x04, 0x04, 0x1F, 0x00, // 4
0x17, 0x15, 0x15, 0x1D, 0x00, // 5
0x1F, 0x15, 0x15, 0x1D, 0x00, // 6
0x01, 0x01, 0x01, 0x1F, 0x00, // 7
0x1F, 0x15, 0x15, 0x1F, 0x00, // 8
0x17, 0x15, 0x15, 0x1F, 0x00, // 9
0x00, 0x00, 0x0A, 0x00, 0x00, // :
0x00, 0x10, 0x0A, 0x00, 0x00, // ;
0x00, 0x04, 0x0A, 0x11, 0x00, // <
0x0A, 0x0A, 0x0A, 0x0A, 0x0A, // =
0x00, 0x11, 0x0A, 0x04, 0x00, // >
0x00, 0x05, 0x15, 0x02, 0x00, // ?
0x09, 0x15, 0x1D, 0x11, 0x1E, // @
0x1E, 0x05, 0x05, 0x1E, 0x00, // A
0x1F, 0x15, 0x15, 0x0E, 0x00, // B
0x0E, 0x11, 0x11, 0x11, 0x00, // C
0x1F, 0x11, 0x11, 0x0E, 0x00, // D
0x00, 0x1F, 0x15, 0x15, 0x00, // E
0x00, 0x1F, 0x05, 0x05, 0x00, // F
0x0E, 0x11, 0x15, 0x0C, 0x00, // G
0x1F, 0x04, 0x04, 0x1F, 0x00, // H
0x00, 0x00, 0x1F, 0x00, 0x00, // I
0x00, 0x08, 0x10, 0x0F, 0x00, // J
0x1F, 0x04, 0x0A, 0x11, 0x00, // K
0x00, 0x1F, 0x10, 0x10, 0x00, // L
0x1F, 0x02, 0x04, 0x02, 0x1F, // M
0x1F, 0x02, 0x04, 0x08, 0x1F, // N
0x0E, 0x11, 0x11, 0x0E, 0x00, // O
0x1F, 0x05, 0x05, 0x02, 0x00, // P
0x0E, 0x11, 0x11, 0x19, 0x1E, // Q
0x1F, 0x05, 0x0D, 0x12, 0x00, // R
0x00, 0x12, 0x15, 0x09, 0x00, // S
0x01, 0x01, 0x1F, 0x01, 0x01, // T
0x0F, 0x10, 0x10, 0x0F, 0x00, // U
0x03, 0x0C, 0x10, 0x0C, 0x03, // V
0x07, 0x18, 0x06, 0x18, 0x07, // W
0x11, 0x0A, 0x04, 0x0A, 0x11, // X
0x03, 0x04, 0x18, 0x04, 0x03, // Y
0x11, 0x19, 0x15, 0x13, 0x11, // Z
0x00, 0x00, 0x1F, 0x11, 0x00, // [
0x00, 0x01, 0x0E, 0x10, 0x00, // backslash
0x00, 0x11, 0x1F, 0x00, 0x00, // ]
0x00, 0x02, 0x01, 0x02, 0x00, // ^
0x10, 0x10, 0x10, 0x10, 0x10, // _
0x00, 0x01, 0x02, 0x00, 0x00, // `
0x0C, 0x12, 0x12, 0x1E, 0x00, // a
0x1F, 0x12, 0x12, 0x0C, 0x00, // b
0x0C, 0x12, 0x12, 0x00, 0x00, // c
0x0C, 0x12, 0x12, 0x1F, 0x00, // d
0x0E, 0x15, 0x15, 0x16, 0x00, // e
0x00, 0x04, 0x1F, 0x05, 0x00, // f
0x00, 0x17, 0x15, 0x0F, 0x00, // g
0x1F, 0x04, 0x04, 0x18, 0x00, // h
0x00, 0x00, 0x1D, 0x00, 0x00, // i
0x00, 0x10, 0x1D, 0x00, 0x00, // j
0x00, 0x1F, 0x08, 0x14, 0x00, // k
0x00, 0x00, 0x1F, 0x00, 0x00, // l
0x1C, 0x02, 0x1C, 0x02, 0x1C, // m
0x1E, 0x04, 0x02, 0x1C, 0x00, // n
0x0C, 0x12, 0x12, 0x0C, 0x00, // o
0x1E, 0x0A, 0x0A, 0x04, 0x00, // p
0x04, 0x0A, 0x0A, 0x1E, 0x00, // q
0x00, 0x1E, 0x04, 0x02, 0x00, // r
0x02, 0x15, 0x15, 0x08, 0x00, // s
0x00, 0x02, 0x0F, 0x12, 0x00, // t
0x0E, 0x10, 0x10, 0x0E, 0x00, // u
0x06, 0x08, 0x10, 0x08, 0x06, // v
0x06, 0x18, 0x0C, 0x18, 0x06, // w
0x00, 0x12, 0x0C, 0x12, 0x00, // x
0x00, 0x16, 0x08, 0x06, 0x00, // y
0x12, 0x1A, 0x16, 0x12, 0x00, // z
0x00, 0x04, 0x0E, 0x11, 0x00, // {
0x00, 0x00, 0x1F, 0x00, 0x00, // |
0x00, 0x11, 0x0E, 0x04, 0x00, // }
0x04, 0x02, 0x04, 0x08, 0x04 // ~
};
#ifdef MENU_USE_UTF8
#define MENU_TEXT_5X5_UTF8CHARS 10
const utf8Char5x5_t characters_5x5_utf8[MENU_TEXT_5X5_UTF8CHARS] PROGMEM = {
{0xC2B0, {0x00, 0x02, 0x05, 0x02, 0x00}}, // °
{0xC2B5, {0x1E, 0x08, 0x08, 0x06, 0x00}}, // µ
{0xC384, {0x19, 0x0C, 0x0A, 0x0C, 0x19}}, // Ä
{0xC396, {0x0D, 0x12, 0x12, 0x0D, 0x00}}, // Ö
{0xC39C, {0x0D, 0x10, 0x10, 0x0D, 0x00}}, // Ü
{0xC39F, {0x1F, 0x01, 0x15, 0x1B, 0x00}}, // ß
{0xC3A4, {0x09, 0x14, 0x14, 0x1D, 0x00}}, // ä
{0xC3B6, {0x09, 0x14, 0x14, 0x09, 0x00}}, // ö
{0xC3BC, {0x0D, 0x10, 0x10, 0x0D, 0x00}}, // ü
{0xCEA9, {0x16, 0x19, 0x01, 0x19, 0x16}} // Ω
};
#else

#define MENU_TEXT_5X5_UTF8CHARS 0

#endif
#else

#define MENU_TEXT_5X5_UTF8CHARS 0

#endif

#ifdef MENU_TEXT_ENABLE_8X15

/*The font, defined by the characters_8x15 and characters_8x15_utf8 arrays
  is based on a converted and modified version of Droid Sans Mono.
  Droid Sans Mono is licensed under Apache license 2.0
*/
//source for the font: extra/8x15-marked.png
const char characters_8x15[MENU_CHARACTERS * MENU_TEXT_8X15_BYTES] PROGMEM = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //
0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, // !
0x00, 0x00, 0x38, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // "
0x00, 0x20, 0xF0, 0x28, 0x20, 0xF0, 0x28, 0x00, 0x01, 0x0D, 0x03, 0x01, 0x0F, 0x01, 0x01, 0x00, // #
0x00, 0x30, 0x58, 0xFC, 0xC8, 0x88, 0x00, 0x00, 0x00, 0x04, 0x04, 0x0F, 0x04, 0x07, 0x01, 0x00, // $
0x30, 0x48, 0x48, 0xB0, 0x60, 0x18, 0x00, 0x00, 0x00, 0x08, 0x06, 0x01, 0x06, 0x09, 0x09, 0x06, // %
0x00, 0x30, 0xF8, 0xC8, 0x68, 0x30, 0x00, 0x00, 0x02, 0x0F, 0x08, 0x08, 0x0B, 0x06, 0x0F, 0x00, // &
0x00, 0x00, 0x00, 0x38, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // '
0x00, 0x00, 0xC0, 0x70, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x07, 0x1C, 0x30, 0x00, 0x00, 0x00, // (
0x00, 0x00, 0x08, 0x18, 0x70, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x1C, 0x07, 0x00, 0x00, // )
0x00, 0x10, 0xD0, 0x3C, 0x70, 0xD0, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // *
0x00, 0x80, 0x80, 0xE0, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, // +
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x08, 0x00, 0x00, 0x00, // ,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, // -
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x00, // .
0x00, 0x00, 0x00, 0x80, 0x60, 0x18, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x03, 0x00, 0x00, 0x00, 0x00, // /
0x00, 0xF0, 0x18, 0x08, 0x08, 0x30, 0xE0, 0x00, 0x00, 0x03, 0x0C, 0x08, 0x08, 0x06, 0x03, 0x00, // 0
0x00, 0x00, 0x10, 0x18, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, // 1
0x00, 0x00, 0x08, 0x08, 0x88, 0xF0, 0x00, 0x00, 0x00, 0x0C, 0x0E, 0x0B, 0x09, 0x08, 0x08, 0x00, // 2
0x00, 0x10, 0x88, 0x88, 0x88, 0x70, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x07, 0x02, 0x00, // 3
0x00, 0x00, 0xC0, 0x20, 0x18, 0xF8, 0x00, 0x00, 0x02, 0x03, 0x02, 0x02, 0x02, 0x0F, 0x02, 0x00, // 4
0x00, 0x70, 0x58, 0x48, 0x48, 0x88, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x07, 0x03, 0x00, // 5
0x00, 0xE0, 0xF0, 0x48, 0x48, 0xC8, 0x80, 0x00, 0x00, 0x03, 0x0C, 0x08, 0x08, 0x0C, 0x03, 0x00, // 6
0x00, 0x08, 0x08, 0x08, 0x88, 0x78, 0x18, 0x00, 0x00, 0x00, 0x08, 0x0E, 0x01, 0x00, 0x00, 0x00, // 7
0x00, 0x30, 0xF8, 0x88, 0xC8, 0x78, 0x00, 0x00, 0x00, 0x07, 0x09, 0x08, 0x08, 0x0D, 0x06, 0x00, // 8
0x00, 0x70, 0x98, 0x88, 0x88, 0xD0, 0xE0, 0x00, 0x00, 0x00, 0x08, 0x08, 0x0C, 0x07, 0x01, 0x00, // 9
0x00, 0x00, 0x00, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x08, 0x00, 0x00, 0x00, // :
0x00, 0x00, 0x00, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, // ;
0x00, 0x80, 0x80, 0x40, 0x40, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x00, // <
0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, // =
0x00, 0x20, 0x20, 0x40, 0x40, 0x80, 0x80, 0x00, 0x00, 0x02, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, // >
0x00, 0x00, 0x08, 0x08, 0x88, 0x58, 0x30, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, // ?
0xC0, 0x30, 0xC8, 0x28, 0x28, 0xE8, 0x30, 0xC0, 0x03, 0x0C, 0x11, 0x12, 0x11, 0x13, 0x02, 0x00, // @
0x00, 0x00, 0xE0, 0x38, 0x38, 0xE0, 0x00, 0x00, 0x00, 0x0F, 0x01, 0x01, 0x01, 0x01, 0x0E, 0x00, // A
0x00, 0xF8, 0x88, 0x88, 0x88, 0xD8, 0x70, 0x00, 0x00, 0x0F, 0x08, 0x08, 0x08, 0x0D, 0x07, 0x00, // B
0x00, 0xE0, 0x30, 0x18, 0x08, 0x08, 0x08, 0x00, 0x00, 0x03, 0x06, 0x08, 0x08, 0x08, 0x08, 0x00, // C
0x00, 0xF8, 0x08, 0x08, 0x08, 0x10, 0xE0, 0x00, 0x00, 0x0F, 0x08, 0x08, 0x08, 0x04, 0x03, 0x00, // D
0x00, 0xF8, 0x88, 0x88, 0x88, 0x88, 0x08, 0x00, 0x00, 0x0F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, // E
0x00, 0x00, 0xF8, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, // F
0x00, 0xE0, 0x10, 0x08, 0x88, 0x88, 0x80, 0x00, 0x00, 0x03, 0x0C, 0x08, 0x08, 0x08, 0x0F, 0x00, // G
0x00, 0xF8, 0x80, 0x80, 0x80, 0x80, 0xF8, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, // H
0x00, 0x00, 0x08, 0xF8, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x0F, 0x08, 0x08, 0x00, 0x00, // I
0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x0C, 0x07, 0x00, 0x00, // J
0x00, 0xF8, 0x80, 0xC0, 0x60, 0x10, 0x08, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x01, 0x06, 0x0C, 0x00, // K
0x00, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, // L
0x00, 0xF8, 0x60, 0x80, 0x00, 0x80, 0x60, 0xF8, 0x00, 0x0F, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0F, // M
0x00, 0xF8, 0x30, 0xC0, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x03, 0x0E, 0x0F, 0x00, // N
0x00, 0xF0, 0x18, 0x08, 0x08, 0x18, 0xF0, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x0C, 0x07, 0x00, // O
0x00, 0xF8, 0x88, 0x88, 0x88, 0xD8, 0x70, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // P
0x00, 0xF0, 0x18, 0x08, 0x08, 0x18, 0xF0, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x3C, 0x27, 0x00, // Q
0x00, 0xF8, 0x88, 0x88, 0x88, 0x78, 0x30, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x01, 0x06, 0x0C, 0x00, // R
0x00, 0x70, 0x58, 0x88, 0x88, 0x08, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x0D, 0x07, 0x00, // S
0x08, 0x08, 0x08, 0xF8, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, // T
0x00, 0xF8, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x0C, 0x07, 0x00, // U
0x08, 0x78, 0xC0, 0x00, 0x00, 0xC0, 0x38, 0x00, 0x00, 0x00, 0x03, 0x0E, 0x0E, 0x01, 0x00, 0x00, // V
0x38, 0xC0, 0x00, 0xC0, 0xC0, 0x00, 0xC0, 0x38, 0x00, 0x0F, 0x0E, 0x01, 0x01, 0x0E, 0x0F, 0x00, // W
0x00, 0x18, 0x30, 0xC0, 0xE0, 0x30, 0x08, 0x00, 0x00, 0x0C, 0x03, 0x01, 0x01, 0x06, 0x0C, 0x00, // X
0x08, 0x18, 0x60, 0x80, 0xC0, 0x60, 0x18, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x01, 0x00, 0x00, 0x00, // Y
0x00, 0x08, 0x08, 0x88, 0x48, 0x38, 0x18, 0x00, 0x00, 0x0C, 0x0B, 0x09, 0x08, 0x08, 0x08, 0x00, // Z
0x00, 0x00, 0x00, 0xF8, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x20, 0x20, 0x00, 0x00, // [
0x00, 0x00, 0x38, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x0C, 0x00, 0x00, // backslash
0x00, 0x00, 0x08, 0x08, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x3F, 0x00, 0x00, 0x00, // ]
0x00, 0x80, 0x60, 0x18, 0x30, 0xC0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, // ^
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // _
0x00, 0x00, 0x00, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // `
0x00, 0x00, 0x20, 0x20, 0x20, 0xE0, 0x80, 0x00, 0x00, 0x06, 0x09, 0x09, 0x09, 0x07, 0x0F, 0x00, // a
0x00, 0xFC, 0x40, 0x20, 0x20, 0x60, 0x80, 0x00, 0x00, 0x0F, 0x04, 0x08, 0x08, 0x0C, 0x03, 0x00, // b
0x00, 0x80, 0x40, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x03, 0x04, 0x08, 0x08, 0x08, 0x00, 0x00, // c
0x00, 0xC0, 0x60, 0x20, 0x20, 0x60, 0xFC, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x04, 0x0F, 0x00, // d
0x00, 0xC0, 0x60, 0x20, 0x20, 0x60, 0xC0, 0x00, 0x00, 0x07, 0x0D, 0x09, 0x09, 0x09, 0x01, 0x00, // e
0x00, 0x20, 0x20, 0xF8, 0x24, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, // f
0x00, 0xC0, 0x60, 0x20, 0x20, 0xE0, 0x20, 0x00, 0x00, 0x71, 0x4F, 0x4A, 0x4A, 0x49, 0x30, 0x00, // g
0x00, 0xFC, 0x40, 0x20, 0x20, 0x60, 0xC0, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, // h
0x00, 0x00, 0x20, 0xE4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x0F, 0x08, 0x08, 0x08, 0x00, // i
0x00, 0x00, 0x20, 0x20, 0xE4, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x3F, 0x00, 0x00, 0x00, // j
0x00, 0xFC, 0x00, 0x80, 0xC0, 0x60, 0x20, 0x00, 0x00, 0x0F, 0x03, 0x01, 0x02, 0x04, 0x08, 0x00, // k
0x00, 0x00, 0x04, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x0F, 0x08, 0x08, 0x00, 0x00, // l
0x00, 0xE0, 0x20, 0xE0, 0x40, 0x20, 0xE0, 0x00, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x00, 0x0F, 0x00, // m
0x00, 0xE0, 0x40, 0x20, 0x20, 0x60, 0xC0, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, // n
0x00, 0xC0, 0x60, 0x20, 0x20, 0x60, 0xC0, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x0C, 0x07, 0x00, // o
0x00, 0xE0, 0x40, 0x20, 0x20, 0x60, 0x80, 0x00, 0x00, 0x7F, 0x04, 0x08, 0x08, 0x0C, 0x03, 0x00, // p
0x00, 0xC0, 0x60, 0x20, 0x20, 0x40, 0xE0, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x0C, 0x7F, 0x00, // q
0x00, 0x00, 0xE0, 0x40, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, // r
0x00, 0x40, 0xE0, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x08, 0x09, 0x09, 0x0E, 0x00, 0x00, // s
0x00, 0x20, 0x20, 0xF8, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x08, 0x08, 0x00, 0x00, // t
0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x04, 0x0F, 0x00, // u
0x00, 0xE0, 0x80, 0x00, 0x00, 0x80, 0x60, 0x00, 0x00, 0x00, 0x03, 0x0C, 0x0E, 0x03, 0x00, 0x00, // v
0xE0, 0x80, 0x00, 0xE0, 0xE0, 0x00, 0x80, 0xE0, 0x00, 0x07, 0x0F, 0x01, 0x01, 0x0F, 0x07, 0x00, // w
0x00, 0x20, 0x40, 0x80, 0x80, 0x60, 0x20, 0x00, 0x00, 0x08, 0x06, 0x03, 0x03, 0x04, 0x08, 0x00, // x
0x00, 0x60, 0x80, 0x00, 0x00, 0x80, 0x60, 0x00, 0x00, 0x40, 0x43, 0x3E, 0x0E, 0x03, 0x00, 0x00, // y
0x00, 0x20, 0x20, 0x20, 0xA0, 0x60, 0x20, 0x00, 0x00, 0x08, 0x0E, 0x0B, 0x08, 0x08, 0x08, 0x00, // z
0x00, 0x00, 0x00, 0x80, 0xF8, 0x08, 0x00, 0x00, 0x00, 0x01, 0x01, 0x03, 0x3E, 0x20, 0x00, 0x00, // {
0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00, // |
0x00, 0x08, 0x08, 0xF0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x1E, 0x03, 0x01, 0x00, 0x00, // }
0x00, 0x80, 0x40, 0x40, 0x80, 0x80, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // ~
};
#ifdef MENU_USE_UTF8
#define MENU_TEXT_8X15_UTF8CHARS 10
const utf8Char8x15_t characters_8x15_utf8[MENU_TEXT_8X15_UTF8CHARS] PROGMEM = {
{0xC2B0, {0x00, 0x00, 0x30, 0x48, 0x48, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}, // °
{0xC2B5, {0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x7F, 0x0C, 0x08, 0x08, 0x0C, 0x0F, 0x00}}, // µ
{0xC384, {0x00, 0x00, 0xE2, 0x38, 0x38, 0xC2, 0x00, 0x00, 0x08, 0x0F, 0x01, 0x01, 0x01, 0x01, 0x0E, 0x00}}, // Ä
{0xC396, {0x00, 0xF0, 0x19, 0x08, 0x08, 0x19, 0xF0, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x0C, 0x07, 0x00}}, // Ö
{0xC39C, {0x00, 0xF8, 0x01, 0x00, 0x00, 0x01, 0xF8, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x0C, 0x07, 0x00}}, // Ü
{0xC39F, {0x00, 0xF8, 0x0C, 0x04, 0xE4, 0x1C, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x08, 0x08, 0x09, 0x0F, 0x00}}, // ß
{0xC3A4, {0x00, 0x00, 0x28, 0x20, 0x20, 0xE8, 0x80, 0x00, 0x00, 0x06, 0x09, 0x09, 0x09, 0x07, 0x0F, 0x00}}, // ä
{0xC3B6, {0x00, 0xC0, 0x68, 0x20, 0x20, 0x68, 0xC0, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x0C, 0x07, 0x00}}, // ö
{0xC3BC, {0x00, 0xE0, 0x08, 0x00, 0x00, 0x08, 0xE0, 0x00, 0x00, 0x07, 0x0C, 0x08, 0x08, 0x04, 0x0F, 0x00}}, // ü
{0xCEA9, {0x00, 0xF0, 0x08, 0x08, 0x08, 0x08, 0xF0, 0x00, 0x08, 0x0B, 0x0E, 0x00, 0x00, 0x0E, 0x0B, 0x08}} // Ω
};
#else
#define MENU_TEXT_8X15_UTF8CHARS 0
#endif

#else

#define MENU_TEXT_8X15_UTF8CHARS 0

#endif

#ifdef MENU_USE_UTF8
  unsigned char g_menu_utf8_state;
  unsigned long g_menu_utf8_char;


static const char * menu_get_pBitmap(unsigned long cdrawx, unsigned char font) {
	unsigned int lowerlim, upperlim, hardlim, idx, utf8Code;
	const unsigned long * utf8CodeAddr;
	unsigned char bytesToData = 0;
#if (MENU_TEXT_FONT_CLASSES < 2)
	UNREFERENCED_PARAMETER(font);
#endif
	hardlim = 0;
	if (MENU_CHECK_FONT_5X5) {
		hardlim = MENU_TEXT_5X5_UTF8CHARS;
		bytesToData = (char *)(&(characters_5x5_utf8[0].bitmap)) - (char *)(&(characters_5x5_utf8[0].utf8Code)); //compiler evaluates this to a single constant
	}
	if (MENU_CHECK_FONT_5X7) {
		hardlim = MENU_TEXT_5X7_UTF8CHARS;
		bytesToData = (char *)(&(characters_5x7_utf8[0].bitmap)) - (char *)(&(characters_5x7_utf8[0].utf8Code)); //compiler evaluates this to a single constant
	}
	if (MENU_CHECK_FONT_8X15) {
		hardlim = MENU_TEXT_8X15_UTF8CHARS;
		bytesToData = (char *)(&(characters_8x15_utf8[0].bitmap)) - (char *)(&(characters_8x15_utf8[0].utf8Code)); //compiler evaluates this to a single constant
	}
	lowerlim = 0;
	upperlim = hardlim;
	utf8CodeAddr = NULL;
	while (upperlim > lowerlim) {
		idx = (upperlim - lowerlim) / 2 + lowerlim;
		if (MENU_CHECK_FONT_5X5) {
			utf8CodeAddr = &(characters_5x5_utf8[idx].utf8Code);
		}
		if (MENU_CHECK_FONT_5X7) {
			utf8CodeAddr = &(characters_5x7_utf8[idx].utf8Code);
		}
		if (MENU_CHECK_FONT_8X15) {
			utf8CodeAddr = &(characters_8x15_utf8[idx].utf8Code);
		}
		utf8Code = COPY4BYTES(utf8CodeAddr);
		//printf("idx= %i lower %i, upper %i , cmp = %lx<>%lx\n", idx, lowerlim, upperlim, utf8Code, cdrawx);
		if (idx >= hardlim)
			break;
		if (utf8Code == cdrawx)
			return ((char *)utf8CodeAddr + bytesToData); //this only works because the struct has the bitmap after the utf8 code
		if (utf8Code < cdrawx) {
			if (lowerlim == idx)
				break;
			lowerlim = idx;
		} else {
			if (upperlim == idx)
				break;
			upperlim = idx;
		}
		//printf("  idx= %i lower %i, upper %i , cmp = %lx<>%lx\n", idx, lowerlim, upperlim, data[idx].utf8Code, cdrawx);
	}
	return NULL; //not found
}
#endif


unsigned char menu_font_heigth(unsigned char font) {
	if (MENU_CHECK_FONT_5X5) {
		return 5;
	}
	if (MENU_CHECK_FONT_5X7) {
		if (font < 2)
			return 7; //normal 5x7 font
		return 8; //with underline
	}
	if (MENU_CHECK_FONT_8X15) {
		if (font < 6)
			return 15;
		return 16; //with underline
	}
	return 0; //might be unreachable
}

unsigned char menu_char_draw(SCREENPOS posx, SCREENPOS posy, unsigned char font, unsigned char cdraw) {
	unsigned char nunbyte, charwidth, nunbit, patternbyte, patternbit;
#if defined(MENU_TEXT_ENABLE_8X15)
	unsigned char copyedbytes[MENU_TEXT_8X15_BYTES];
#else
	unsigned char copyedbytes[MENU_TEXT_5X7_BYTES];
#endif
	SCREENPOS tempx, tempy;
	unsigned char byte_eq_count,nun;
	/* level of char shrinking:
	0: Fixed with
	1: Width defined by character within array, only empty colums are removed.
	2: three or four same colums -> remove one. five same colums -> remove two.
	3: two -> remove one. three or four -> remove two. five -> remove three
	*/
	unsigned char shrink = 0;
	unsigned char fontheight;
//------------------- UTF-8 decoding ----------------------
#if defined(MENU_USE_UTF8)
	unsigned long cdrawx;
	//catch utf8 chars
#ifndef MENU_ENABLE_SPECIAL_CHARS
	if (cdraw >= (MENU_CHARACTERS + MENU_CHARACTER_TABLE_OFFSET)) { //its something for utf-8
		if (!g_menu_utf8_state)  {
			//a new char begins
			g_menu_utf8_char = cdraw;
			if ((cdraw & 0xE0) == 0xC0) {
				g_menu_utf8_state = 1;
			}
			if ((cdraw & 0xF0) == 0xE0) {
				g_menu_utf8_state = 2;
			}
			if ((cdraw & 0xF8) == 0xF0) {
				g_menu_utf8_state = 3;
			}
			return -1; //symbol not complete, 255 = -1 do not go one pixel to the right
		} else {
			g_menu_utf8_char <<= 8;
			g_menu_utf8_char |= cdraw;
			g_menu_utf8_state--;
			if (g_menu_utf8_state) {
				return 0; //symbol not complete
			}
			cdrawx = g_menu_utf8_char; //symbol complete
		}
	} else {
		g_menu_utf8_state = 0; //reset state
		cdrawx = cdraw;
	}
#else
	//for backward compatibility only
	cdrawx = cdraw;
	if (cdraw == 196) cdrawx = 0xC384; //Ä
	if (cdraw == 214) cdrawx = 0xC396; //Ö
	if (cdraw == 220) cdrawx = 0xC39C; //Ü
	if (cdraw == 228) cdrawx = 0xC3A4; //ä
	if (cdraw == 246) cdrawx = 0xC3B6; //ö
	if (cdraw == 252) cdrawx = 0xC3BC; //ü
	if (cdraw == 223) cdrawx = 0xC39F; //ß
	if (cdraw == 0xB0) cdrawx = 0xC2B0; //°
#endif
#else
	unsigned char cdrawx = cdraw;
#endif
//--------------- special handling... ------------------------------------------
	if (MENU_CHECK_FONT_5X5) {
		if (cdraw == 9) { //its a tab, make same width as digits -> easy blinking (safe to use cdraw instead of cdrawx here)
			return 3;
		}
	}
//--------------- determine data source ----------------------------------------
	const char * bmpsource = NULL;
	char bmpsize = 0;
	char bmpwidth = 0;
	if (MENU_CHECK_FONT_5X5) {
		bmpsize = MENU_TEXT_5X5_BYTES;
		bmpwidth = MENU_TEXT_5X5_WIDTH;
	}
	if (MENU_CHECK_FONT_5X7) {
		bmpsize = MENU_TEXT_5X7_BYTES;
		bmpwidth = MENU_TEXT_5X7_WIDTH;
	}
	if (MENU_CHECK_FONT_8X15) {
		bmpsize = MENU_TEXT_8X15_BYTES;
		bmpwidth = MENU_TEXT_8X15_WIDTH;
	}
	if ((cdrawx < (MENU_CHARACTERS + MENU_CHARACTER_TABLE_OFFSET)) &&
	    (cdrawx >= MENU_CHARACTER_TABLE_OFFSET)) { //simple ASCII case, cdraw = cdrawx
		cdraw -= MENU_CHARACTER_TABLE_OFFSET;
		if (MENU_CHECK_FONT_5X5) {
			bmpsource = &(characters_5x5[MENU_TEXT_5X5_BYTES * cdraw]);
		}
		if (MENU_CHECK_FONT_5X7) {
			bmpsource = &(characters_5x7[MENU_TEXT_5X7_BYTES * cdraw]);
		}
		if (MENU_CHECK_FONT_8X15) {
			bmpsource = &(characters_8x15[MENU_TEXT_8X15_BYTES * cdraw]);
		}
	}
#if defined(MENU_USE_UTF8)
	else {
		bmpsource = menu_get_pBitmap(cdrawx, font);
	}
#endif
	if (!bmpsource) {
		MENU_DEBUGMSG("Error: no bitmap for char 0x%lx\r\n", (unsigned long)cdrawx);
		return 0;
	}
//--------------- copy bitmask to buffer ---------------------------------------
	COPYFUNCTION(copyedbytes, bmpsource, bmpsize);
//------------- prepare drawing by evaluating actual size of the character -----
	if (MENU_CHECK_FONT_5X5) {
		shrink = font - 12;
	}
	if (MENU_CHECK_FONT_5X7) {
		if ((font == 0) || (font == 2)) {
			shrink = 2;
		}
	}
	if (MENU_CHECK_FONT_8X15) {
		if ((font == 4) || (font == 6)) {
			shrink = 2;
		}
	}
	charwidth = 0;
	//automated shortening of chars
	if (shrink > 1) {
		byte_eq_count = 0;
		for (nun = 0; nun < bmpwidth-1; nun++) {
			byte_eq_count++;
			if (copyedbytes[nun] != copyedbytes[nun+1]) {
				byte_eq_count = 0;
			}
			if (MENU_CHECK_FONT_5X5) {
				if ((byte_eq_count == 1) && (shrink == 3)) {
					copyedbytes[nun] = 0;
				}
				if ((nun > 0) && (font == 14) && (byte_eq_count == 1) &&
			  	  (copyedbytes[nun-1]) && (cdraw >= ('0' - MENU_CHARACTER_TABLE_OFFSET)) &&
				    (cdraw <= ('9' - MENU_CHARACTER_TABLE_OFFSET))) {
					//shortens numbers to 3 pixel witdh, but no less. Relevant for 3 and 7.
					copyedbytes[nun] = 0;
				}
			}
			if (MENU_CHECK_FONT_8X15) {
				if (copyedbytes[nun + MENU_TEXT_8X15_WIDTH] != copyedbytes[nun + MENU_TEXT_8X15_WIDTH + 1]) {
					byte_eq_count = 0;
				}
			}
			if (byte_eq_count == 2) {
				//remove one byte, if it repeated three times
				copyedbytes[nun] = 0;
				if (MENU_CHECK_FONT_8X15) {
					copyedbytes[nun + MENU_TEXT_8X15_WIDTH] = 0;
				}
			}
			if (byte_eq_count == 4) {
				//if there are 5 bytes equal, we remove a second one
				copyedbytes[nun] = 0;
				if (MENU_CHECK_FONT_8X15) {
					copyedbytes[nun + MENU_TEXT_8X15_WIDTH] = 0;
				}
			}
		} //end of loop
	} //end if shrink != 0
	fontheight = menu_font_heigth(font);
	if (MENU_CHECK_FONT_UNDERLINED) { //only valid for font 2, 3, 6, 7
		fontheight--; //no underline here
	}
//------------------- actually draw --------------------------------------------
	for (nunbyte = 0; nunbyte < bmpwidth; nunbyte++) {
		tempx = posx+charwidth;
		if (tempx < posx+charwidth) {
			break; //prevent overflow to the left side of the screen
		}
		patternbyte = copyedbytes[nunbyte];
		patternbit = 1;
		for (nunbit = 0; nunbit < fontheight; nunbit++) {
			if (MENU_CHECK_FONT_8X15) {
				if (!patternbit) {
					patternbit = 1;
					patternbyte = copyedbytes[nunbyte + bmpwidth];
				}
			}
			tempy = posy+nunbit;
			if ((patternbyte & patternbit) != 0) {
				menu_screen_set(tempx, tempy, 1);
			} else {
				menu_screen_set(tempx, tempy, 0);
			}
			patternbit <<= 1;
		}       //end: inner loop
		if (MENU_CHECK_FONT_UNDERLINED) { //only valid for font 2, 3, 6, 7
			menu_screen_set(tempx, posy + fontheight, 1);//underline fonts
		}
		if ((copyedbytes[nunbyte] != 0) || (shrink == 0) ||
				((MENU_CHECK_FONT_8X15) && (copyedbytes[nunbyte + MENU_TEXT_8X15_WIDTH] != 0))) {
			charwidth++;
		}       //end: charwidth++
	}         //end: outer loop
#ifdef MENU_USE_FONT_14
	if ((font == 14) && (cdraw == ('1' - MENU_CHARACTER_TABLE_OFFSET))) {
		charwidth = 3;
	}
#endif
	if (MENU_CHECK_FONT_UNDERLINED) { //only valid for font 2, 3, 6, 7
		menu_screen_set(posx + charwidth, posy + fontheight, 1); //underline empty part between chars (this is not perfect on word ending)
	}
	return charwidth++;
}          //end: function
