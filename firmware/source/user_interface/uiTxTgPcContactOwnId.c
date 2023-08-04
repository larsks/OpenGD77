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
#include "functions/codeplug.h"
#include "hardware/HR-C6000.h"
#include "functions/settings.h"
#include "functions/ticks.h"
#include "user_interface/menuSystem.h"
#include "user_interface/uiUtilities.h"
#include "user_interface/uiLocalisation.h"
#include "functions/voicePrompts.h"

static char digits[9];
static int pcIdx;
static struct_codeplugContact_t contact;

static void updateCursor(void);
static void updateScreen(bool inputModeHasChanged);
static void handleEvent(uiEvent_t *ev);
static void announceContactName(void);

static const uint32_t CURSOR_UPDATE_TIMEOUT = 500;

enum DISPLAY_MENU_LIST { ENTRY_TG = 0, ENTRY_PC, ENTRY_SELECT_CONTACT, ENTRY_USER_DMR_ID, NUM_ENTRY_ITEMS};
static const char *menuName[NUM_ENTRY_ITEMS];

static menuStatus_t menuNumericalExitStatus = MENU_STATUS_SUCCESS;


// public interface
menuStatus_t menuNumericalEntry(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		menuName[ENTRY_TG] = currentLanguage->tg_entry;
		menuName[ENTRY_PC] = currentLanguage->pc_entry;
		menuName[ENTRY_SELECT_CONTACT] = currentLanguage->contact;
		menuName[ENTRY_USER_DMR_ID] = ((uiDataGlobal.manualOverrideDMRId == 0) && (trxDMRID == uiDataGlobal.userDMRId)) ? currentLanguage->user_dmr_id : currentLanguage->dmr_id;
		menuDataGlobal.currentItemIndex = ENTRY_TG;
		menuDataGlobal.endIndex = NUM_ENTRY_ITEMS;
		digits[0] = 0;
		pcIdx = 0;
		updateScreen(true);
		return (MENU_STATUS_INPUT_TYPE | MENU_STATUS_SUCCESS);
	}
	else
	{
		// Clear input type beep for AudioAssist
		menuNumericalExitStatus = MENU_STATUS_SUCCESS;

		if (ev->events == EVENT_BUTTON_NONE)
		{
			if ((menuDataGlobal.currentItemIndex != ENTRY_SELECT_CONTACT) && (strlen(digits) <= NUM_PC_OR_TG_DIGITS))
			{
				updateCursor();
			}
		}
		else
		{
			if (ev->hasEvent)
			{
				handleEvent(ev);
			}
			else
			{
				if ((menuDataGlobal.currentItemIndex != ENTRY_SELECT_CONTACT) && (strlen(digits) <= NUM_PC_OR_TG_DIGITS))
				{
					updateCursor();
				}
			}
		}
	}
	return menuNumericalExitStatus;
}

static void updateCursor(void)
{
	size_t sLen;

	// Display blinking cursor only when digits could be entered.
	if ((menuDataGlobal.currentItemIndex != ENTRY_SELECT_CONTACT) && ((sLen = strlen(digits)) <= NUM_PC_OR_TG_DIGITS))
	{
		static uint32_t lastBlink = 0;
		static bool     blink = false;
		uint32_t        m = fw_millis();

		if ((m - lastBlink) > CURSOR_UPDATE_TIMEOUT)
		{
			sLen *= 8;

			ucPrintCore((((DISPLAY_SIZE_X - sLen) >> 1) + sLen), (DISPLAY_SIZE_Y / 2), "_", FONT_SIZE_3, 0, blink);

			blink = !blink;
			lastBlink = m;

			ucRender();
		}
	}
}

static void updateScreen(bool inputModeHasChanged)
{
	char buf[33];
	size_t sLen = strlen(menuName[menuDataGlobal.currentItemIndex]) * 8;
	int16_t y = 8;

	ucClearBuf();

	ucDrawRoundRectWithDropShadow(2, y - 1, (DISPLAY_SIZE_X - 6), ((DISPLAY_SIZE_Y / 8) - 1) * 3, 3, true);

	// Not really centered, off by 2 pixels
	ucPrintAt(((DISPLAY_SIZE_X - sLen) >> 1) - 2, y, (char *)menuName[menuDataGlobal.currentItemIndex], FONT_SIZE_3);


	if (inputModeHasChanged)
	{
		voicePromptsInit();
		switch(menuDataGlobal.currentItemIndex)
		{
			case ENTRY_TG:
				voicePromptsAppendLanguageString(&currentLanguage->tg_entry);
				break;
			case ENTRY_PC:
				voicePromptsAppendLanguageString(&currentLanguage->pc_entry);
				break;
			case ENTRY_SELECT_CONTACT:
				voicePromptsAppendPrompt(PROMPT_CONTACT);
				{
					char buf2[17];
					codeplugUtilConvertBufToString(contact.name, buf2, 16);
					voicePromptsAppendString(buf2);
				}
				break;
			case ENTRY_USER_DMR_ID:
				voicePromptsAppendString("ID");
				break;
		}

		promptsPlayNotAfterTx();
	}

	if (pcIdx == 0)
	{
		ucPrintCentered((DISPLAY_SIZE_Y / 2), (char *)digits, FONT_SIZE_3);
	}
	else
	{
		codeplugUtilConvertBufToString(contact.name, buf, 16);
		ucPrintCentered((DISPLAY_SIZE_Y / 2), buf, FONT_SIZE_3);
		ucPrintCentered((DISPLAY_SIZE_Y - 12), (char *)digits, FONT_SIZE_1);
	}

	ucRender();
}

// curIdx: CODEPLUG_CONTACTS_MIN .. CODEPLUG_CONTACTS_MAX, if equal to 0 it means the previous index was unknown
static int getNextContact(int curIdx, bool next, struct_codeplugContact_t *ct)
{
	int idx = (curIdx != 0) ? (curIdx - 1) : (next ? (CODEPLUG_CONTACTS_MAX - 1) : 0);
	int startIdx = idx;

	do {
		idx = ((next ? (idx + 1) : (idx + (CODEPLUG_CONTACTS_MAX - 1)))) % CODEPLUG_CONTACTS_MAX;

		codeplugContactGetDataForIndex((idx + 1), ct);

	} while ((startIdx != idx) && ((*ct).name[0] == 0xff)); // will stops after one turn, maximum.

	return (((curIdx == 0) && ((*ct).name[0] == 0xff)) ? curIdx : (idx + 1));
}

static void announceContactName(void)
{
	if (nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1)
	{
		char buf[17];
		codeplugUtilConvertBufToString(contact.name, buf, 16);
		voicePromptsInit();
		voicePromptsAppendString(buf);
		voicePromptsPlay();
	}
}

static void handleEvent(uiEvent_t *ev)
{
	size_t sLen;
	uint32_t tmpID;

	if ((nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1) && (ev->events & BUTTON_EVENT))
	{
		if (BUTTONCHECK_SHORTUP(ev, BUTTON_SK1))
		{
			if (!voicePromptsIsPlaying())
			{
				voicePromptsInit();
				switch(menuDataGlobal.currentItemIndex)
				{
					case ENTRY_TG:
						voicePromptsAppendPrompt(PROMPT_TALKGROUP);
						voicePromptsAppendString(digits);
						break;
					case ENTRY_PC:
						voicePromptsAppendLanguageString(&currentLanguage->private_call);
						voicePromptsAppendString(digits);
						break;
					case ENTRY_SELECT_CONTACT:
						voicePromptsAppendPrompt(PROMPT_CONTACT);
						{
							char buf[17];
							codeplugUtilConvertBufToString(contact.name, buf, 16);
							voicePromptsAppendString(buf);
						}
						break;
					case ENTRY_USER_DMR_ID:
						voicePromptsAppendString("ID");
						voicePromptsAppendString(digits);
						break;
				}
				voicePromptsPlay();
			}
			else
			{
				voicePromptsTerminate();
			}
		}
		return;
	}

	if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
	{
		menuSystemPopPreviousMenu();
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		tmpID = (uint32_t)atoi(digits);

		if (tmpID <= MAX_TG_OR_PC_VALUE)
		{
			bool userIDEntered = (menuDataGlobal.currentItemIndex == ENTRY_USER_DMR_ID);

			if (userIDEntered == false)
			{
				if ((menuDataGlobal.currentItemIndex == ENTRY_PC) || ((pcIdx != 0) && (contact.callType == CONTACT_CALLTYPE_PC)))
				{
					setOverrideTGorPC(tmpID, true);
				}
				else
				{
					setOverrideTGorPC(tmpID, false);
				}

				// Apply TS override, if any
				if (menuDataGlobal.currentItemIndex == ENTRY_SELECT_CONTACT)
				{
					tsSetFromContactOverride(((menuSystemGetRootMenuNumber() == UI_CHANNEL_MODE) ? CHANNEL_CHANNEL : (CHANNEL_VFO_A + nonVolatileSettings.currentVFONumber)), &contact);
				}
			}
			else
			{
				uiDataGlobal.manualOverrideDMRId = tmpID;

				// The user's DMR ID has been entered, possibly clear the override.
				uint32_t chanIDOverride = codeplugChannelGetOptionalDMRID(currentChannelData);
				if ((uiDataGlobal.manualOverrideDMRId == uiDataGlobal.userDMRId) &&
						((chanIDOverride == 0) || (uiDataGlobal.manualOverrideDMRId == chanIDOverride)))
				{
					uiDataGlobal.manualOverrideDMRId = tmpID = 0;
				}

				if (tmpID != 0)
				{
					trxDMRID = uiDataGlobal.manualOverrideDMRId;
				}
				else
				{
					trxDMRID = uiDataGlobal.userDMRId;
				}

				if (BUTTONCHECK_DOWN(ev, BUTTON_SK2))
				{
					// make the change to DMR ID permanent if Function + Green is pressed
					codeplugSetUserDMRID(trxDMRID);
					uiDataGlobal.userDMRId = trxDMRID;

					if ((uiDataGlobal.manualOverrideDMRId != 0) &&
							((chanIDOverride == 0) || (uiDataGlobal.manualOverrideDMRId == chanIDOverride)))
					{
						uiDataGlobal.manualOverrideDMRId = 0;
					}
				}
			}

			uiDataGlobal.VoicePrompts.inhibitInitial = true;
			menuSystemPopAllAndDisplayRootMenu();

			if (userIDEntered == false)
			{
				announceItem(PROMPT_SEQUENCE_CONTACT_TG_OR_PC, PROMPT_THRESHOLD_3);
			}
			return;
		}
		else
		{
			menuNumericalExitStatus |= MENU_STATUS_ERROR;
		}

	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_HASH))
	{
		pcIdx = 0;

		menuNumericalExitStatus |= MENU_STATUS_INPUT_TYPE;

		if ((BUTTONCHECK_DOWN(ev, BUTTON_SK2) != 0) && (menuDataGlobal.currentItemIndex == ENTRY_SELECT_CONTACT))
		{
			snprintf(digits, 8, "%u", trxDMRID);
			menuDataGlobal.currentItemIndex = ENTRY_USER_DMR_ID;
		}
		else
		{
			menuDataGlobal.currentItemIndex++;
			if (menuDataGlobal.currentItemIndex > ENTRY_SELECT_CONTACT)
			{
				digits[0] = 0;
				menuDataGlobal.currentItemIndex = ENTRY_TG;
			}
			else
			{
				if (menuDataGlobal.currentItemIndex == ENTRY_SELECT_CONTACT)
				{
					menuNumericalExitStatus &= ~MENU_STATUS_INPUT_TYPE;

					pcIdx = getNextContact(0, true, &contact);

					if (pcIdx != 0)
					{
						itoa(contact.tgNumber, digits, 10);
						menuNumericalExitStatus |= (MENU_STATUS_LIST_TYPE | MENU_STATUS_FORCE_FIRST);
					}
				}
			}
		}
		updateScreen(true);
	}

	if (menuDataGlobal.currentItemIndex == ENTRY_SELECT_CONTACT)
	{
		int idx = pcIdx;

		if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
		{
			idx = getNextContact(pcIdx, true, &contact);
			announceContactName();
		}
		else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
		{
			idx = getNextContact(pcIdx, false, &contact);
			announceContactName();
		}

		if (pcIdx != idx)
		{
			pcIdx = idx;
			itoa(contact.tgNumber, digits, 10);

			if (pcIdx == 1)
			{
				menuNumericalExitStatus |= (MENU_STATUS_LIST_TYPE | MENU_STATUS_FORCE_FIRST);
			}

			updateScreen(false);
		}
	}
	else
	{
		if ((sLen = strlen(digits)) <= NUM_PC_OR_TG_DIGITS)
		{
			bool refreshScreen = false;

			// Inc / Dec entered value.
			if (KEYCHECK_PRESS(ev->keys, KEY_UP) || KEYCHECK_PRESS(ev->keys, KEY_DOWN))
			{
				if (strlen(digits))
				{
					unsigned long int ccs7 = strtoul(digits, NULL, 10);

					if (KEYCHECK_PRESS(ev->keys, KEY_UP))
					{
						if (ccs7 < MAX_TG_OR_PC_VALUE)
						{
							ccs7++;
						}
						refreshScreen = true;
					}
					else
					{
						if (ccs7 > 1)
						{
							ccs7--;
						}
						refreshScreen = true;
					}

					if (refreshScreen)
					{
						sprintf(digits, "%lu", ccs7);
					}
				}
			} // Delete a digit
			else if (KEYCHECK_PRESS(ev->keys, KEY_LEFT))
			{
				if ((sLen = strlen(digits)) > 0)
				{
					digits[sLen - 1] = 0;
					refreshScreen = true;
				}
			}
			else
			{
				// Add a digit
				if (sLen < NUM_PC_OR_TG_DIGITS)
				{
					int keyval = menuGetKeypadKeyValue(ev, true);

					if (keyval != 99)
					{
						char c[2] = {0, 0};
						c[0] = keyval + '0';

						if (nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1)
						{
							voicePromptsInit();
							voicePromptsAppendString(c);
							voicePromptsPlay();
						}

						strcat(digits, c);
						refreshScreen = true;
					}
				}
			}

			if (refreshScreen)
			{
				updateScreen(false);
			}

			updateCursor();
		}
	}
}
