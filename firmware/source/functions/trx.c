/*
 * Copyright (C) 2019      Kai Ludwig, DG4KLU
 * Copyright (C) 2019-2021 Roger Clark, VK3KYY / G4KYF
 *                         Colin, G4EML
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

#include "hardware/AT1846S.h"
#include "functions/calibration.h"
#include "hardware/HR-C6000.h"
#include "functions/settings.h"
#include "functions/trx.h"
#include "user_interface/menuSystem.h"
#include "user_interface/uiUtilities.h"
#include <FreeRTOS.h>

//#define NEW_PA_CONTROL 1


#if USE_DATASHEET_RANGES
const frequencyHardwareBand_t RADIO_HARDWARE_FREQUENCY_BANDS[RADIO_BANDS_TOTAL_NUM] =  {
													{
														.minFreq=13400000,
														.maxFreq=17400000
													},// VHF
													{
														.minFreq=20000000,
														.maxFreq=26000000
													},// 220Mhz
													{
														.minFreq=40000000,
														.maxFreq=52000000
													}// UHF
};
#else
const frequencyHardwareBand_t RADIO_HARDWARE_FREQUENCY_BANDS[RADIO_BANDS_TOTAL_NUM] =  {
													{
														.calTableMinFreq = 13400000,
														.minFreq=12700000,
														.maxFreq=17800000
													},// VHF
													{
														.calTableMinFreq = 13400000,
														.minFreq=19000000,
														.maxFreq=28200000
													},// 220Mhz
													{
														.calTableMinFreq = 40000000,
														.minFreq=38000000,
														.maxFreq=56400000
													}// UHF
};
#endif

static const int TRX_SQUELCH_MAX = 70;
const uint8_t TRX_NUM_CTCSS = 50U;
const uint16_t TRX_CTCSSTones[] = {
		670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
		948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
		1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
		1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
		2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541
};

const uint16_t TRX_DCS_TONE = 13440;  // 134.4Hz is the data rate of the DCS bitstream (and a reason not to use that tone for CTCSS)
const uint8_t TRX_NUM_DCS = 83U;

const uint16_t TRX_DCSCodes[] = {
		0x023, 0x025, 0x026, 0x031, 0x032, 0x043, 0x047, 0x051, 0x054, 0x065, 0x071, 0x072, 0x073, 0x074,
		0x114, 0x115, 0x116, 0x125, 0x131, 0x132, 0x134, 0x143, 0x152, 0x155, 0x156, 0x162, 0x165, 0x172, 0x174,
		0x205, 0x223, 0x226, 0x243, 0x244, 0x245, 0x251, 0x261, 0x263, 0x265, 0x271,
		0x306, 0x311, 0x315, 0x331, 0x343, 0x345, 0x351, 0x364, 0x365, 0x371,
		0x411, 0x412, 0x413, 0x423, 0x431, 0x432, 0x445, 0x464, 0x465, 0x466,
		0x503, 0x506, 0x516, 0x532, 0x546, 0x565,
		0x606, 0x612, 0x624, 0x627, 0x631, 0x632, 0x654, 0x662, 0x664,
		0x703, 0x712, 0x723, 0x731, 0x732, 0x734, 0x743, 0x754
};

#define DCS_PACKED_DATA_NUM          83
#define DCS_PACKED_DATA_ENTRY_SIZE   3

// 83 * 3 bytes(20 bits packed) of DCS code (9bits) + Golay(11bits).
static const uint8_t DCS_PACKED_DATA[(DCS_PACKED_DATA_NUM * DCS_PACKED_DATA_ENTRY_SIZE)] = {
		0x63,  0x87,  0x09,  0xB7,  0x86,  0x0A,  0x5D,  0x06,  0x0B,  0x1F,  0x85,  0x0C,  0xF5,  0x05,  0x0D,  0xB6,
		0x85,  0x11,  0xFD,  0x80,  0x13,  0xCA,  0x87,  0x14,  0xF4,  0x06,  0x16,  0xD1,  0x85,  0x1A,  0x79,  0x86,
		0x1C,  0x93,  0x06,  0x1D,  0xE6,  0x82,  0x1D,  0x47,  0x07,  0x1E,  0x5E,  0x03,  0x26,  0x2B,  0x87,  0x26,
		0xC1,  0x07,  0x27,  0x7B,  0x80,  0x2A,  0xD3,  0x83,  0x2C,  0x39,  0x03,  0x2D,  0xED,  0x02,  0x2E,  0x7A,
		0x83,  0x31,  0xEC,  0x01,  0x35,  0x4D,  0x84,  0x36,  0xA7,  0x04,  0x37,  0xBC,  0x06,  0x39,  0x1D,  0x83,
		0x3A,  0x5F,  0x00,  0x3D,  0x8B,  0x01,  0x3E,  0xE9,  0x86,  0x42,  0x8E,  0x86,  0x49,  0xB0,  0x07,  0x4B,
		0x5B,  0x84,  0x51,  0xFA,  0x01,  0x52,  0x8F,  0x85,  0x52,  0x27,  0x86,  0x54,  0x77,  0x81,  0x58,  0xE8,
		0x85,  0x59,  0x3C,  0x84,  0x5A,  0x94,  0x87,  0x5C,  0xCF,  0x00,  0x63,  0x8D,  0x83,  0x64,  0xC6,  0x86,
		0x66,  0x3E,  0x82,  0x6C,  0x97,  0x82,  0x71,  0xA9,  0x03,  0x73,  0xEB,  0x80,  0x74,  0x85,  0x06,  0x7A,
		0xF0,  0x82,  0x7A,  0x58,  0x81,  0x7C,  0x76,  0x87,  0x84,  0x9C,  0x07,  0x85,  0xE9,  0x83,  0x85,  0xB9,
		0x84,  0x89,  0xC5,  0x86,  0x8C,  0x2F,  0x06,  0x8D,  0xB8,  0x87,  0x92,  0x7E,  0x02,  0x9A,  0x0B,  0x86,
		0x9A,  0xE1,  0x06,  0x9B,  0xC6,  0x83,  0xA1,  0xF8,  0x02,  0xA3,  0x1B,  0x04,  0xA7,  0xE3,  0x00,  0xAD,
		0x9E,  0x01,  0xB3,  0xC7,  0x80,  0xBA,  0xD9,  0x05,  0xC3,  0x71,  0x06,  0xC5,  0xF5,  0x00,  0xCA,  0x1F,
		0x80,  0xCB,  0x28,  0x87,  0xCC,  0xC2,  0x07,  0xCD,  0xC3,  0x04,  0xD6,  0x47,  0x02,  0xD9,  0x93,  0x03,
		0xDA,  0x2B,  0x82,  0xE1,  0xBD,  0x00,  0xE5,  0x98,  0x83,  0xE9,  0xE4,  0x81,  0xEC,  0x0E,  0x01,  0xED,
		0xDA,  0x00,  0xEE,  0x4D,  0x81,  0xF1,  0x0F,  0x02,  0xF6
};

frequencyBand_t USER_FREQUENCY_BANDS[RADIO_BANDS_TOTAL_NUM] =  {
													{
														.minFreq=14400000,
														.maxFreq=14800000
													},// VHF
													{
														.minFreq=22200000,
														.maxFreq=22500000
													},// 220Mhz
													{
														.minFreq=42000000,
														.maxFreq=45000000
													}// UHF
};

const frequencyBand_t DEFAULT_USER_FREQUENCY_BANDS[RADIO_BANDS_TOTAL_NUM] =  {
													{
														.minFreq=14400000,
														.maxFreq=14800000
													},// VHF
													{
														.minFreq=22200000,
														.maxFreq=22500000
													},// 220Mhz
													{
														.minFreq=42000000,
														.maxFreq=45000000
													}// UHF
};

enum CAL_DEV_TONE_INDEXES { CAL_DEV_DTMF = 0, CAL_DEV_TONE = 1, CAL_DEV_CTCSS_WIDE = 2, CAL_DEV_CTCSS_NARROW = 3, CAL_DEV_DCS_WIDE = 4, CAL_DEV_DCS_NARROW = 5};

const uint32_t RSSI_NOISE_SAMPLE_PERIOD_PIT = 25 * 10;// 25 milliseconds

static uint16_t txDACDrivePower;
static int txPowerLevel = -1;
static bool analogSignalReceived = false;
static bool analogTriggeredAudio = false;
static bool digitalSignalReceived = false;
static uint32_t trxNextRssiNoiseSamplePIT = 0;

static uint8_t trxCssMeasureCount = 0;

static int currentMode = RADIO_MODE_NONE;
static bool currentBandWidthIs25kHz = BANDWIDTH_12P5KHZ;
static int currentRxFrequency = -1;
static int currentTxFrequency = -1;
static int currentCC = 1;

#define CTCSS_HOLD_DELAY      6
#define SQUELCH_CLOSE_DELAY   1

static bool rxCSSactive = false;
//static uint8_t rxCSSTriggerCount = 0;
static int trxCurrentDMRTimeSlot;

// AT-1846 native values for Rx
static uint8_t rx_fl_l;
static uint8_t rx_fl_h;
static uint8_t rx_fh_l;
static uint8_t rx_fh_h;

// AT-1846 native values for Tx
static uint8_t tx_fl_l;
static uint8_t tx_fl_h;
static uint8_t tx_fh_l;
static uint8_t tx_fh_h;

volatile uint8_t trxRxSignal;
volatile uint8_t trxRxNoise;
volatile uint8_t trxTxVox;
volatile uint8_t trxTxMic;

static uint8_t trxSaveVoiceGainTx = 0xff;
static uint16_t trxSaveDeviation = 0xff;
static uint8_t voice_gain_tx = 0x31; // default voice_gain_tx fro calibration, needs to be declared here in case calibration:OFF

static int lastSetTxFrequency = -1;
static int lastSetTxPowerLevel = -1;

volatile bool trxTransmissionEnabled = false;
volatile bool trxIsTransmitting = false;
volatile bool txPAEnabled = false;

uint32_t trxTalkGroupOrPcId = 9;// Set to local TG just in case there is some problem with it not being loaded
uint32_t trxDMRID = 0;// Set ID to 0. Not sure if its valid. This value needs to be loaded from the codeplug.
int txstopdelay = 0;

int trxCurrentBand[2] = { RADIO_BAND_VHF, RADIO_BAND_VHF };// Rx and Tx band.
int trxDMRModeRx = DMR_MODE_DMO;// simplex
int trxDMRModeTx = DMR_MODE_DMO;// simplex


// DTMF Order: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, A, B, C, D, *, #
const int trxDTMFfreq1[] = { 1336, 1209, 1336, 1477, 1209, 1336, 1477, 1209, 1336, 1477, 1633, 1633, 1633, 1633, 1209, 1477 };
const int trxDTMFfreq2[] = {  941,  697,  697,  697,  770,  770,  770,  852,  852,  852,  697,  770,  852,  941,  941,  941 };

calibrationPowerValues_t trxPowerSettings;

static bool powerUpDownState = true;

static uint8_t padrv_ibit;// Tx Drive of AT1846S

static uint8_t trxAnalogFilterLevel = ANALOG_FILTER_CSS;


static uint16_t convertCSSNative2BinaryCodedOctal(uint16_t nativeCSS);
static uint32_t dcsGetBitPatternFromCode(uint16_t dcs);



//
// =================================================================
//

uint8_t trxGetAnalogFilterLevel()
{
	return trxAnalogFilterLevel;
}

void trxSetAnalogFilterLevel(uint8_t newFilterLevel)
{
	trxAnalogFilterLevel = newFilterLevel;
}


int trxGetMode(void)
{
	return currentMode;
}

int trxGetBandwidthIs25kHz(void)
{
	return currentBandWidthIs25kHz;
}

void trxSetModeAndBandwidth(int mode, bool bandwidthIs25kHz)
{
	digitalSignalReceived = false;
	analogSignalReceived = false;
	trxNextRssiNoiseSamplePIT = PITCounter + RSSI_NOISE_SAMPLE_PERIOD_PIT;
	trxCssMeasureCount = 0;
	//rxCSSTriggerCount = 0;
	trxRxSignal = 0;
	trxRxNoise = 0;

	// DMR (digital) is disabled, hence force it
	// to RADIO_MODE_ANALOG (has we could currently be in RADIO_MODE_NONE (CPS))
	if (uiDataGlobal.dmrDisabled && (mode == RADIO_MODE_DIGITAL))
	{
		mode = RADIO_MODE_ANALOG;
	}

	if ((mode != currentMode) || (bandwidthIs25kHz != currentBandWidthIs25kHz))
	{
		currentMode = mode;

		taskENTER_CRITICAL();
		switch(mode)
		{
			case RADIO_MODE_NONE:// not truly off
				GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 0); // connect AT1846S audio to HR_C6000
				soundTerminateSound();
				terminate_digital();
				AT1846SetMode(); // Set to digital (as fallback)
				trxUpdateC6000Calibration();
				trxUpdateAT1846SCalibration();
				break;
			case RADIO_MODE_ANALOG:
				currentBandWidthIs25kHz = bandwidthIs25kHz;
				GPIO_PinWrite(GPIO_TX_audio_mux, Pin_TX_audio_mux, 0); // Connect mic to mic input of AT-1846
				// Enabling the following line cause cracking sound on the DM-1801, at the end of the boot melody.
				// It's managed anyway by the Squelch code.
				//GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 1); // connect AT1846S audio to speaker
				terminate_digital();
				AT1846SetMode();
				trxUpdateC6000Calibration();
				trxUpdateAT1846SCalibration();
				break;
			case RADIO_MODE_DIGITAL:
				currentBandWidthIs25kHz = BANDWIDTH_12P5KHZ;// DMR bandwidth is 12.5kHz
				AT1846SetMode();// Also sets the bandwidth to 12.5kHz which is the standard for DMR
				trxUpdateC6000Calibration();
				trxUpdateAT1846SCalibration();
				GPIO_PinWrite(GPIO_TX_audio_mux, Pin_TX_audio_mux, 1); // Connect mic to MIC_P input of HR-C6000
				GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 0); // connect AT1846S audio to HR_C6000
				soundInit();
				init_digital();
				break;
		}
		taskEXIT_CRITICAL();
	}
	else
	{
		if (mode == RADIO_MODE_ANALOG)
		{
			disableAudioAmp(AUDIO_AMP_MODE_RF);
		}
		else
		{
			reset_timeslot_detection();
			// We need to reset the slot state because some part of the UI
			// are getting stuck, like the green LED and QSO info, while navigating the UI.
			if (slot_state != DMR_STATE_IDLE)
			{
				slot_state = DMR_STATE_RX_END;
			}
		}
	}
}

int trxGetNextOrPrevBandFromFrequency(int frequency, bool nextBand)
{
	if (nextBand)
	{
		if (frequency > RADIO_HARDWARE_FREQUENCY_BANDS[RADIO_BANDS_TOTAL_NUM - 1].maxFreq)
		{
			return 0; // First band
		}

		for(int band = 0; band < RADIO_BANDS_TOTAL_NUM - 1; band++)
		{
			if (frequency > RADIO_HARDWARE_FREQUENCY_BANDS[band].maxFreq && frequency < RADIO_HARDWARE_FREQUENCY_BANDS[band + 1].minFreq)
			{
				return (band + 1); // Next band
			}
		}
	}
	else
	{
		if (frequency < RADIO_HARDWARE_FREQUENCY_BANDS[0].minFreq)
		{
			return (RADIO_BANDS_TOTAL_NUM - 1); // Last band
		}

		for(int band = 1; band < RADIO_BANDS_TOTAL_NUM; band++)
		{
			if (frequency < RADIO_HARDWARE_FREQUENCY_BANDS[band].minFreq && frequency > RADIO_HARDWARE_FREQUENCY_BANDS[band - 1].maxFreq)
			{
				return (band - 1); // Prev band
			}
		}
	}

	return -1;
}

int trxGetBandFromFrequency(int frequency)
{
	for(int i = 0; i < RADIO_BANDS_TOTAL_NUM; i++)
	{
		if ((frequency >= RADIO_HARDWARE_FREQUENCY_BANDS[i].minFreq) && (frequency <= RADIO_HARDWARE_FREQUENCY_BANDS[i].maxFreq))
		{
			return i;
		}
	}

	return -1;
}

bool trxCheckFrequencyInAmateurBand(int frequency)
{
	if (nonVolatileSettings.txFreqLimited == BAND_LIMITS_FROM_CPS)
	{
		return ((frequency >= USER_FREQUENCY_BANDS[RADIO_BAND_VHF].minFreq) && (frequency <= USER_FREQUENCY_BANDS[RADIO_BAND_VHF].maxFreq)) ||
			((frequency >= USER_FREQUENCY_BANDS[RADIO_BAND_UHF].minFreq) && (frequency <= USER_FREQUENCY_BANDS[RADIO_BAND_UHF].maxFreq));
	}
	else if (nonVolatileSettings.txFreqLimited == BAND_LIMITS_ON_LEGACY_DEFAULT)
	{
		return ((frequency >= DEFAULT_USER_FREQUENCY_BANDS[RADIO_BAND_VHF].minFreq) && (frequency <= DEFAULT_USER_FREQUENCY_BANDS[RADIO_BAND_VHF].maxFreq)) ||
			((frequency >= DEFAULT_USER_FREQUENCY_BANDS[RADIO_BAND_220MHz].minFreq) && (frequency <= DEFAULT_USER_FREQUENCY_BANDS[RADIO_BAND_220MHz].maxFreq)) ||
			((frequency >= DEFAULT_USER_FREQUENCY_BANDS[RADIO_BAND_UHF].minFreq) && (frequency <= DEFAULT_USER_FREQUENCY_BANDS[RADIO_BAND_UHF].maxFreq));
	}
	return true;// Setting must be BAND_LIMITS_NONE
}

void trxReadVoxAndMicStrength(void)
{
	AT1846ReadVoxAndMicStrength();
}

void trxReadRSSIAndNoise(void)
{
	AT1846ReadRSSIAndNoise();
}

bool trxCarrierDetected(void)
{
	uint8_t squelch = 0;

	trxReadRSSIAndNoise();

	switch(currentMode)
	{
		case RADIO_MODE_NONE:
			break;

		case RADIO_MODE_ANALOG:
			if (currentChannelData->sql != 0)
			{
				squelch = TRX_SQUELCH_MAX - (((currentChannelData->sql - 1) * 11) >> 2);
			}
			else
			{
				squelch = TRX_SQUELCH_MAX - (((nonVolatileSettings.squelchDefaults[trxCurrentBand[TRX_RX_FREQ_BAND]]) * 11) >> 2);
			}
			break;

		case RADIO_MODE_DIGITAL:
			squelch = TRX_SQUELCH_MAX - (((nonVolatileSettings.squelchDefaults[trxCurrentBand[TRX_RX_FREQ_BAND]]) * 11) >> 2);
			break;
	}

	return (trxRxNoise < squelch);
}

bool trxCheckDigitalSquelch(void)
{
	if (PITCounter >= trxNextRssiNoiseSamplePIT)
	{
		uint8_t squelch;

		trxReadRSSIAndNoise();

		squelch = TRX_SQUELCH_MAX - (((nonVolatileSettings.squelchDefaults[trxCurrentBand[TRX_RX_FREQ_BAND]]) * 11) >> 2);

		if (trxRxNoise < squelch)
		{
			if(!digitalSignalReceived)
			{
				digitalSignalReceived = true;
				LEDs_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 1);
			}
		}
		else
		{
			if (digitalSignalReceived)
			{
				digitalSignalReceived = false;
				LEDs_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
			}
		}

		trxNextRssiNoiseSamplePIT = PITCounter + RSSI_NOISE_SAMPLE_PERIOD_PIT;
	}
	return digitalSignalReceived;
}

void trxTerminateCheckAnalogSquelch(void)
{
	disableAudioAmp(AUDIO_AMP_MODE_RF);
	analogSignalReceived = false;
	analogTriggeredAudio = false;
	trxCssMeasureCount = 0;
	//rxCSSTriggerCount = 0;
}

bool trxCheckAnalogSquelch(void)
{
	if (uiVFOModeSweepScanning(false))
	{
		return false;
	}

	if (PITCounter >= trxNextRssiNoiseSamplePIT)
	{
		uint8_t squelch;

		trxReadRSSIAndNoise();

		// check for variable squelch control
		if (currentChannelData->sql != 0)
		{
			squelch = TRX_SQUELCH_MAX - (((currentChannelData->sql - 1) * 11) >> 2);
		}
		else
		{
			squelch = TRX_SQUELCH_MAX - (((nonVolatileSettings.squelchDefaults[trxCurrentBand[TRX_RX_FREQ_BAND]]) * 11) >> 2);
		}

		if (trxRxNoise < squelch)
		{
			if(analogSignalReceived == false)
			{
				analogSignalReceived = true;
				LEDs_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 1);
				analogTriggeredAudio = true;
				//rxCSSTriggerCount = 0;
				trxCssMeasureCount = 0;
			}
		}
		else
		{
			if(analogSignalReceived
#if !defined (PLATFORM_GD77S) // The GD-77S drives the LEDs for the heart beat
					|| LEDs_PinRead(GPIO_LEDgreen, Pin_LEDgreen)
#endif
			)
			{
				analogSignalReceived = false;
				LEDs_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
				analogTriggeredAudio = false;
				//rxCSSTriggerCount = 0;
				trxCssMeasureCount = 0;
			}
		}

		bool cssFlag = (rxCSSactive ? trxCheckCSSFlag(currentChannelData->rxTone) : false);

		if (analogSignalReceived)
		{
			if (((getAudioAmpStatus() & AUDIO_AMP_MODE_RF) == 0) && ((rxCSSactive == false) || cssFlag))
			{
				/*
				// Delay the audio amp enabling to avoid false triggering.
				if (rxCSSactive && (rxCSSTriggerCount < 1))
				{
					rxCSSTriggerCount++;
					trxNextRssiNoiseSamplePIT = PITCounter + RSSI_NOISE_SAMPLE_PERIOD_PIT;
					return;
				}*/

				if (analogTriggeredAudio) // Execute that block of code just once after valid signal is received.
				{
					taskENTER_CRITICAL();
					if (!voicePromptsIsPlaying())
					{
						GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 1); // Set the audio path to AT1846 -> audio amp.
						enableAudioAmp(AUDIO_AMP_MODE_RF);
						displayLightTrigger(false);
						analogTriggeredAudio = false;
						trxCssMeasureCount = 0;
					}
					taskEXIT_CRITICAL();
				}
			}
			else if (getAudioAmpStatus() & AUDIO_AMP_MODE_RF)
			{
				if (rxCSSactive && (cssFlag == false)) // CSS disappeared.
				{
					trxCssMeasureCount++;
					// If using CTCSS or DCS and signal isn't lost, allow some loss of tone / code.
					// Note:
					//    It's not unusual to have the CSS detection failing (CTCSS, depending of the sub-tone) if
					//    the signal is over-modulated/deviated, so waiting for 150ms is fine, and almost needed.
					//    Waiting for shorter time will just constantly disable and enable the audio Amp.
					if (trxCssMeasureCount >= CTCSS_HOLD_DELAY)
					{
						disableAudioAmp(AUDIO_AMP_MODE_RF);
						analogSignalReceived = false;
						analogTriggeredAudio = false;
						trxCssMeasureCount = 0;
						//rxCSSTriggerCount = 0;
					}
				}
				else
				{
					trxCssMeasureCount = 0;
				}
			}
		}
		else
		{
			if (getAudioAmpStatus() & AUDIO_AMP_MODE_RF)
			{
				trxCssMeasureCount++;
				// If using CTCSS or DCS and signal isn't lost, allow some loss of tone / code
				//
				// NOTE: Currently it is NOT waiting at all.
				//
				if ((rxCSSactive == false) || (trxCssMeasureCount >= SQUELCH_CLOSE_DELAY))
				{
					disableAudioAmp(AUDIO_AMP_MODE_RF);
					trxCssMeasureCount = 0;
					//rxCSSTriggerCount = 0;
				}
			}
		}

		trxNextRssiNoiseSamplePIT = PITCounter + RSSI_NOISE_SAMPLE_PERIOD_PIT;
	}

	return analogSignalReceived;
}

void trxSetFrequency(int fRx,int fTx, int dmrMode)
{
	if (currentChannelData->libreDMR_Power != 0x00)
	{
		txPowerLevel = currentChannelData->libreDMR_Power - 1;
	}
	else
	{
		txPowerLevel = nonVolatileSettings.txPowerLevel;
	}

	if ((currentRxFrequency != fRx) || (currentTxFrequency != fTx))
	{
		taskENTER_CRITICAL();
		trxCurrentBand[TRX_RX_FREQ_BAND] = trxGetBandFromFrequency(fRx);

		currentRxFrequency = fRx;
		currentTxFrequency = fTx;

		if (dmrMode == DMR_MODE_AUTO)
		{
			// Most DMR radios determine whether to use Active or Passive DMR depending on whether the Tx and Rx freq are the same
			// This prevents split simplex operation, but since no other radio appears to support split freq simplex
			// Its easier to do things the same way as othe radios, and revisit this again in the future if split freq simplex is required.
			if (currentRxFrequency == currentTxFrequency)
			{
				trxDMRModeTx = DMR_MODE_DMO;
				trxDMRModeRx = DMR_MODE_DMO;
			}
			else
			{
				trxDMRModeTx = DMR_MODE_RMO;
				trxDMRModeRx = DMR_MODE_RMO;
			}
		}
		else
		{
			trxDMRModeTx = dmrMode;
			trxDMRModeRx = dmrMode;
		}

		uint32_t f = currentRxFrequency * 0.16f;
		rx_fl_l = (f & 0x000000ff) >> 0;
		rx_fl_h = (f & 0x0000ff00) >> 8;
		rx_fh_l = (f & 0x00ff0000) >> 16;
		rx_fh_h = (f & 0xff000000) >> 24;

		f = currentTxFrequency * 0.16f;
		tx_fl_l = (f & 0x000000ff) >> 0;
		tx_fl_h = (f & 0x0000ff00) >> 8;
		tx_fh_l = (f & 0x00ff0000) >> 16;
		tx_fh_h = (f & 0xff000000) >> 24;

		if (currentMode == RADIO_MODE_DIGITAL)
		{
			terminate_digital();
		}

		if (currentBandWidthIs25kHz)
		{
			// 25 kHz settings
			AT1846SWriteReg2byte( 0x30, 0x70, 0x06); // RX off
		}
		else
		{
			// 12.5 kHz settings
			AT1846SWriteReg2byte( 0x30, 0x40, 0x06); // RX off
		}
		AT1846SWriteReg2byte( 0x05, 0x87, 0x63); // select 'normal' frequency mode

		AT1846SWriteReg2byte( 0x29, rx_fh_h, rx_fh_l);
		AT1846SWriteReg2byte( 0x2a, rx_fl_h, rx_fl_l);
		AT1846SWriteReg2byte( 0x49, 0x0C, 0x15); // setting SQ open and shut threshold

		if (currentBandWidthIs25kHz)
		{
			// 25 kHz settings
			AT1846SWriteReg2byte( 0x30, 0x70, 0x26); // RX on
		}
		else
		{
			// 12.5 kHz settings
			AT1846SWriteReg2byte( 0x30, 0x40, 0x26); // RX on
		}

		trxUpdateC6000Calibration();
		trxUpdateAT1846SCalibration();

		if (!txPAEnabled)
		{
			if (trxCurrentBand[TRX_RX_FREQ_BAND] == RADIO_BAND_VHF)
			{
				GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 1);
				GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 0);
			}
			else
			{
				GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 0);
				GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 1);
			}
		}

		if (currentMode == RADIO_MODE_DIGITAL)
		{
			init_digital();
		}

		trxNextRssiNoiseSamplePIT = PITCounter + RSSI_NOISE_SAMPLE_PERIOD_PIT;
		taskEXIT_CRITICAL();
	}
}

int trxGetFrequency(void)
{
	if (trxTransmissionEnabled)
	{
		return currentTxFrequency;
	}
	else
	{
		return currentRxFrequency;
	}
}

void trxSetRX(void)
{
	if (currentMode == RADIO_MODE_ANALOG)
	{
		trxActivateRx();
	}
}

void trxSetTX(void)
{
	if ((currentTxFrequency != lastSetTxFrequency) || (lastSetTxPowerLevel != txPowerLevel))
	{
		trxCurrentBand[TRX_TX_FREQ_BAND] = trxGetBandFromFrequency(currentTxFrequency);
		calibrationGetPowerForFrequency(currentTxFrequency, &trxPowerSettings);
		lastSetTxFrequency = currentTxFrequency;
		lastSetTxPowerLevel = txPowerLevel;

		trxUpdate_PA_DAC_Drive();
	}

	trxTransmissionEnabled = true;

	// RX pre-amp off
	GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 0);
	GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 0);

	if (currentMode == RADIO_MODE_ANALOG)
	{
		trxActivateTx();
	}
}

void trxAT1846RxOff(void)
{
	AT1846SetClearReg2byteWithMask(0x30, 0xFF, 0xDF, 0x00, 0x00);
}

void trxAT1846RxOn(void)
{
	trxNextRssiNoiseSamplePIT = PITCounter + RSSI_NOISE_SAMPLE_PERIOD_PIT;
	AT1846SetClearReg2byteWithMask(0x30, 0xFF, 0xFF, 0x00, 0x20);
}

#ifndef NEW_PA_CONTROL

void trxActivateRx(void)
{
    DAC_SetBufferValue(DAC0, 0U, 0U);// PA drive power to zero

    // Possibly quicker to turn them both off, than to check which on is on and turn that one off
	GPIO_PinWrite(GPIO_VHF_TX_amp_power, Pin_VHF_TX_amp_power, 0);// VHF PA off
	GPIO_PinWrite(GPIO_UHF_TX_amp_power, Pin_UHF_TX_amp_power, 0);// UHF PA off

	txPAEnabled = false;

    if (trxCurrentBand[TRX_RX_FREQ_BAND] == RADIO_BAND_VHF)
	{
		GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 1);// VHF pre-amp on
		GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 0);// UHF pre-amp off
	}
    else
	{
		GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 0);// VHF pre-amp off
		GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 1);// UHF pre-amp on
	}

	if (currentBandWidthIs25kHz)
	{
		AT1846SWriteReg2byte( 0x30, 0x70, 0x06); 		// 25 kHz settings // RX off
	}
	else
	{
		AT1846SWriteReg2byte( 0x30, 0x40, 0x06); 		// 12.5 kHz settings // RX off
	}

	AT1846SWriteReg2byte( 0x29, rx_fh_h, rx_fh_l);
	AT1846SWriteReg2byte( 0x2a, rx_fl_h, rx_fl_l);

	if (currentBandWidthIs25kHz)
	{
		AT1846SWriteReg2byte( 0x30, 0x70, 0x26); // 25 kHz settings // RX on
	}
	else
	{
		AT1846SWriteReg2byte( 0x30, 0x40, 0x26); // 12.5 kHz settings // RX on
	}

	trxNextRssiNoiseSamplePIT = PITCounter + RSSI_NOISE_SAMPLE_PERIOD_PIT;
	trxRxSignal = 0;
	trxRxNoise = 0;
}

void trxActivateTx(void)
{
	txPAEnabled = true;
	trxRxSignal = 0;
	trxRxNoise = 0;
	AT1846SWriteReg2byte( 0x29, tx_fh_h, tx_fh_l);
	AT1846SWriteReg2byte( 0x2a, tx_fl_h, tx_fl_l);

	AT1846SetClearReg2byteWithMask(0x30, 0xFF, 0x1F, 0x00, 0x00); // Clear Tx and Rx bits
	if (currentMode == RADIO_MODE_ANALOG)
	{
		AT1846SetClearReg2byteWithMask(0x30, 0xFF, 0x1F, 0x00, 0x40); // analog TX
		trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_MIC);// For 1750 tone burst
		trxSetMicGainFM(nonVolatileSettings.micGainFM);
	}
	else
	{
		AT1846SetClearReg2byteWithMask(0x30, 0xFF, 0x1F, 0x00, 0xC0); // digital TX
	}

	// TX PA on
	if (trxCurrentBand[TRX_TX_FREQ_BAND] == RADIO_BAND_VHF)
	{
		GPIO_PinWrite(GPIO_UHF_TX_amp_power, Pin_UHF_TX_amp_power, 0);// I can't see why this would be needed. Its probably just for safety.
		GPIO_PinWrite(GPIO_VHF_TX_amp_power, Pin_VHF_TX_amp_power, 1);
	}
	else
	{
		GPIO_PinWrite(GPIO_VHF_TX_amp_power, Pin_VHF_TX_amp_power, 0);// I can't see why this would be needed. Its probably just for safety.
		GPIO_PinWrite(GPIO_UHF_TX_amp_power, Pin_UHF_TX_amp_power, 1);
	}
    DAC_SetBufferValue(DAC0, 0U, txDACDrivePower);	// PA drive to appropriate level
}

#else

void trxActivateRx(void)
{
    DAC_SetBufferValue(DAC0, 0U, 0U);// PA drive power to zero

    I2C_AT1846_set_register_with_mask(0x0A, 0x87FF, 0, 11);// Set AT1846S Tx power output to zero

    if (trxCurrentBand[TRX_RX_FREQ_BAND] == RADIO_BAND_VHF)
	{
    	GPIO_PinWrite(GPIO_VHF_TX_amp_power, Pin_VHF_TX_amp_power, 0);// VHF PA off
		GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 1);// VHF pre-amp on
	}
    else
    {
    	GPIO_PinWrite(GPIO_UHF_TX_amp_power, Pin_UHF_TX_amp_power, 0);// UHF PA off
		GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 1);// UHF pre-amp on
    }

	txPAEnabled = false;

	if (currentBandWidthIs25kHz)
	{
		AT1846SWriteReg2byte( 0x30, 0x70, 0x06); 		// 25 kHz   settings:  Tx and RX off
	}
	else
	{
		AT1846SWriteReg2byte( 0x30, 0x40, 0x06); 		// 12.5 kHz settings:  Tx and RX off
	}

	AT1846SWriteReg2byte( 0x29, rx_fh_h, rx_fh_l);
	AT1846SWriteReg2byte( 0x2a, rx_fl_h, rx_fl_l);

	if (currentBandWidthIs25kHz)
	{
		AT1846SWriteReg2byte( 0x30, 0x70, 0x26); // 25 kHz settings // RX on
	}
	else
	{
		AT1846SWriteReg2byte( 0x30, 0x40, 0x26); // 12.5 kHz settings // RX on
	}

	trxNextRssiNoiseSamplePIT = PITCounter + RSSI_NOISE_SAMPLE_PERIOD_PIT;
	trxRxSignal = 0;
	trxRxNoise = 0;
}

void trxActivateTx(void)
{
	txPAEnabled = true;
	trxRxSignal = 0;
	trxRxNoise = 0;
	AT1846SWriteReg2byte( 0x29, tx_fh_h, tx_fh_l);
	AT1846SWriteReg2byte( 0x2a, tx_fl_h, tx_fl_l);

	AT1846SetClearReg2byteWithMask(0x30, 0xFF, 0x1F, 0x00, 0x00); // Turn off Tx and Rx bits

	if (currentMode == RADIO_MODE_ANALOG)
	{
		AT1846SetClearReg2byteWithMask(0x30, 0xFF, 0x1F, 0x00, 0x40); // analog TX
		trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_MIC);// For 1750 tone burst
		trxSetMicGainFM(nonVolatileSettings.micGainFM);
	}
	else
	{
		AT1846SetClearReg2byteWithMask(0x30, 0xFF, 0x1F, 0x00, 0xC0); // digital TX
	}

	// TX PA on
	if (trxCurrentBand[TRX_TX_FREQ_BAND] == RADIO_BAND_VHF)
	{
		GPIO_PinWrite(GPIO_VHF_TX_amp_power, Pin_VHF_TX_amp_power, 1);
	}
	else
	{
		GPIO_PinWrite(GPIO_UHF_TX_amp_power, Pin_UHF_TX_amp_power, 1);
	}

	I2C_AT1846_set_register_with_mask(0x0A, 0x87FF, padrv_ibit, 11);// AT1846S Tx output level

	DAC_SetBufferValue(DAC0, 0U, txDACDrivePower);	// PA drive to appropriate level
}
#endif


void trxSetPowerFromLevel(int powerLevel)
{
	txPowerLevel = powerLevel;
}

void trxUpdate_PA_DAC_Drive(void)
{
// Note. Fraction values for 200Mhz are currently the same as the VHF band, because there isn't any way to set the 1W value on 220Mhz as there are only 2 calibration tables
#if defined(PLATFORM_GD77) || defined(PLATFORM_GD77S)

static const float fractionalPowers[3][7] = {	{0.59f,0.73f,0.84f,0.93f,0.60f,0.72f,0.77f},// VHF
												{0.62f,0.75f,0.85f,0.93f,0.49f,0.64f,0.71f},// 220Mhz
												{0.62f,0.75f,0.85f,0.93f,0.49f,0.64f,0.71f}};// UHF

#elif defined(PLATFORM_DM1801) || defined(PLATFORM_DM1801A)


static const float fractionalPowers[3][7] = {	{0.28f,0.37f,0.62f,0.82f,0.60f,0.72f,0.77f},// VHF - THESE VALUE HAVE NOT BEEN CALIBRATED
												{0.28f,0.37f,0.62f,0.82f,0.49f,0.64f,0.73f},// 220Mhz - THESE VALUE HAVE NOT BEEN CALIBRATED
												{0.05f,0.25f,0.51f,0.75f,0.49f,0.64f,0.71f}};// UHF - THESE VALUE HAVE NOT BEEN CALIBRATED

#elif defined(PLATFORM_RD5R)

static const float fractionalPowers[3][7] = {	{0.37f,0.54f,0.73f,0.87f,0.49f,0.64f,0.73f},// VHF
												{0.28f,0.37f,0.62f,0.82f,0.49f,0.64f,0.71f},// 220Mhz - THESE VALUE HAVE NOT BEEN CALIBRATED
												{0.05f,0.25f,0.45f,0.85f,0.49f,0.64f,0.71f}};// UHF

#endif

	switch(txPowerLevel)
	{
		case 0:// 50mW
		case 1:// 250mW
		case 2:// 500mW
		case 3:// 750mW
			txDACDrivePower = trxPowerSettings.lowPower * fractionalPowers[trxCurrentBand[TRX_TX_FREQ_BAND]][txPowerLevel];
			break;
		case 4:// 1W
			txDACDrivePower = trxPowerSettings.lowPower;
			break;
		case 5:// 2W
		case 6:// 3W
		case 7:// 4W
			{
				int stepPerWatt = (trxPowerSettings.highPower - trxPowerSettings.lowPower)/( 5 - 1);
				txDACDrivePower = (((txPowerLevel - 3) * stepPerWatt) * fractionalPowers[trxCurrentBand[TRX_TX_FREQ_BAND]][txPowerLevel-1]) + trxPowerSettings.lowPower;
			}
			break;
		case 8:// 5W
			txDACDrivePower = trxPowerSettings.highPower;
			break;
		case 9:// 5W+
			txDACDrivePower = nonVolatileSettings.userPower;
			break;
		default:
			txDACDrivePower = trxPowerSettings.lowPower;
			break;
	}

	if (txDACDrivePower > MAX_PA_DAC_VALUE)
	{
		txDACDrivePower = MAX_PA_DAC_VALUE;
	}
}

uint16_t trxGetPA_DAC_Drive(void)
{
	return txDACDrivePower;
}

int trxGetPowerLevel(void)
{
	return txPowerLevel;
}

void trxCalcBandAndFrequencyOffset(CalibrationBand_t *calibrationBand, uint32_t *freq_offset)
{
// NOTE. For crossband duplex DMR, the calibration potentially needs to be changed every time the Tx/Rx is switched over on each 30ms cycle
// But at the moment this is an unnecessary complication and I'll just use the Rx frequency to get the calibration offsets

	if (trxCurrentBand[TRX_RX_FREQ_BAND] == RADIO_BAND_UHF)
	{
		*calibrationBand = CalibrationBandUHF;
		*freq_offset = (currentTxFrequency - 40000000)/1000000;
	}
	else
	{
		*calibrationBand = CalibrationBandVHF;
		*freq_offset = (currentTxFrequency - 13250000)/500000;
	}

	// Limit VHF freq calculation exceeds the max lookup table index (of 7)
	if (*freq_offset > 7)
	{
		*freq_offset = 7;
	}
}

void trxUpdateC6000Calibration(void)
{
	uint32_t freq_offset;
	CalibrationBand_t calBand;
	CalibrationDataResult_t calRes;

	trxCalcBandAndFrequencyOffset(&calBand, &freq_offset);

	SPI0WritePageRegByte(0x04, 0x00, 0x3F); // Reset HR-C6000 state

	calibrationGetSectionData(calBand, CalibrationSection_DACDATA_SHIFT, &calRes);
	SPI0WritePageRegByte(0x04, 0x37, calRes.value); // DACDATA shift (LIN_VOL)

	calibrationGetSectionData(calBand, CalibrationSection_Q_MOD2_OFFSET, &calRes);
	SPI0WritePageRegByte(0x04, 0x04, calRes.value); // MOD2 offset

	calRes.offset = freq_offset;
	calibrationGetSectionData(calBand, CalibrationSection_PHASE_REDUCE, &calRes);
	SPI0WritePageRegByte(0x04, 0x46, calRes.value); // phase reduce

	calibrationGetSectionData(calBand, CalibrationSection_TWOPOINT_MOD, &calRes);
	uint16_t refOscOffset = calRes.value; //(highByte<<8)+lowByte;

	if (refOscOffset > 1023)
	{
		refOscOffset = 1023;
	}
	SPI0WritePageRegByte(0x04, 0x48, (refOscOffset >> 8) & 0x03); // bit 0 to 1 = upper 2 bits of 10-bit twopoint mod
	SPI0WritePageRegByte(0x04, 0x47, (refOscOffset & 0xFF)); // bit 0 to 7 = lower 8 bits of 10-bit twopoint mod
}

void I2C_AT1846_set_register_with_mask(uint8_t reg, uint16_t mask, uint16_t value, uint8_t shift)
{
	AT1846SetClearReg2byteWithMask(reg, (mask & 0xff00) >> 8, (mask & 0x00ff) >> 0, ((value << shift) & 0xff00) >> 8, ((value << shift) & 0x00ff) >> 0);
}

void trxUpdateAT1846SCalibration(void)
{
	uint32_t freq_offset = 0x00000000;
	CalibrationBand_t calBand;
	CalibrationDataResult_t calRes;

	trxCalcBandAndFrequencyOffset(&calBand, &freq_offset);

	uint8_t val_pga_gain;
	uint8_t gain_tx;


	uint16_t xmitter_dev;

	uint8_t dac_vgain_analog;
	uint8_t volume_analog;

	uint16_t noise1_th;
	uint16_t noise2_th;
	uint16_t rssi3_th;

	uint16_t squelch_th;

	calibrationGetSectionData(calBand, CalibrationSection_PGA_GAIN, &calRes);
	val_pga_gain = calRes.value;

	calibrationGetSectionData(calBand, CalibrationSection_VOICE_GAIN_TX, &calRes);
	voice_gain_tx = calRes.value;

	calibrationGetSectionData(calBand, CalibrationSection_GAIN_TX, &calRes);
	gain_tx = calRes.value;

	calibrationGetSectionData(calBand, CalibrationSection_PADRV_IBIT, &calRes);
	padrv_ibit = calRes.value;


	// 25 or 12.5 kHz settings
	calibrationGetSectionData(calBand,
			(currentBandWidthIs25kHz ? CalibrationSection_XMITTER_DEV_WIDEBAND : CalibrationSection_XMITTER_DEV_NARROWBAND), &calRes);
	xmitter_dev = calRes.value;

	if (currentMode == RADIO_MODE_ANALOG)
	{
		calibrationGetSectionData(calBand, CalibrationSection_DAC_VGAIN_ANALOG, &calRes);
		dac_vgain_analog = calRes.value;

		calibrationGetSectionData(calBand, CalibrationSection_VOLUME_ANALOG, &calRes);
		volume_analog = calRes.value;
	}
	else
	{
		dac_vgain_analog = 0x0C;
		volume_analog = 0x0C;
	}

	calibrationGetSectionData(calBand,
			(currentBandWidthIs25kHz ? CalibrationSection_NOISE1_TH_WIDEBAND : CalibrationSection_NOISE1_TH_NARROWBAND), &calRes);
	noise1_th = calRes.value;

	calibrationGetSectionData(calBand,
			(currentBandWidthIs25kHz ? CalibrationSection_NOISE2_TH_WIDEBAND : CalibrationSection_NOISE2_TH_NARROWBAND), &calRes);
	noise2_th = calRes.value;

	calibrationGetSectionData(calBand,
			(currentBandWidthIs25kHz ? CalibrationSection_RSSI3_TH_WIDEBAND : CalibrationSection_RSSI3_TH_NARROWBAND), &calRes);
	rssi3_th = calRes.value;

	calRes.mod = (currentBandWidthIs25kHz ? 0 : 3);
	calibrationGetSectionData(calBand, CalibrationSection_SQUELCH_TH, &calRes);
	squelch_th = calRes.value;

	I2C_AT1846_set_register_with_mask(0x0A, 0xF83F, val_pga_gain, 6);
	I2C_AT1846_set_register_with_mask(0x41, 0xFF80, voice_gain_tx, 0);
	I2C_AT1846_set_register_with_mask(0x44, 0xF0FF, gain_tx, 8);

	I2C_AT1846_set_register_with_mask(0x59, 0x003f, xmitter_dev, 6);
	I2C_AT1846_set_register_with_mask(0x44, 0xFF0F, dac_vgain_analog, 4);
	I2C_AT1846_set_register_with_mask(0x44, 0xFFF0, volume_analog, 0);

	I2C_AT1846_set_register_with_mask(0x48, 0x0000, noise1_th, 0);
	I2C_AT1846_set_register_with_mask(0x60, 0x0000, noise2_th, 0);
	I2C_AT1846_set_register_with_mask(0x3f, 0x0000, rssi3_th, 0);

#ifndef NEW_PA_CONTROL
	I2C_AT1846_set_register_with_mask(0x0A, 0x87FF, padrv_ibit, 11);// This is now done during trxActiveTx
#else
	I2C_AT1846_set_register_with_mask(0x0A, 0x87FF, 0, 11); // set power to zero
#endif
	I2C_AT1846_set_register_with_mask(0x49, 0x0000, squelch_th, 0);
}

void trxSetDMRColourCode(int colourCode)
{
	SPI0WritePageRegByte(0x04, 0x1F, (colourCode << 4)); // DMR Colour code in upper 4 bits.
	currentCC = colourCode;
}

int trxGetDMRColourCode(void)
{
	return currentCC;
}

int trxGetDMRTimeSlot(void)
{
	return trxCurrentDMRTimeSlot;
}

void trxSetDMRTimeSlot(int timeslot)
{
	trxCurrentDMRTimeSlot = timeslot;
}

void trxUpdateTsForCurrentChannelWithSpecifiedContact(struct_codeplugContact_t *contactData)
{
	// Contact TS override ?
	if ((nonVolatileSettings.overrideTG == 0) && (contactData->reserve1 & 0x01) == 0x00)
	{
		if (tsIsContactHasBeenOverriddenFromCurrentChannel())
		{
			trxCurrentDMRTimeSlot = (tsGetManualOverrideFromCurrentChannel() - 1);
		}
		else
		{
			if ((contactData->reserve1 & 0x02) != 0)
			{
				trxCurrentDMRTimeSlot = 1;
			}
			else
			{
				trxCurrentDMRTimeSlot = 0;
			}
		}
	}
	else
	{
		int8_t overriddenTS = tsGetManualOverrideFromCurrentChannel();

		// No manual override
		if (overriddenTS == 0)
		{
			// Apply channnel TS
			trxCurrentDMRTimeSlot = ((currentChannelData->flag2 & 0x40) != 0);
		}
		else
		{
			// Restore overriden TS (as previous contact may have changed it
			trxCurrentDMRTimeSlot = (overriddenTS - 1);
		}
	}
}

void trxSetTxCSS(uint16_t tone)
{
	taskENTER_CRITICAL();
	CodeplugCSSTypes_t type = codeplugGetCSSType(tone);
	if (type == CSS_TYPE_NONE)
	{
		// tone value of 0xffff in the codeplug seem to be a flag that no tone has been selected
		// Zero the CTCSS1 Register
		AT1846SWriteReg2byte(0x4a, 0x00, 0x00);
		// Zero the CTCSS2 Register
		AT1846SWriteReg2byte(0x4d, 0x00, 0x00);
		// disable the transmit CTCSS/DCS
		AT1846SetClearReg2byteWithMask(0x4e, 0xF9, 0xFF, 0x00, 0x00);
	}
	else if (type == CSS_TYPE_CTCSS)
	{
		// value that is stored is 100 time the tone freq but its stored in the codeplug as freq times 10
		tone *= 10;
		// CTCSS 1
		AT1846SWriteReg2byte(0x4a, (tone >> 8) & 0xff, (tone & 0xff));
		// Zero CTCSS 2
		AT1846SWriteReg2byte(0x4d, 0x00, 0x00);
		// init cdcss_code
		AT1846SWriteReg2byte(0x4b, 0x00, 0x00);
		AT1846SWriteReg2byte(0x4c, 0x00, 0x00);
		// enable the transmit CTCSS
		AT1846SetClearReg2byteWithMask(0x4e, 0xF9, 0xFF, 0x06, 0x00);
	}
	else if (type & CSS_TYPE_DCS)
	{
		// Set the CTCSS1 Register to 134.4Hz (DCS data rate)
		AT1846SWriteReg2byte(0x4a, (TRX_DCS_TONE >> 8) & 0xff, TRX_DCS_TONE & 0xff);
		// Zero the CTCSS2 Register
		AT1846SWriteReg2byte(0x4d, 0x00, 0x00);

		// The AT1846S wants the Golay{23,12} encoding of the DCS code, rather than just the code itself.
		uint32_t encoded = dcsGetBitPatternFromCode(convertCSSNative2BinaryCodedOctal(tone & ~CSS_TYPE_DCS_MASK));
		AT1846SWriteReg2byte(0x4b, 0x00, (encoded >> 16) & 0xff);           // init cdcss_code
		AT1846SWriteReg2byte(0x4c, (encoded >> 8) & 0xff, encoded & 0xff);  // init cdcss_code

		uint8_t reg4e_high = ((type & CSS_TYPE_DCS_INVERTED) ? 0x05 : 0x04);
		AT1846SetClearReg2byteWithMask(0x4e, 0x38, 0x3F, reg4e_high, 0x00); // enable transmit DCS
	}
	taskEXIT_CRITICAL();
}

void trxSetRxCSS(uint16_t tone)
{
	taskENTER_CRITICAL();
	CodeplugCSSTypes_t type = codeplugGetCSSType(tone);
	if (type == CSS_TYPE_NONE)
	{
		// tone value of 0xffff in the codeplug seem to be a flag that no tone has been selected
		// Zero the CTCSS1 Register
		AT1846SWriteReg2byte(0x4a, 0x00, 0x00);
		// Zero the CTCSS2 Register
		AT1846SWriteReg2byte(0x4d, 0x00, 0x00);
		// disable the transmit CTCSS/DCS
		AT1846SetClearReg2byteWithMask(0x4e, 0xF9, 0xFF, 0x00, 0x00);
		rxCSSactive = false;
	}
	else if (type == CSS_TYPE_CTCSS)
	{
		int threshold = (2500 - tone) / 100;  // adjust threshold value to match tone frequency.
		if (tone > 2400)
		{
			threshold = 1;
		}
		// value that is stored is 100 time the tone freq but its stored in the codeplug as freq times 10
		tone *= 10;
		// CTCSS 1
		AT1846SWriteReg2byte(0x4a, (tone >> 8) & 0xff, (tone & 0xff));
		// Zero CTCSS 2
		AT1846SWriteReg2byte(0x4d, 0x00, 0x00);
		// set the detection thresholds
		AT1846SWriteReg2byte(0x5b, (threshold & 0xFF), (threshold & 0xFF));
		// set detection to CTCSS 1
		AT1846SetClearReg2byteWithMask(0x3a, 0xFF, 0xE0, 0x00, 0x01);

		rxCSSactive = (trxAnalogFilterLevel != ANALOG_FILTER_NONE);
		// Force closing the AudioAmp
		disableAudioAmp(AUDIO_AMP_MODE_RF);
		analogSignalReceived = false;
		analogTriggeredAudio = false;
	}
	else if (type & CSS_TYPE_DCS)
	{
		// Set the CTCSS1 Register to 134.4Hz (DCS data rate)
		AT1846SWriteReg2byte(0x4a, (TRX_DCS_TONE >> 8) & 0xff, TRX_DCS_TONE & 0xff);
		// Zero the CTCSS2 Register
		AT1846SWriteReg2byte(0x4d, 0x00, 0x00);

		// The AT1846S wants the Golay{23,12} encoding of the DCS code, rather than just the code itself.
		uint32_t encoded = dcsGetBitPatternFromCode(convertCSSNative2BinaryCodedOctal(tone & ~CSS_TYPE_DCS_MASK));
		AT1846SWriteReg2byte(0x4b, 0x00, (encoded >> 16) & 0xff);           // init cdcss_code
		AT1846SWriteReg2byte(0x4c, (encoded >> 8) & 0xff, encoded & 0xff);  // init cdcss_code

		uint8_t reg4e_high = ((type & CSS_TYPE_DCS_INVERTED) ? 0x05 : 0x04);
		uint8_t reg3a_low = ((type & CSS_TYPE_DCS_INVERTED) ? 0x04 : 0x02);
		// The cdcss_sel bits have to be set for DCS receive to work
		AT1846SetClearReg2byteWithMask(0x4e, 0x38, 0x3F, reg4e_high, 0x00); // enable transmit DCS
		AT1846SetClearReg2byteWithMask(0x3a, 0xFF, 0xE0, 0x00, reg3a_low); // enable receive DCS

		//set_clear_I2C_reg_2byte_with_mask(0x4e, 0xF9, 0xFF, 0x04, 0x00); // enable transmit DCS
		rxCSSactive = (trxAnalogFilterLevel != ANALOG_FILTER_NONE);
		// Force closing the AudioAmp
		disableAudioAmp(AUDIO_AMP_MODE_RF);
		analogSignalReceived = false;
		analogTriggeredAudio = false;
	}
	taskEXIT_CRITICAL();
}

bool trxCheckCSSFlag(uint16_t tone)
{
	uint8_t FlagsH;
	uint8_t FlagsL;
	status_t retval;

	taskENTER_CRITICAL();
	retval = AT1846SReadReg2byte(0x1c, &FlagsH, &FlagsL);
	taskEXIT_CRITICAL();

	CodeplugCSSTypes_t type = codeplugGetCSSType(tone);

	return ((retval == kStatus_Success) && ((type != CSS_TYPE_NONE) && ((FlagsL & 0x05) == 0x05)));
}

void trxUpdateDeviation(int channel)
{
	CalibrationBand_t calBand = ((trxCurrentBand[TRX_RX_FREQ_BAND] == RADIO_BAND_UHF) ? CalibrationBandUHF : CalibrationBandVHF);
	CalibrationDataResult_t calRes;
	uint8_t deviation;

	taskENTER_CRITICAL();
	switch (channel)
	{
		case AT1846_VOICE_CHANNEL_TONE1:
		case AT1846_VOICE_CHANNEL_TONE2:
			calRes.offset = CAL_DEV_TONE;
			calibrationGetSectionData(calBand, CalibrationSection_DEV_TONE, &calRes);
			deviation = (calRes.value & 0x7f);
			I2C_AT1846_set_register_with_mask(0x41, 0xFF80, deviation, 0);// Tone deviation value
			break;

		case AT1846_VOICE_CHANNEL_DTMF:
			calRes.offset = CAL_DEV_DTMF;
			calibrationGetSectionData(calBand, CalibrationSection_DEV_TONE, &calRes);
			deviation = (calRes.value & 0x7f);
			I2C_AT1846_set_register_with_mask(0x41, 0xFF80, deviation, 0);//  Tone deviation value
			break;
	}
	taskEXIT_CRITICAL();
}

uint8_t trxGetCalibrationVoiceGainTx(void)
{
	return voice_gain_tx;
}

void trxSelectVoiceChannel(uint8_t channel) {
	uint8_t valh;
	uint8_t vall;

	taskENTER_CRITICAL();
	switch (channel)
	{
		case AT1846_VOICE_CHANNEL_TONE1:
		case AT1846_VOICE_CHANNEL_TONE2:
		case AT1846_VOICE_CHANNEL_DTMF:
			AT1846SetClearReg2byteWithMask(0x79, 0xff, 0xff, 0xc0, 0x00); // Select single tone
			AT1846SetClearReg2byteWithMask(0x57, 0xff, 0xfe, 0x00, 0x01); // Audio feedback on

			AT1846SReadReg2byte( 0x41, &valh, &trxSaveVoiceGainTx);
			trxSaveVoiceGainTx &= 0x7f;

			AT1846SReadReg2byte( 0x59, &valh, &vall);
			trxSaveDeviation = (vall + (valh << 8)) >> 6;

			trxUpdateDeviation(channel);
			break;
		default:
			AT1846SetClearReg2byteWithMask(0x57, 0xff, 0xfe, 0x00, 0x00); // Audio feedback off
			if (trxSaveVoiceGainTx != 0xff)
			{
				I2C_AT1846_set_register_with_mask(0x41, 0xFF80, trxSaveVoiceGainTx, 0);
				trxSaveVoiceGainTx = 0xff;
			}
			if (trxSaveDeviation != 0xFF)
			{
				I2C_AT1846_set_register_with_mask(0x59, 0x003f, trxSaveDeviation, 6);
				trxSaveDeviation = 0xFF;
			}
			break;
	}
	AT1846SetClearReg2byteWithMask(0x3a, 0x8f, 0xff, channel, 0x00);
	taskEXIT_CRITICAL();
}

void trxSetTone1(int toneFreq)
{
	toneFreq = toneFreq * 10;
	taskENTER_CRITICAL();
	AT1846SWriteReg2byte( 0x35, (toneFreq >> 8) & 0xff, (toneFreq & 0xff));   // tone1_freq
	taskEXIT_CRITICAL();
}

void trxSetTone2(int toneFreq)
{
	toneFreq = toneFreq * 10;
	taskENTER_CRITICAL();
	AT1846SWriteReg2byte( 0x36, (toneFreq >> 8) & 0xff, (toneFreq & 0xff));   // tone2_freq
	taskEXIT_CRITICAL();
}

void trxSetDTMF(int code)
{
	if (code < 16)
	{
		trxSetTone1(trxDTMFfreq1[code]);
		trxSetTone2(trxDTMFfreq2[code]);
	}
}

// Codeplug format (hex) -> octal
static uint16_t convertCSSNative2BinaryCodedOctal(uint16_t nativeCSS)
{
	uint16_t octalCSS = 0;
	uint16_t shift = 0;

	while (nativeCSS)
	{
		octalCSS += (nativeCSS & 0xF) << shift;
		nativeCSS >>= 4;
		shift += 3;
	}
	return octalCSS;
}

// Lookup for Golay pattern, then returns the full bit pattern for given DCS code
static uint32_t dcsGetBitPatternFromCode(uint16_t dcs)
{
	uint16_t startPos = 0;
	uint16_t endPos = (DCS_PACKED_DATA_NUM - 1);
	uint16_t curPos;
	uint8_t *p = (uint8_t *)DCS_PACKED_DATA;

	while (startPos <= endPos)
	{
		curPos = (startPos + endPos) >> 1;

		uint32_t entry = *(uint32_t *)(p + (DCS_PACKED_DATA_ENTRY_SIZE * curPos)) & 0x00FFFFFF;
		uint16_t foundCode = (entry >> 15) & 0x1FF;

		if (foundCode < dcs)
		{
			startPos = curPos + 1;
		}
		else if (foundCode > dcs)
		{
			endPos = curPos - 1;
		}
		else
		{
			return (((entry & 0x7FF) << 12) | 0x800 | dcs);
		}
	}

	return 0x00;
}

void trxSetMicGainFM(uint8_t gain)
{
	uint8_t gain_tx = trxGetCalibrationVoiceGainTx();

	// Apply extra gain 17 (the calibration default value, not the datasheet one)
	if (gain > 17)
	{
		gain_tx += (gain - 16); // Seems to be enough
	}

	I2C_AT1846_set_register_with_mask(0x0A, 0xF83F, gain, 6);
	I2C_AT1846_set_register_with_mask(0x41, 0xFF80, gain_tx, 0);
}

void trxEnableTransmission(void)
{
	LEDs_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
	LEDs_PinWrite(GPIO_LEDred, Pin_LEDred, 1);

	txstopdelay = 0;
	trxSetTX();
}

void trxDisableTransmission(void)
{
	LEDs_PinWrite(GPIO_LEDred, Pin_LEDred, 0);
	// Need to wrap this in Task Critical to avoid bus contention on the I2C bus.
	taskENTER_CRITICAL();
	trxActivateRx();
	taskEXIT_CRITICAL();
}

void trxPowerUpDownRxAndC6000(bool powerUp, bool includeC6000)
{
	// Check the radio is not transmitting.
	if (powerUp == powerUpDownState || 	trxTransmissionEnabled || trxIsTransmitting)
	{
		return;
	}

	if (powerUp)
	{
#if USE_AT1846S_DEEP_SLEEP
		// Wake the AT1846S. But Rx still off.
		if (currentBandWidthIs25kHz)
		{
			// 25 kHz settings
			AT1846SWriteReg2byte( 0x30, 0x70, 0x06); // RX off
		}
		else
		{
			// 12.5 kHz settings
			AT1846SWriteReg2byte( 0x30, 0x40, 0x06); // RX off
		}

		vTaskDelay(portTICK_PERIOD_MS * 10);// The data sheet says this takes 10 milliseconds, but this does not seem to be necessary
#endif


		// Always power up the C6000 even if its may already be powered up
		GPIO_PinWrite(GPIO_INT_C6000_PWD, Pin_INT_C6000_PWD, 0);// Power Up the C6000

		// Now enable the Rx
		if (currentBandWidthIs25kHz)
		{
			// 25 kHz settings
			AT1846SWriteReg2byte( 0x30, 0x70, 0x26); // RX on
		}
		else
		{
			// 12.5 kHz settings
			AT1846SWriteReg2byte( 0x30, 0x40, 0x26); // RX on
		}


		// allow some time between power up of C6000 to NVIC enable
		// Always Enable the NVIC even if its already enabled, to ensure it is always enabled.
		NVIC_EnableIRQ(PORTC_IRQn);


		if (trxCurrentBand[TRX_RX_FREQ_BAND] == RADIO_BAND_VHF)
		{
			GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 1);// VHF pre-amp on
		}
		else
		{
			GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 1);// UHF pre-amp on
		}
	}
	else
	{
		if (currentBandWidthIs25kHz)
		{
			// 25 kHz settings
			AT1846SWriteReg2byte( 0x30, 0x70, 0x06); // RX off
		}
		else
		{
			// 12.5 kHz settings
			AT1846SWriteReg2byte( 0x30, 0x40, 0x06); // RX off
		}
#if USE_AT1846S_DEEP_SLEEP
		AT1846SWriteReg2byte(0x30, 0x00, 0x00); // Now enter power down mode
#endif

		taskENTER_CRITICAL();
		if (!voicePromptsIsPlaying())
		{
			if (includeC6000)
			{
				NVIC_DisableIRQ(PORTC_IRQn);

				GPIO_PinWrite(GPIO_INT_C6000_PWD, Pin_INT_C6000_PWD, 1);// Power down the C6000
			}
		}
		taskEXIT_CRITICAL();

		GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 0);// VHF pre-amp on
		GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 0);// UHF pre-amp off
	}

	 powerUpDownState = powerUp;
}
