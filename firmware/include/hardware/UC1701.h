/*
 * Copyright (C) 2019-2021 Roger Clark, VK3KYY / G4KYF
 *                         Daniel Caujolle-Bert, F1RMB
 *
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. Use of this source code or binary releases for commercial purposes is strictly forbidden. This includes, without limitation,
 *    incorporation in a commercial product or incorporation into a product or project which allows commercial use.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _OPENGD77_UC1701_H_
#define _OPENGD77_UC1701_H_

#include <stdbool.h>
#include <math.h>
#include <FreeRTOS.h>
#include <task.h>


typedef enum
{
	FONT_SIZE_1 = 0,
	FONT_SIZE_1_BOLD,
	FONT_SIZE_2,
	FONT_SIZE_3,
	FONT_SIZE_4
} ucFont_t;

typedef enum
{
	TEXT_ALIGN_LEFT = 0,
	TEXT_ALIGN_CENTER,
	TEXT_ALIGN_RIGHT
} ucTextAlign_t;

typedef enum
{
	CHOICE_OK = 0,
	CHOICE_YESNO,
	CHOICE_DISMISS,
	CHOICES_OKARROWS,// QuickKeys
	CHOICES_NUM
} ucChoice_t;

#if defined(PLATFORM_RD5R)
#define FONT_SIZE_3_HEIGHT                        8
#define DISPLAY_SIZE_Y                           48
#else
#define FONT_SIZE_3_HEIGHT                       16
#define DISPLAY_SIZE_Y                           64
#endif

#define DISPLAY_SIZE_X                          128
#define DISPLAY_NUMBER_OF_ROWS  (DISPLAY_SIZE_Y / 8)



void displayBegin(bool isInverted);
void displayClearBuf(void);
void displayClearRows(int16_t startRow, int16_t endRow, bool isInverted);
void displayRenderWithoutNotification(void);
void displayRender(void);
void displayRenderRows(int16_t startRow, int16_t endRow);
void displayPrintCentered(uint8_t y,const  char *text, ucFont_t fontSize);
void displayPrintAt(uint8_t x, uint8_t y,const  char *text, ucFont_t fontSize);
int displayPrintCore(int16_t x, int16_t y,const char *szMsg, ucFont_t fontSize, ucTextAlign_t alignment, bool isInverted);

int16_t displaySetPixel(int16_t x, int16_t y, bool color);

void displayDrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color);
void displayDrawFastVLine(int16_t x, int16_t y, int16_t h, bool color);
void displayDrawFastHLine(int16_t x, int16_t y, int16_t w, bool color);

void displayDrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, bool color);
void displayDrawCircle(int16_t x0, int16_t y0, int16_t r, bool color);
void displayFillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, bool color);
void displayFillCircle(int16_t x0, int16_t y0, int16_t r, bool color);

void displayDrawEllipse(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color);

void displayDrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool color);
void displayFillTriangle ( int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool color);

void displayFillArc(uint16_t x, uint16_t y, uint16_t radius, uint16_t thickness, float start, float end, bool color);

void displayDrawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, bool color);
void displayFillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, bool color);
void displayDrawRoundRectWithDropShadow(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, bool color);

void displayDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool color);
void displayFillRect(int16_t x, int16_t y, int16_t width, int16_t height, bool isInverted);
void displayDrawRectWithDropShadow(int16_t x, int16_t y, int16_t w, int16_t h, bool color);

void displayDrawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, bool color);
void displayDrawXBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, bool color);

void displaySetContrast(uint8_t contrast);
void displaySetInverseVideo(bool isInverted);
void displaySetDisplayPowerMode(bool wake);

void displayDrawChoice(ucChoice_t choice, bool clearRegion);

uint8_t *displayGetScreenBuffer(void);
void displayRestorePrimaryScreenBuffer(void);
uint8_t *displayGetPrimaryScreenBuffer(void);
void displayOverrideScreenBuffer(uint8_t *buffer);


#endif /* _OPENGD77_UC1701_H_ */
