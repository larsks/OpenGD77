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

#include "user_interface/uiGlobals.h"

const int DBM_LEVELS[16] = {
		SMETER_S0, SMETER_S1, SMETER_S2, SMETER_S3, SMETER_S4, SMETER_S5, SMETER_S6, SMETER_S7, SMETER_S8,
		SMETER_S9, SMETER_S9_10, SMETER_S9_20, SMETER_S9_30, SMETER_S9_40, SMETER_S9_50, SMETER_S9_60
};


const char *POWER_LEVELS[]                  = { "50", "250", "500", "750", "1", "2", "3", "4", "5", "+W-"};
const char *POWER_LEVEL_UNITS[]             = { "mW", "mW",  "mW",  "mW",  "W", "W", "W", "W", "W", ""};
const char *DMR_DESTINATION_FILTER_LEVELS[] = { "TG", "Ct", "RxG" };
const char *ANALOG_FILTER_LEVELS[]          = { "CTCSS|DCS" };


uiDataGlobal_t uiDataGlobal =
{
		.userDMRId                    = 0x00,
		.receivedPcId 	              = 0x00, // No current Private call awaiting acceptance
		.tgBeforePcMode 	          = 0,    // No TG saved, prior to a Private call being accepted.
		.displayQSOState              = QSO_DISPLAY_DEFAULT_SCREEN,
		.displayQSOStatePrev          = QSO_DISPLAY_DEFAULT_SCREEN,
		.displaySquelch               = false,
		.isDisplayingQSOData          = false,
		.displayChannelSettings       = false,
		.reverseRepeater              = false,
		.currentSelectedChannelNumber = 0,
		.currentSelectedContactIndex  = 0,
		.lastHeardCount               = 0,
		.dmrDisabled                  = true,
		.manualOverrideDMRId          = 0x00,

		.Scan =
		{
				.timer                  = 0,
				.timerReload			= 30,
				.direction              = 1,
				.availableChannelsCount = 0,
				.nuisanceDeleteIndex    = 0,
				.nuisanceDelete         = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
				.state                  = SCAN_SCANNING,
				.active                 = false,
				.toneActive             = false,
				.refreshOnEveryStep     = false,
				.lastIteration          = false,
				.scanType               = SCAN_TYPE_NORMAL_STEP,
				.stepTimeMilliseconds   = 0
		},

		.QuickMenu =
		{
				.tmpDmrDestinationFilterLevel = 0,
				.tmpDmrCcTsFilterLevel        = 0,
				.tmpAnalogFilterLevel         = 0,
				.tmpTxRxLockMode              = 0,
				.tmpToneScanCSS               = CSS_TYPE_NONE,
				.tmpVFONumber                 = 0
		},

		.FreqEnter =
		{
				.index  = 0,
				.digits = { '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-' }
		},

		.PrivateCall =
		{
				.lastID = 0,
				.state  = PRIVATE_CALL_NOT_IN_CALL
		},

		.VoicePrompts =
		{
				.inhibitInitial =
#if defined(PLATFORM_GD77S)
									true
#else
									false // Used to indicate whether the voice prompts should be reloaded with the channel name or VFO freq
#endif
		},

		.MessageBox =
		{
				.type              = MESSAGEBOX_TYPE_UNDEF,
				.decoration        = MESSAGEBOX_DECORATION_NONE,
				.buttons           = MESSAGEBOX_BUTTONS_NONE,
				.pinLength         = 4,
				.message           = { 0 },
				.keyPressed        = 0,
				.validatorCallback = NULL
		},

		.DTMFContactList =
		{
				.nextPeriod = 0U,
				.isKeying   = false,
				.buffer     = { 0xff },
				.poLen      = 0U,
				.poPtr      = 0U,
				.durations  = { 0, 0, 0, 0, 0 },
				.inTone     = false
		}

};


settingsStruct_t originalNonVolatileSettings =
{
		.magicNumber = 0xDEADBEEF
};

struct_codeplugZone_t currentZone;
__attribute__((section(".data.$RAM2"))) struct_codeplugRxGroup_t currentRxGroupData;
int lastLoadedRxGroup = -1;// to show data for which RxGroupIndex has been loaded into    currentRxGroupData
struct_codeplugContact_t currentContactData;

bool PTTToggledDown = false; // PTT toggle feature

