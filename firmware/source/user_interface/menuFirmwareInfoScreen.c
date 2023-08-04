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

static void updateScreen(void);
static void handleEvent(uiEvent_t *ev);
static menuStatus_t menuFirmwareInfoExitCode = MENU_STATUS_SUCCESS;

menuStatus_t menuFirmwareInfoScreen(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		menuDataGlobal.endIndex = 0;
		updateScreen();
	}
	else
	{
		menuFirmwareInfoExitCode = MENU_STATUS_SUCCESS;
		if (ev->hasEvent)
		{
			handleEvent(ev);
		}
	}
	return menuFirmwareInfoExitCode;
}

static void updateScreen(void)
{
#if !defined(PLATFORM_GD77S)
	char buf[SCREEN_LINE_BUFFER_SIZE];
	char * const *radioModel;

	snprintf(buf, SCREEN_LINE_BUFFER_SIZE, "[ %s", GITVERSION);
	buf[9] = 0; // git hash id 7 char long;
	strcat(buf, (uiDataGlobal.dmrDisabled ? " F ]" : " D ]"));

	ucClearBuf();

#if defined(PLATFORM_GD77)
	radioModel = (char * const *)&currentLanguage->openGD77;
#elif defined(PLATFORM_DM1801)
	radioModel = (char * const *)&currentLanguage->openDM1801;
#elif defined(PLATFORM_DM1801A)
	radioModel = (char * const *)&currentLanguage->openDM1801A;
#elif defined(PLATFORM_RD5R)
	radioModel = (char * const *)&currentLanguage->openRD5R;
#endif

#if defined(PLATFORM_RD5R)
	ucPrintCentered(2, *radioModel, FONT_SIZE_3);
#else
	ucPrintCentered(5, *radioModel, FONT_SIZE_3);
#endif



#if defined(PLATFORM_RD5R)
	ucPrintCentered(14, currentLanguage->built, FONT_SIZE_2);
	ucPrintCentered(24,__TIME__, FONT_SIZE_2);
	ucPrintCentered(32,__DATE__, FONT_SIZE_2);
	ucPrintCentered(40, buf, FONT_SIZE_2);
#else
	ucPrintCentered(24, currentLanguage->built, FONT_SIZE_2);
	ucPrintCentered(34,__TIME__, FONT_SIZE_2);
	ucPrintCentered(44,__DATE__, FONT_SIZE_2);
	ucPrintCentered(54, buf, FONT_SIZE_2);

#endif

	voicePromptsInit();
	voicePromptsAppendPrompt(PROMPT_SILENCE);
	voicePromptsAppendLanguageString((const char * const *)radioModel);
	voicePromptsAppendLanguageString(&currentLanguage->built);
	voicePromptsAppendString(__TIME__);
	voicePromptsAppendString(__DATE__);
	voicePromptsAppendLanguageString(&currentLanguage->gitCommit);
	voicePromptsAppendString(buf);
	promptsPlayNotAfterTx();

	ucRender();
#endif
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

	if (KEYCHECK_SHORTUP_NUMBER(ev->keys)  && (BUTTONCHECK_DOWN(ev, BUTTON_SK2)))
	{
		saveQuickkeyMenuIndex(ev->keys.key, menuSystemGetCurrentMenuNumber(), 0, 0);
		return;
	}
}
