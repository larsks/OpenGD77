/*
 * Copyright (C) 2019      Kai Ludwig, DG4KLU
 * Copyright (C) 2019-2021 Roger Clark, VK3KYY / G4KYF
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

#include "interfaces/wdog.h"
#include "interfaces/pit.h"

static void watchdogTick(void);
static void fw_watchdog_task(void *data);

TaskHandle_t fwwatchdogTaskHandle;

volatile bool alive_maintask;
volatile bool alive_beeptask;
volatile bool alive_hrc6000task;

float averageBatteryVoltage;
float previousAverageBatteryVoltage;
int batteryVoltage = 0;
bool headerRowIsDirty = false;

static WDOG_Type *wdog_base = WDOG;
static int watchdog_refresh_tick = 0;
static bool reboot = false;
static int batteryVoltageTick = 0;
static int batteryVoltageCallbackTick = 0;
static const int BATTERY_VOLTAGE_TICK_RELOAD = 100;
static const int BATTERY_VOLTAGE_CALLBACK_TICK_RELOAD = 20;
static const int AVERAGE_BATTERY_VOLTAGE_SAMPLE_WINDOW = 60.0f;// 120 secs = Sample window * BATTERY_VOLTAGE_TICK_RELOAD in milliseconds
static const int BATTERY_VOLTAGE_STABILISATION_TIME = 2000;// time in PIT ticks for the battery voltage from the ADC to stabilise

batteryHistoryCallback_t batteryCallbackFunction = NULL;

static void fw_watchdog_task(void *data)
{
	while (1U)
	{
		if (timer_watchdogtask == 0)
		{
			timer_watchdogtask = 10;
			watchdogTick();
		}
		vTaskDelay(0);
	}
}

static void watchdogTick(void)
{
	watchdog_refresh_tick++;
	if (watchdog_refresh_tick == 200)
	{
		if (alive_maintask && alive_beeptask && alive_hrc6000task && !reboot)
		{
			WDOG_Refresh(wdog_base);
		}
		alive_maintask = false;
		alive_beeptask = false;
		alive_hrc6000task = false;
		watchdog_refresh_tick = 0;
	}

	batteryVoltageTick++;
	if (batteryVoltageTick >= BATTERY_VOLTAGE_TICK_RELOAD)
	{
		batteryVoltage = adcGetBatteryVoltage();

		if (PITCounter < BATTERY_VOLTAGE_STABILISATION_TIME)
		{
			averageBatteryVoltage = batteryVoltage;
		}
		else
		{
			averageBatteryVoltage = (averageBatteryVoltage * (AVERAGE_BATTERY_VOLTAGE_SAMPLE_WINDOW - 1) + batteryVoltage) / AVERAGE_BATTERY_VOLTAGE_SAMPLE_WINDOW;
		}

		if (previousAverageBatteryVoltage != averageBatteryVoltage)
		{
			previousAverageBatteryVoltage = averageBatteryVoltage;
			headerRowIsDirty = true;
		}

		batteryVoltageTick = 0;
		batteryVoltageCallbackTick++;
		if (batteryCallbackFunction && (batteryVoltageCallbackTick >= BATTERY_VOLTAGE_CALLBACK_TICK_RELOAD))
		{
			batteryCallbackFunction(averageBatteryVoltage);
			batteryVoltageCallbackTick = 0;
		}
	}
	adcTriggerConversion(NO_ADC_CHANNEL_OVERRIDE);// need the ADC value next time though, so request conversion now, so that its ready by the time we need it
}

void watchdogReboot(void)
{
	reboot = true;
}

void watchdogInit(batteryHistoryCallback_t cb)
{
	wdog_config_t config;

	WDOG_GetDefaultConfig(&config);
	config.timeoutValue = 0x3ffU;
	WDOG_Init(wdog_base, &config);
	for (uint32_t i = 0; i < 256; i++)
	{
		wdog_base->RSTCNT;
	}

	batteryCallbackFunction = cb;

	watchdog_refresh_tick = 0;

	alive_maintask = false;
	alive_beeptask = false;
	alive_hrc6000task = false;

	batteryVoltage = adcGetBatteryVoltage();
	averageBatteryVoltage = batteryVoltage;
	previousAverageBatteryVoltage = 0;// need to set this to zero to force an immediate display update.
	batteryVoltageTick = 0;
	batteryVoltageCallbackTick = 0;

	if (batteryCallbackFunction)
	{
		batteryCallbackFunction(batteryVoltage);
	}

	xTaskCreate(fw_watchdog_task,                        /* pointer to the task */
			"WDT",                      /* task name for kernel awareness debugging */
			1000L / sizeof(portSTACK_TYPE),      /* task stack size */
			NULL,                      			 /* optional task startup argument */
			5U,                                  /* initial priority */
			fwwatchdogTaskHandle					 /* optional task handle to create */
	);
}

void watchdogDeinit(void)
{
	WDOG_Deinit(wdog_base);
}
