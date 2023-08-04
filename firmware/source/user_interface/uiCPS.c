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
#include "functions/trx.h"
#include "user_interface/menuSystem.h"

static void handleTick(void);

typedef enum
{
	LED_NONE,
	LED_RED,
	LED_GREEN
} blinkLed_t;

static blinkLed_t mode = LED_NONE;
static uint32_t nextPIT;
static int ledState = 0;
static const int PIT_COUNTS_PER_UPDATE = 5000;
static int radioMode;
static int radioBandWidth;


menuStatus_t uiCPS(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		menuDataGlobal.endIndex = 0;
		radioMode = trxGetMode();
		radioBandWidth = trxGetBandwidthIs25kHz();
		trxSetModeAndBandwidth(RADIO_MODE_NONE, radioBandWidth);
		// Just clear the display and turn on the
//		UC1701_clearBuf();
//		UC1701_render();
		nextPIT = PITCounter + PIT_COUNTS_PER_UPDATE;
	}
	else
	{
		if (PITCounter >= nextPIT)
		{
			nextPIT = PITCounter + PIT_COUNTS_PER_UPDATE;
			handleTick();
		}
	}
	return MENU_STATUS_SUCCESS;
}

void uiCPSUpdate(uiCPSCommand_t command, int x, int y, ucFont_t fontSize, ucTextAlign_t alignment, bool isInverted, char *szMsg)
{
	switch(command)
	{
		case CPS2UI_COMMAND_CLEARBUF:
			ucClearBuf();
			break;
		case CPS2UI_COMMAND_PRINT:
			ucPrintCore(x, y, szMsg, fontSize, alignment, isInverted);
			break;
		case CPS2UI_COMMAND_RENDER_DISPLAY:
			ucRender();
			displayLightTrigger(true);
			break;
		case CPS2UI_COMMAND_BACKLIGHT:
			displayLightTrigger(true);
			break;
		case CPS2UI_COMMAND_GREEN_LED:
			mode = LED_GREEN;// flash green LED
			break;
		case CPS2UI_COMMAND_RED_LED:
			mode = LED_RED;// flash red LED
			break;
		case CPS2UI_COMMAND_END:
		    LEDs_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
		    LEDs_PinWrite(GPIO_LEDred, Pin_LEDred, 0);
		    mode = LED_NONE;
		    trxSetRX();// Rx would be turned off at start of CPS by setting radio mode to none
		    trxSetModeAndBandwidth(radioMode, radioBandWidth);
			menuSystemPopAllAndDisplayRootMenu();
			break;
		default:
			break;
	}
}

static void handleTick(void)
{
	switch(mode)
	{
		case LED_GREEN:
			if (ledState == 0)
			{
				ledState = 1;
			    LEDs_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 1);
			}
			else
			{
				ledState = 0;
			    LEDs_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
			}
			break;

		case LED_RED:
			if (ledState == 0)
			{
				ledState = 1;
			    LEDs_PinWrite(GPIO_LEDred, Pin_LEDred, 1);
			}
			else
			{
				ledState = 0;
			    LEDs_PinWrite(GPIO_LEDred, Pin_LEDred, 0);
			}
			break;

		case LED_NONE:
		default:
			break;
	}
}