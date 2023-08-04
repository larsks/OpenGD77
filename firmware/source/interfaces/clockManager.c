/*
 * Copyright (C) 2020-2021 Roger Clark, VK3KYY / G4KYF
 *
 * Using some code from NXP examples
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

#include <interfaces/clockManager.h>
#include "fsl_rcm.h"
#include "fsl_pmc.h"

#include "clock_config.h"
#include "interfaces/pit.h"
#include "fsl_tickless_generic.h"
#include "interfaces/hr-c6000_spi.h"
#include "interfaces/i2c.h"

#if defined(USING_EXTERNAL_DEBUGGER)
#include "SeggerRTT/RTT/SEGGER_RTT.h"
#endif

static void APP_SetClockHsrun(void);
static void APP_SetClockRunFromHsrun(void);

status_t beforeChangeCallback(notifier_notification_block_t *notify, void *dataPtr);

static power_user_config_t runConfig   = {
		kAPP_PowerModeRun,
        true
    };
static power_user_config_t hsrunConfig = {
		kAPP_PowerModeHsrun,
        true
    };

static notifier_user_config_t *powerConfigs[] = {
    &runConfig,
	&hsrunConfig,
};

static notifier_handle_t powerModeHandle;
static user_callback_data_t callbackData0;
static notifier_callback_config_t callbackCfg0 = { beforeChangeCallback, kNOTIFIER_CallbackBeforeAfter, (void *)&callbackData0 };
static notifier_callback_config_t callbacks[1];

app_power_mode_t clockManagerCurrentRunMode;

static void updateRTOSAndPitTimings(void)
{
    SystemCoreClock = CLOCK_GetFreq(kCLOCK_CoreSysClk);
	vPortSetupTimerInterrupt();
	PIT_DisableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);
	PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, USEC_TO_COUNT(100U, CLOCK_GetFreq(kCLOCK_BusClk)));
	PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);

    SPI0Setup();
    SPI1Setup();
    I2C0Setup();
}

void APP_SetClockHsrun(void)
{
	CLOCK_SetPbeMode(kMCG_PllClkSelPll0, &mcgConfig_BOARD_BootClockHSRUN.pll0Config);
	CLOCK_SetPeeMode();
	CLOCK_SetSimConfig(&simConfig_BOARD_BootClockHSRUN);

	updateRTOSAndPitTimings();
}

void APP_SetClockRunFromHsrun(void)
{
	CLOCK_SetPbeMode(kMCG_PllClkSelPll0, &mcgConfig_BOARD_BootClockRUN.pll0Config);
	CLOCK_SetPeeMode();
	CLOCK_SetSimConfig(&simConfig_BOARD_BootClockRUN);
	updateRTOSAndPitTimings();
}


status_t beforeChangeCallback(notifier_notification_block_t *notify, void *dataPtr)
{
	user_callback_data_t *userData     = (user_callback_data_t *)dataPtr;
	status_t ret                       = kStatus_Fail;
	app_power_mode_t targetMode        = ((power_user_config_t *)notify->targetConfig)->mode;
	smc_power_state_t originPowerState = userData->originPowerState;

	switch (notify->notifyType)
	{
		case kNOTIFIER_NotifyBefore:
			userData->beforeNotificationCounter++;
			ret = kStatus_Success;
			break;
		case kNOTIFIER_NotifyRecover:
			break;
		case kNOTIFIER_CallbackAfter:
			userData->afterNotificationCounter++;

			if ((kAPP_PowerModeRun != targetMode) &&  (kAPP_PowerModeVlpr != targetMode))
			{
				if (kSMC_PowerStateRun == originPowerState)
				{
					/* Wait for PLL lock. */
					while (!(kMCG_Pll0LockFlag & CLOCK_GetStatusFlags()))
					{
					}
					CLOCK_SetPeeMode();
				}
			}
			ret = kStatus_Success;
			break;
		default:
			break;
	}
	return ret;
}


status_t APP_PowerModeSwitch(notifier_user_config_t *targetConfig, void *userData)
{
	smc_power_state_t currentPowerMode;
	app_power_mode_t targetPowerMode;
	power_user_config_t *targetPowerModeConfig;

	targetPowerModeConfig = (power_user_config_t *)targetConfig;
	currentPowerMode      = SMC_GetPowerModeState(SMC);
	targetPowerMode       = targetPowerModeConfig->mode;

	switch (targetPowerMode)
	{
		case kAPP_PowerModeRun:
			/* If enter RUN from HSRUN, fisrt change clock. */
			if (kSMC_PowerStateHsrun == currentPowerMode)
			{
				APP_SetClockRunFromHsrun();
			}

			/* Power mode change. */
			SMC_SetPowerModeRun(SMC);
			while (kSMC_PowerStateRun != SMC_GetPowerModeState(SMC))
			{
			}

			clockManagerCurrentRunMode = kAPP_PowerModeRun;
			break;

		case kAPP_PowerModeHsrun:
			SMC_SetPowerModeHsrun(SMC);
			while (kSMC_PowerStateHsrun != SMC_GetPowerModeState(SMC))
			{
			}

			APP_SetClockHsrun(); /* Change clock setting after power mode change. */
			clockManagerCurrentRunMode = kAPP_PowerModeHsrun;
			break;
		default:
			break;
	}

	return kStatus_Success;
}

void clockManagerSetRunMode(uint8_t targetConfigIndex)
{
	taskENTER_CRITICAL();
	callbackData0.originPowerState = SMC_GetPowerModeState(SMC);
	NOTIFIER_SwitchConfig(&powerModeHandle, targetConfigIndex - kAPP_PowerModeMin - 1, kNOTIFIER_PolicyAgreement);
	taskEXIT_CRITICAL();
}

void clockManagerInit(void)
{
	callbacks[0] = callbackCfg0;

	clockManagerCurrentRunMode = kAPP_PowerModeMin;
	memset(&callbackData0, 0, sizeof(user_callback_data_t));
	NOTIFIER_CreateHandle(&powerModeHandle, powerConfigs, ARRAY_SIZE(powerConfigs), callbacks, 1U, APP_PowerModeSwitch, NULL);

	SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll);
}
