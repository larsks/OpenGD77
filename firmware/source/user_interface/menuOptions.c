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
#include "functions/settings.h"
#include "user_interface/menuSystem.h"
#include "user_interface/uiLocalisation.h"
#include "user_interface/uiUtilities.h"
#include "interfaces/wdog.h"
#include "utils.h"
#include "functions/rxPowerSaving.h"

static void updateScreen(bool isFirstRun);
static void handleEvent(uiEvent_t *ev);

static menuStatus_t menuOptionsExitCode = MENU_STATUS_SUCCESS;
enum OPTIONS_MENU_LIST { OPTIONS_MENU_TX_FREQ_LIMITS = 0U,
							OPTIONS_MENU_KEYPAD_TIMER_LONG, OPTIONS_MENU_KEYPAD_TIMER_REPEAT, OPTIONS_MENU_DMR_MONITOR_CAPTURE_TIMEOUT,
							OPTIONS_MENU_SCAN_DELAY, OPTIONS_MENU_SCAN_STEP_TIME, OPTIONS_MENU_SCAN_MODE, OPTIONS_MENU_SCAN_ON_BOOT,
							OPTIONS_MENU_SQUELCH_DEFAULT_VHF, OPTIONS_MENU_SQUELCH_DEFAULT_220MHz, OPTIONS_MENU_SQUELCH_DEFAULT_UHF,
							OPTIONS_MENU_PTT_TOGGLE, OPTIONS_MENU_HOTSPOT_TYPE, OPTIONS_MENU_TALKER_ALIAS_TX,
							OPTIONS_MENU_PRIVATE_CALLS,
							OPTIONS_MENU_USER_POWER,
							OPTIONS_MENU_TEMPERATURE_CALIBRATON, OPTIONS_MENU_BATTERY_CALIBRATON,
							OPTIONS_MENU_ECO_LEVEL,
							NUM_OPTIONS_MENU_ITEMS};

menuStatus_t menuOptions(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		menuDataGlobal.menuOptionsSetQuickkey = 0;
		menuDataGlobal.menuOptionsTimeout = 0;
		menuDataGlobal.newOptionSelected = true;
		menuDataGlobal.endIndex = NUM_OPTIONS_MENU_ITEMS;

		if (originalNonVolatileSettings.magicNumber == 0xDEADBEEF)
		{
			// Store original settings, used on cancel event.
			memcpy(&originalNonVolatileSettings, &nonVolatileSettings, sizeof(settingsStruct_t));
		}

		voicePromptsInit();
		voicePromptsAppendPrompt(PROMPT_SILENCE);
		voicePromptsAppendLanguageString(&currentLanguage->options);
		voicePromptsAppendLanguageString(&currentLanguage->menu);
		voicePromptsAppendPrompt(PROMPT_SILENCE);

		updateScreen(true);
		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}
	else
	{
		menuOptionsExitCode = MENU_STATUS_SUCCESS;

		if (ev->hasEvent || (menuDataGlobal.menuOptionsTimeout > 0))
		{
			handleEvent(ev);
		}
	}
	return menuOptionsExitCode;
}

static void updateScreen(bool isFirstRun)
{
	int mNum = 0;
	char buf[SCREEN_LINE_BUFFER_SIZE];
	char buf2[SCREEN_LINE_BUFFER_SIZE];
	char * const *leftSide = NULL;// initialize to please the compiler
	char * const *rightSideConst = NULL;// initialize to please the compiler
	char rightSideVar[SCREEN_LINE_BUFFER_SIZE];
	voicePrompt_t rightSideUnitsPrompt;
	const char * rightSideUnitsStr;

	ucClearBuf();
	bool settingOption = uiShowQuickKeysChoices(buf, SCREEN_LINE_BUFFER_SIZE, currentLanguage->options);

	// Can only display 3 of the options at a time menu at -1, 0 and +1
	for(int i = -1; i <= 1; i++)
	{
		if ((settingOption == false) || (i == 0))
		{
			mNum = menuGetMenuOffset(NUM_OPTIONS_MENU_ITEMS, i);
			buf[0] = 0;
			buf[2] = 0;
			leftSide = NULL;
			rightSideConst = NULL;
			rightSideVar[0] = 0;
			rightSideUnitsPrompt = PROMPT_SILENCE;// use PROMPT_SILENCE as flag that the unit has not been set
			rightSideUnitsStr = NULL;

			switch(mNum)
			{
				case OPTIONS_MENU_TX_FREQ_LIMITS:// Tx Freq limits
					leftSide = (char * const *)&currentLanguage->band_limits;
					switch(nonVolatileSettings.txFreqLimited)
					{
						case BAND_LIMITS_NONE:
							rightSideConst = (char * const *)(&currentLanguage->off);
							break;
						case BAND_LIMITS_ON_LEGACY_DEFAULT:
							rightSideConst = (char * const *)(&currentLanguage->on);
							break;
						case BAND_LIMITS_FROM_CPS:
							strcpy(rightSideVar,"CPS");
							break;
					}

					break;
				case OPTIONS_MENU_KEYPAD_TIMER_LONG:// Timer longpress
					leftSide = (char * const *)&currentLanguage->key_long;
					snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%1d.%1d", nonVolatileSettings.keypadTimerLong / 10, nonVolatileSettings.keypadTimerLong % 10);
					rightSideUnitsPrompt = PROMPT_SECONDS;
					rightSideUnitsStr = "s";
					break;
				case OPTIONS_MENU_KEYPAD_TIMER_REPEAT:// Timer repeat
					leftSide = (char * const *)&currentLanguage->key_repeat;
					snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%1d.%1d", nonVolatileSettings.keypadTimerRepeat/10, nonVolatileSettings.keypadTimerRepeat % 10);
					rightSideUnitsPrompt = PROMPT_SECONDS;
					rightSideUnitsStr = "s";
					break;
				case OPTIONS_MENU_DMR_MONITOR_CAPTURE_TIMEOUT:// DMR filtr timeout repeat
					leftSide = (char * const *)&currentLanguage->dmr_filter_timeout;
					snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%d", nonVolatileSettings.dmrCaptureTimeout);
					rightSideUnitsPrompt = PROMPT_SECONDS;
					rightSideUnitsStr = "s";
					break;
				case OPTIONS_MENU_SCAN_DELAY:// Scan hold and pause time
					leftSide = (char * const *)&currentLanguage->scan_delay;
					snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%d", nonVolatileSettings.scanDelay);
					rightSideUnitsPrompt = PROMPT_SECONDS;
					rightSideUnitsStr = "s";
					break;
				case OPTIONS_MENU_SCAN_STEP_TIME:// Scan step time
					leftSide = (char * const *)&currentLanguage->scan_dwell_time;
					snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%d", settingsGetScanStepTimeMilliseconds());
					rightSideUnitsPrompt = PROMPT_MILLISECONDS;
					rightSideUnitsStr = "ms";
					break;

				case OPTIONS_MENU_SCAN_MODE:// scanning mode
					leftSide = (char * const *)&currentLanguage->scan_mode;
					{
						const char * const *scanModes[] = { &currentLanguage->hold, &currentLanguage->pause, &currentLanguage->stop };
						rightSideConst = (char * const *)scanModes[nonVolatileSettings.scanModePause];
					}
					break;
				case OPTIONS_MENU_SCAN_ON_BOOT:
					leftSide = (char * const *)&currentLanguage->scan_on_boot;
					rightSideConst = settingsIsOptionBitSet(BIT_SCAN_ON_BOOT_ENABLED) ? (char * const *)&currentLanguage->on : (char * const *)&currentLanguage->off;
					break;
				case OPTIONS_MENU_SQUELCH_DEFAULT_VHF:
					leftSide = (char * const *)&currentLanguage->squelch_VHF;
					snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%d%%", (nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF] - 1) * 5);// 5% steps
					break;
				case OPTIONS_MENU_SQUELCH_DEFAULT_220MHz:
					leftSide = (char * const *)&currentLanguage->squelch_220;
					snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%d%%", (nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz] - 1) * 5);// 5% steps
					break;
				case OPTIONS_MENU_SQUELCH_DEFAULT_UHF:
					leftSide = (char * const *)&currentLanguage->squelch_UHF;
					snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%d%%", (nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF] - 1) * 5);// 5% steps
					break;
				case OPTIONS_MENU_PTT_TOGGLE:
					leftSide = (char * const *)&currentLanguage->ptt_toggle;
					rightSideConst = (char * const *)(settingsIsOptionBitSet(BIT_PTT_LATCH) ? &currentLanguage->on : &currentLanguage->off);
					break;
				case OPTIONS_MENU_HOTSPOT_TYPE:
					leftSide = (char * const *)&currentLanguage->hotspot_mode;
#if defined(PLATFORM_RD5R)
					rightSideConst = (char * const *)&currentLanguage->n_a;
#else
					// DMR (digital) is disabled.
					if (uiDataGlobal.dmrDisabled)
					{
						rightSideConst = (char * const *)&currentLanguage->n_a;
					}
					else
					{
						const char *hsTypes[] = {"MMDVM", "BlueDV" };
						if (nonVolatileSettings.hotspotType == 0)
						{
							rightSideConst = (char * const *)&currentLanguage->off;
						}
						else
						{
							snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%s", hsTypes[nonVolatileSettings.hotspotType - 1]);
						}
					}
#endif
					break;
				case OPTIONS_MENU_TALKER_ALIAS_TX:
					leftSide = (char * const *)&currentLanguage->transmitTalkerAlias;
					rightSideConst = (char * const *)(settingsIsOptionBitSet(BIT_TRANSMIT_TALKER_ALIAS) ? &currentLanguage->on : &currentLanguage->off);
					break;
				case OPTIONS_MENU_PRIVATE_CALLS:
					leftSide = (char * const *)&currentLanguage->private_call_handling;
					const char * const *allowPCOptions[] = { &currentLanguage->off, &currentLanguage->on, &currentLanguage->ptt, &currentLanguage->Auto};
					rightSideConst = (char * const *)allowPCOptions[nonVolatileSettings.privateCalls];
					break;
				case OPTIONS_MENU_USER_POWER:
					leftSide = (char * const *)&currentLanguage->user_power;
					snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%d", (nonVolatileSettings.userPower));
					break;
				case OPTIONS_MENU_TEMPERATURE_CALIBRATON:
					{
						int absValue = abs(nonVolatileSettings.temperatureCalibration);
						leftSide = (char * const *)&currentLanguage->temperature_calibration;
						snprintf(buf2, SCREEN_LINE_BUFFER_SIZE, "%c%d.%d", (nonVolatileSettings.temperatureCalibration == 0 ? ' ' :
								(nonVolatileSettings.temperatureCalibration > 0 ? '+' : '-')), ((absValue) / 2), ((absValue % 2) * 5));
						snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%s%s", buf2, currentLanguage->celcius);
					}
					break;
				case OPTIONS_MENU_BATTERY_CALIBRATON:
					{
						int batCal = nonVolatileSettings.batteryCalibration - 5;
						leftSide = (char * const *)&currentLanguage->battery_calibration;
						snprintf(buf2, SCREEN_LINE_BUFFER_SIZE, "%c0.%d", (batCal == 0 ? ' ' : (batCal > 0 ? '+' : '-')), abs(batCal));
						snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%sV", buf2);
					}
					break;
				case OPTIONS_MENU_ECO_LEVEL:
					leftSide = (char * const *)&currentLanguage->eco_level;
					snprintf(rightSideVar, SCREEN_LINE_BUFFER_SIZE, "%d", (nonVolatileSettings.ecoLevel));
					break;
			}

			snprintf(buf, SCREEN_LINE_BUFFER_SIZE, "%s:%s", *leftSide, (rightSideVar[0] ? rightSideVar : (rightSideConst ? *rightSideConst : "")));

			if (i == 0)
			{
				bool wasPlaying = voicePromptsIsPlaying();

				if (!isFirstRun && (menuDataGlobal.menuOptionsSetQuickkey == 0))
				{
					voicePromptsInit();
				}

				if (!wasPlaying || menuDataGlobal.newOptionSelected)
				{
					voicePromptsAppendLanguageString((const char * const *)leftSide);
				}

				if ((rightSideVar[0] != 0) || ((rightSideVar[0] == 0) && (rightSideConst == NULL)))
				{
					if (mNum == OPTIONS_MENU_TEMPERATURE_CALIBRATON)
					{
						voicePromptsAppendString(buf2);
						voicePromptsAppendLanguageString(&currentLanguage->celcius);
					}
					else if (mNum == OPTIONS_MENU_BATTERY_CALIBRATON)
					{
						voicePromptsAppendString(buf2);
						voicePromptsAppendPrompt(PROMPT_VOLTS);
					}
					else
					{
						voicePromptsAppendString(rightSideVar);
					}
				}
				else
				{
					voicePromptsAppendLanguageString((const char * const *)rightSideConst);
				}

				if (rightSideUnitsPrompt != PROMPT_SILENCE)
				{
					voicePromptsAppendPrompt(rightSideUnitsPrompt);
				}

				if (rightSideUnitsStr != NULL)
				{
					strncat(rightSideVar, rightSideUnitsStr, SCREEN_LINE_BUFFER_SIZE);
				}

				if (menuDataGlobal.menuOptionsTimeout != -1)
				{
					promptsPlayNotAfterTx();
				}
				else
				{
					menuDataGlobal.menuOptionsTimeout = 0;// clear flag indicating that a QuickKey has just been set
				}
			}

			// QuickKeys
			if (menuDataGlobal.menuOptionsTimeout > 0)
			{
				menuDisplaySettingOption(*leftSide, (rightSideVar[0] ? rightSideVar : *rightSideConst));
			}
			else
			{
				if (rightSideUnitsStr != NULL)
				{
					strncat(buf, rightSideUnitsStr, SCREEN_LINE_BUFFER_SIZE);
				}

				menuDisplayEntry(i, mNum, buf);
			}
		}
	}

	ucRender();
}

static void handleEvent(uiEvent_t *ev)
{
	bool isDirty = false;

	if (ev->events & BUTTON_EVENT)
	{
		if (repeatVoicePromptOnSK1(ev))
		{
			return;
		}
	}

	if ((menuDataGlobal.menuOptionsTimeout > 0) && (!BUTTONCHECK_DOWN(ev, BUTTON_SK2)))
	{
		menuDataGlobal.menuOptionsTimeout--;
		if (menuDataGlobal.menuOptionsTimeout == 0)
		{
			resetOriginalSettingsData();
			menuSystemPopPreviousMenu();
			return;
		}
	}
	if (ev->events & FUNCTION_EVENT)
	{
		isDirty = true;
		if ((QUICKKEY_TYPE(ev->function) == QUICKKEY_MENU) && (QUICKKEY_ENTRYID(ev->function) < NUM_OPTIONS_MENU_ITEMS))
		{
			menuDataGlobal.currentItemIndex = QUICKKEY_ENTRYID(ev->function);
		}
		if ((QUICKKEY_FUNCTIONID(ev->function) != 0))
		{
			menuDataGlobal.menuOptionsTimeout = 1000;
		}
	}

	if ((ev->events & KEY_EVENT) && (menuDataGlobal.menuOptionsSetQuickkey == 0) && (menuDataGlobal.menuOptionsTimeout == 0))
	{
		if (KEYCHECK_PRESS(ev->keys, KEY_DOWN) && (menuDataGlobal.endIndex != 0))
		{
			isDirty = true;
			menuSystemMenuIncrement(&menuDataGlobal.currentItemIndex, NUM_OPTIONS_MENU_ITEMS);
			menuDataGlobal.newOptionSelected = true;
			menuOptionsExitCode |= MENU_STATUS_LIST_TYPE;
		}
		else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
		{
			isDirty = true;
			menuSystemMenuDecrement(&menuDataGlobal.currentItemIndex, NUM_OPTIONS_MENU_ITEMS);
			menuDataGlobal.newOptionSelected = true;
			menuOptionsExitCode |= MENU_STATUS_LIST_TYPE;
		}
		else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
		{
			settingsSaveIfNeeded(true);
			resetOriginalSettingsData();
			rxPowerSavingSetLevel(nonVolatileSettings.ecoLevel);
			menuSystemPopAllAndDisplayRootMenu();
			return;
		}
		else if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
		{
			// Restore original settings.
			memcpy(&nonVolatileSettings, &originalNonVolatileSettings, sizeof(settingsStruct_t));
			settingsSaveIfNeeded(true);
			trxUpdate_PA_DAC_Drive();
			resetOriginalSettingsData();
			menuSystemPopPreviousMenu();
			return;
		}
		else if (KEYCHECK_SHORTUP_NUMBER(ev->keys) && BUTTONCHECK_DOWN(ev, BUTTON_SK2))
		{
				menuDataGlobal.menuOptionsSetQuickkey = ev->keys.key;
				isDirty = true;
		}
	}
	if ((ev->events & (KEY_EVENT | FUNCTION_EVENT)) && (menuDataGlobal.menuOptionsSetQuickkey == 0))
	{

		if (KEYCHECK_PRESS(ev->keys, KEY_RIGHT) || (QUICKKEY_FUNCTIONID(ev->function) == FUNC_RIGHT))
		{
			isDirty = true;
			menuDataGlobal.newOptionSelected = false;
			switch(menuDataGlobal.currentItemIndex)
			{
				case OPTIONS_MENU_TX_FREQ_LIMITS:
					if (nonVolatileSettings.txFreqLimited < BAND_LIMITS_FROM_CPS)
					{
						settingsIncrement(nonVolatileSettings.txFreqLimited, 1);
					}
					break;
				case OPTIONS_MENU_KEYPAD_TIMER_LONG:
					if (nonVolatileSettings.keypadTimerLong < 90)
					{
						settingsIncrement(nonVolatileSettings.keypadTimerLong, 1);
					}
					break;
				case OPTIONS_MENU_KEYPAD_TIMER_REPEAT:
					if (nonVolatileSettings.keypadTimerRepeat < 90)
					{
						settingsIncrement(nonVolatileSettings.keypadTimerRepeat, 1);
					}
					break;
				case OPTIONS_MENU_DMR_MONITOR_CAPTURE_TIMEOUT:
					if (nonVolatileSettings.dmrCaptureTimeout < 90)
					{
						settingsIncrement(nonVolatileSettings.dmrCaptureTimeout, 1);
					}
					break;
				case OPTIONS_MENU_SCAN_DELAY:
					if (nonVolatileSettings.scanDelay < 30)
					{
						settingsIncrement(nonVolatileSettings.scanDelay, 1);
					}
					break;
				case OPTIONS_MENU_SCAN_STEP_TIME:
					if (nonVolatileSettings.scanStepTime < 15)  // <30> + (15 * 30ms) MAX
					{
						settingsIncrement(nonVolatileSettings.scanStepTime, 1);
					}
					break;
				case OPTIONS_MENU_SCAN_MODE:
					if (nonVolatileSettings.scanModePause < SCAN_MODE_STOP)
					{
						settingsIncrement(nonVolatileSettings.scanModePause, 1);
					}
					break;
				case OPTIONS_MENU_SCAN_ON_BOOT:
					if (settingsIsOptionBitSet(BIT_SCAN_ON_BOOT_ENABLED) == false)
					{
						settingsSetOptionBit(BIT_SCAN_ON_BOOT_ENABLED, true);
					}
					break;
				case OPTIONS_MENU_SQUELCH_DEFAULT_VHF:
					if (nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF] < CODEPLUG_MAX_VARIABLE_SQUELCH)
					{
						settingsIncrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF], 1);
					}
					break;
				case OPTIONS_MENU_SQUELCH_DEFAULT_220MHz:
					if (nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz] < CODEPLUG_MAX_VARIABLE_SQUELCH)
					{
						settingsIncrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz], 1);
					}
					break;
				case OPTIONS_MENU_SQUELCH_DEFAULT_UHF:
					if (nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF] < CODEPLUG_MAX_VARIABLE_SQUELCH)
					{
						settingsIncrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF], 1);
					}
					break;
				case OPTIONS_MENU_PTT_TOGGLE:
					if (settingsIsOptionBitSet(BIT_PTT_LATCH) == false)
					{
						settingsSetOptionBit(BIT_PTT_LATCH, true);
					}
					break;
				case OPTIONS_MENU_HOTSPOT_TYPE:
#if !defined(PLATFORM_RD5R)
					if ((uiDataGlobal.dmrDisabled == false) && (nonVolatileSettings.hotspotType < HOTSPOT_TYPE_BLUEDV))
					{
						settingsIncrement(nonVolatileSettings.hotspotType, 1);
					}
#endif
					break;
				case OPTIONS_MENU_TALKER_ALIAS_TX:
					if (settingsIsOptionBitSet(BIT_TRANSMIT_TALKER_ALIAS) == false)
					{
						settingsSetOptionBit(BIT_TRANSMIT_TALKER_ALIAS, true);
					}
					break;
				case OPTIONS_MENU_PRIVATE_CALLS:
					// Note. Currently the "AUTO" option is not available
					if (nonVolatileSettings.privateCalls < ALLOW_PRIVATE_CALLS_PTT)
					{
						settingsIncrement(nonVolatileSettings.privateCalls, 1);
					}
					break;
				case OPTIONS_MENU_USER_POWER:
					{
						int newVal = (int)nonVolatileSettings.userPower;

						// Not the real max value of 4096, but trxUpdate_PA_DAC_Drive() will auto limit it to 4096
						// and it makes the logic easier and there is no functional difference
						newVal = SAFE_MIN((newVal + (BUTTONCHECK_DOWN(ev, BUTTON_SK2) ? 10 : 100)), 4100);

						settingsSet(nonVolatileSettings.userPower, newVal);
						trxUpdate_PA_DAC_Drive();
					}
					break;
				case OPTIONS_MENU_TEMPERATURE_CALIBRATON:
					if (nonVolatileSettings.temperatureCalibration < 20)
					{
						settingsIncrement(nonVolatileSettings.temperatureCalibration, 1);
					}
					break;
				case OPTIONS_MENU_BATTERY_CALIBRATON:
					if (nonVolatileSettings.batteryCalibration < 10) // = +0.5V as val is (batteryCalibration -5 ) /10
					{
						settingsIncrement(nonVolatileSettings.batteryCalibration,1);
					}
					break;
				case OPTIONS_MENU_ECO_LEVEL:
					if (nonVolatileSettings.ecoLevel < ECO_LEVEL_MAX)
					{
						settingsIncrement(nonVolatileSettings.ecoLevel, 1);
					}
					break;
			}
		}
		else if (KEYCHECK_PRESS(ev->keys, KEY_LEFT) || (QUICKKEY_FUNCTIONID(ev->function) == FUNC_LEFT))
		{
			isDirty = true;
			menuDataGlobal.newOptionSelected = false;
			switch(menuDataGlobal.currentItemIndex)
			{
				case OPTIONS_MENU_TX_FREQ_LIMITS:
					if (nonVolatileSettings.txFreqLimited > BAND_LIMITS_NONE)
					{
						settingsDecrement(nonVolatileSettings.txFreqLimited, 1);
					}
					break;
				case OPTIONS_MENU_KEYPAD_TIMER_LONG:
					if (nonVolatileSettings.keypadTimerLong > 1)
					{
						settingsDecrement(nonVolatileSettings.keypadTimerLong, 1);
					}
					break;
				case OPTIONS_MENU_KEYPAD_TIMER_REPEAT:
					if (nonVolatileSettings.keypadTimerRepeat > 1) // Don't set it to zero, otherwise watchdog may kicks in.
					{
						settingsDecrement(nonVolatileSettings.keypadTimerRepeat, 1);
					}
					break;
				case OPTIONS_MENU_DMR_MONITOR_CAPTURE_TIMEOUT:
					if (nonVolatileSettings.dmrCaptureTimeout > 1)
					{
						settingsDecrement(nonVolatileSettings.dmrCaptureTimeout, 1);
					}
					break;
				case OPTIONS_MENU_SCAN_DELAY:
					if (nonVolatileSettings.scanDelay > 1)
					{
						settingsDecrement(nonVolatileSettings.scanDelay, 1);
					}
					break;
				case OPTIONS_MENU_SCAN_STEP_TIME:
					if (nonVolatileSettings.scanStepTime > 0)
					{
						settingsDecrement(nonVolatileSettings.scanStepTime, 1);
					}
					break;
				case OPTIONS_MENU_SCAN_MODE:
					if (nonVolatileSettings.scanModePause > SCAN_MODE_HOLD)
					{
						settingsDecrement(nonVolatileSettings.scanModePause, 1);
					}
					break;
				case OPTIONS_MENU_SCAN_ON_BOOT:
					if (settingsIsOptionBitSet(BIT_SCAN_ON_BOOT_ENABLED))
					{
						settingsSetOptionBit(BIT_SCAN_ON_BOOT_ENABLED, false);
					}
					break;
				case OPTIONS_MENU_SQUELCH_DEFAULT_VHF:
					if (nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF] > 1)
					{
						settingsDecrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF], 1);
					}
					break;
				case OPTIONS_MENU_SQUELCH_DEFAULT_220MHz:
					if (nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz] > 1)
					{
						settingsDecrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz], 1);
					}
					break;
				case OPTIONS_MENU_SQUELCH_DEFAULT_UHF:
					if (nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF] > 1)
					{
						settingsDecrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF], 1);
					}
					break;
				case OPTIONS_MENU_PTT_TOGGLE:
					if (settingsIsOptionBitSet(BIT_PTT_LATCH))
					{
						settingsSetOptionBit(BIT_PTT_LATCH, false);
					}
					break;
				case OPTIONS_MENU_HOTSPOT_TYPE:
#if !defined(PLATFORM_RD5R)
					if ((uiDataGlobal.dmrDisabled == false) && (nonVolatileSettings.hotspotType > HOTSPOT_TYPE_OFF))
					{
						settingsDecrement(nonVolatileSettings.hotspotType, 1);
					}
#endif
					break;
				case OPTIONS_MENU_TALKER_ALIAS_TX:
					if (settingsIsOptionBitSet(BIT_TRANSMIT_TALKER_ALIAS))
					{
						settingsSetOptionBit(BIT_TRANSMIT_TALKER_ALIAS, false);
					}
					break;
				case OPTIONS_MENU_PRIVATE_CALLS:
					if (nonVolatileSettings.privateCalls > 0)
					{
						settingsDecrement(nonVolatileSettings.privateCalls, 1);
					}
					break;
				case OPTIONS_MENU_USER_POWER:
					{
						int newVal = (int)nonVolatileSettings.userPower;

						// Not the real max value of 4096, but trxUpdate_PA_DAC_Drive() will auto limit it to 4096
						// and it makes the logic easier and there is no functional difference
						newVal = SAFE_MAX((newVal - (BUTTONCHECK_DOWN(ev, BUTTON_SK2) ? 10 : 100)), 0);

						settingsSet(nonVolatileSettings.userPower, newVal);
						trxUpdate_PA_DAC_Drive();
					}
					break;
				case OPTIONS_MENU_TEMPERATURE_CALIBRATON:
					if (nonVolatileSettings.temperatureCalibration > -20)
					{
						settingsDecrement(nonVolatileSettings.temperatureCalibration, 1);
					}
					break;
				case OPTIONS_MENU_BATTERY_CALIBRATON:
					if (nonVolatileSettings.batteryCalibration > 0)
					{
						settingsDecrement(nonVolatileSettings.batteryCalibration, 1);
					}
					break;
				case OPTIONS_MENU_ECO_LEVEL:
					if (nonVolatileSettings.ecoLevel > 0)
					{
						settingsDecrement(nonVolatileSettings.ecoLevel, 1);
					}
					break;
			}
		}
		else if ((ev->keys.event & KEY_MOD_PRESS) && (menuDataGlobal.menuOptionsTimeout > 0))
		{
			menuDataGlobal.menuOptionsTimeout = 0;
			resetOriginalSettingsData();
			menuSystemPopPreviousMenu();
			return;
		}
	}

	if ((ev->events & KEY_EVENT) && (menuDataGlobal.menuOptionsSetQuickkey != 0) && (menuDataGlobal.menuOptionsTimeout == 0))
	{
		if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
		{
			menuDataGlobal.menuOptionsSetQuickkey = 0;
			menuDataGlobal.menuOptionsTimeout = 0;
			menuOptionsExitCode |= MENU_STATUS_ERROR;
		}
		else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
		{
			saveQuickkeyMenuIndex(menuDataGlobal.menuOptionsSetQuickkey, menuSystemGetCurrentMenuNumber(), menuDataGlobal.currentItemIndex, 0);
			menuDataGlobal.menuOptionsSetQuickkey = 0;
		}
		else if (KEYCHECK_SHORTUP(ev->keys, KEY_LEFT))
		{
			saveQuickkeyMenuIndex(menuDataGlobal.menuOptionsSetQuickkey, menuSystemGetCurrentMenuNumber(), menuDataGlobal.currentItemIndex, FUNC_LEFT);
			menuDataGlobal.menuOptionsSetQuickkey = 0;
		}
		else if (KEYCHECK_SHORTUP(ev->keys, KEY_RIGHT))
		{
			saveQuickkeyMenuIndex(menuDataGlobal.menuOptionsSetQuickkey, menuSystemGetCurrentMenuNumber(), menuDataGlobal.currentItemIndex, FUNC_RIGHT);
			menuDataGlobal.menuOptionsSetQuickkey = 0;
		}
		isDirty = true;
	}

	if (isDirty)
	{
		updateScreen(false);
	}
}
