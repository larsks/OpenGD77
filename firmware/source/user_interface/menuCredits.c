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
#include "user_interface/menuSystem.h"
#include "user_interface/uiLocalisation.h"
#include "user_interface/uiUtilities.h"

static void updateScreen(bool isFirstRun);
static void handleEvent(uiEvent_t *ev);

//#define CREDIT_TEXT_LENGTH 33
#if defined(PLATFORM_RD5R)
static const int NUM_LINES_PER_SCREEN = 4;
#else
static const int NUM_LINES_PER_SCREEN = 6;
#endif

static const int NUM_CREDITS = 7;
static const char *creditTexts[] = {"Roger VK3KYY","Daniel F1RMB","Dzmitry EW1ADG","Colin G4EML","Alex DL4LEX","Kai DG4KLU","Jason VK7ZJA"};

static menuStatus_t menuCreditsExitCode = MENU_STATUS_SUCCESS;

menuStatus_t menuCredits(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		menuDataGlobal.endIndex = ((NUM_CREDITS >= NUM_LINES_PER_SCREEN) ? (NUM_CREDITS - NUM_LINES_PER_SCREEN) : (NUM_LINES_PER_SCREEN - NUM_CREDITS));
		updateScreen(true);
	}
	else
	{
		menuCreditsExitCode = MENU_STATUS_SUCCESS;
		if (ev->hasEvent)
		{
			handleEvent(ev);
		}
	}
	return menuCreditsExitCode;
}

static void updateScreen(bool isFirstRun)
{

	if (isFirstRun && (nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1))
	{
		voicePromptsInit();
		voicePromptsAppendPrompt(PROMPT_SILENCE);
		voicePromptsAppendLanguageString(&currentLanguage->credits);
		voicePromptsAppendLanguageString(&currentLanguage->menu);
		voicePromptsAppendPrompt(PROMPT_SILENCE);
		promptsPlayNotAfterTx();
	}

	ucClearBuf();
	menuDisplayTitle(currentLanguage->credits);


	for(int i = 0; i < NUM_LINES_PER_SCREEN; i++)
	{
		ucPrintCentered(i * 8 + 16, (char *)creditTexts[i + menuDataGlobal.currentItemIndex], FONT_SIZE_1);
	}

	ucRender();
}

static void handleEvent(uiEvent_t *ev)
{
	if (ev->events & BUTTON_EVENT)
	{
		if (repeatVoicePromptOnSK1(ev))
		{
			return;
		}
	}

	if (KEYCHECK_SHORTUP(ev->keys, KEY_RED) || KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		menuSystemPopPreviousMenu();
		return;
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
	{
		if (menuDataGlobal.currentItemIndex < ((NUM_CREDITS >= NUM_LINES_PER_SCREEN) ? (NUM_CREDITS - NUM_LINES_PER_SCREEN) : (NUM_LINES_PER_SCREEN - NUM_CREDITS)))
		{
			menuDataGlobal.currentItemIndex++;
		}

		updateScreen(false);
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
	{
		if (menuDataGlobal.currentItemIndex > 0)
		{
			menuDataGlobal.currentItemIndex--;
		}

		updateScreen(false);
	}

	if (KEYCHECK_SHORTUP_NUMBER(ev->keys)  && (BUTTONCHECK_DOWN(ev, BUTTON_SK2)))
	{
		saveQuickkeyMenuIndex(ev->keys.key, menuSystemGetCurrentMenuNumber(), 0, 0);
		return;
	}
}
