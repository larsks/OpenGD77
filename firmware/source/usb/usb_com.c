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
#include <stdarg.h>
#include "functions/hotspot.h"
#include "functions/settings.h"
#include "user_interface/uiUtilities.h"
#include "user_interface/menuSystem.h"
#include "usb/usb_com.h"
#include "functions/ticks.h"
#include "interfaces/wdog.h"
#include "hardware/HR-C6000.h"
#include "functions/sound.h"
#include "hardware/SPI_Flash.h"
#include "user_interface/uiLocalisation.h"
#include "functions/rxPowerSaving.h"

//#define LOOKUP_ENABLED 1

static void handleCPSRequest(void);

volatile int com_request = 0;
__attribute__((section(".data.$RAM2"))) volatile uint8_t com_requestbuffer[COM_REQUESTBUFFER_SIZE];
__attribute__((section(".data.$RAM2"))) USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) uint8_t usbComSendBuf[COM_BUFFER_SIZE];//DATA_BUFF_SIZE
int sector = -1;
volatile int comRecvMMDVMIndexIn = 0;
volatile int comRecvMMDVMIndexOut = 0;
volatile int comRecvMMDVMFrameCount = 0;
static bool flashingDMRIDs = false;
static bool channelsRewritten = false;

bool isCompressingAMBE = false;


void tick_com_request(void)
{
	switch (settingsUsbMode)
	{
		case USB_MODE_CPS:
			if (com_request == 1)
			{
				handleCPSRequest();
				com_request = 0;
			}
			break;

		case USB_MODE_HOTSPOT:
			// That will happen once when MMDVMHost send the first frame
			// out of the Hotspot mode.
			if (com_request == 1)
			{
				com_request = 0;

				if ((nonVolatileSettings.hotspotType != HOTSPOT_TYPE_OFF) &&
						((comRecvMMDVMFrameCount >= 1) && (com_requestbuffer[1] == MMDVM_FRAME_START)) &&
						(uiDataGlobal.dmrDisabled == false)) // DMR (digital) is disabled.
				{
					if (menuSystemGetCurrentMenuNumber() != UI_HOTSPOT_MODE)
					{
						menuSystemPushNewMenu(UI_HOTSPOT_MODE);
					}
				}
			}
			break;
	}
}

enum CPS_ACCESS_AREA { 	CPS_ACCESS_FLASH = 1,
						CPS_ACCESS_EEPROM = 2,
						CPS_ACCESS_MCU_ROM = 5,
						CPS_ACCESS_DISPLAY_BUFFER = 6,
						CPS_ACCESS_WAV_BUFFER = 7,
						CPS_COMPRESS_AND_ACCESS_AMBE_BUFFER = 8,
						CPS_ACCESS_RADIO_INFO = 9,
						};

static void cpsHandleReadCommand(void)
{
	uint32_t address = (com_requestbuffer[2] << 24) + (com_requestbuffer[3] << 16) + (com_requestbuffer[4] << 8) + (com_requestbuffer[5] << 0);
	uint32_t length = (com_requestbuffer[6] << 8) + (com_requestbuffer[7] << 0);
	bool result = false;

	watchdogRun(false);

	if (length > (COM_REQUESTBUFFER_SIZE - 3))
	{
		length = (COM_REQUESTBUFFER_SIZE - 3);
	}

	switch(com_requestbuffer[1])
	{
		case CPS_ACCESS_FLASH:
			result = SPI_Flash_read(address, &usbComSendBuf[3], length);
			break;
		case CPS_ACCESS_EEPROM:
			result = EEPROM_Read(address, &usbComSendBuf[3], length);
			break;
		case CPS_ACCESS_MCU_ROM:
			memcpy(&usbComSendBuf[3], (uint8_t *)address, length);
			result = true;
			break;
		case CPS_ACCESS_DISPLAY_BUFFER:
			memcpy(&usbComSendBuf[3], displayGetPrimaryScreenBuffer() + address, length);
			result = true;
			break;
		case CPS_ACCESS_WAV_BUFFER:
			memcpy(&usbComSendBuf[3], (uint8_t *)&audioAndHotspotDataBuffer.rawBuffer[address], length);
			result = true;
			break;
		case CPS_COMPRESS_AND_ACCESS_AMBE_BUFFER:// read from ambe audio buffer
			{
				uint8_t ambeBuf[32];// ambe data is up to 27 bytes long, but the normal transfer length for the CPS is 32, so make the buffer big enough for that transfer size
				memset(ambeBuf, 0, 32);// Clear the ambe output buffer
				codecEncode((uint8_t *)ambeBuf, 3);
				memcpy(&usbComSendBuf[3], ambeBuf, length);// The ambe data is only 27 bytes long but the normal CPS request size is 32
				memset((uint8_t *)&audioAndHotspotDataBuffer.rawBuffer[0], 0x00, 960);// clear the input wave buffer, in case the next transfer is not a complete AMBE frame. 960 bytes compresses to 27 bytes of AMBE
				result = true;
			}
			break;
		case CPS_ACCESS_RADIO_INFO:
			{
				struct
				{
					uint32_t structVersion;
					uint32_t radioType;
					char gitRevision[16];
					char buildDateTime[16];
					uint32_t flashId;
				} radioInfo;

				radioInfo.structVersion = 0x01;
#if defined(PLATFORM_GD77)
				radioInfo.radioType = 0;
#elif defined(PLATFORM_GD77S)
				radioInfo.radioType = 1;
#elif defined(PLATFORM_DM1801)
				radioInfo.radioType = 2;
#elif defined(PLATFORM_RD5R)
				radioInfo.radioType = 3;
#elif defined(PLATFORM_DM1801A)
				radioInfo.radioType = 4;
#elif defined(PLATFOR_MD9600)
				radioInfo.radioType = 5;
#endif
				snprintf(radioInfo.gitRevision, sizeof(radioInfo.gitRevision), "%s", GITVERSION);
				snprintf(radioInfo.buildDateTime, sizeof(radioInfo.buildDateTime), "%04d%02d%02d%02d%02d%02d", BUILD_YEAR, BUILD_MONTH, BUILD_DAY, BUILD_HOUR, BUILD_MIN, BUILD_SEC);
				radioInfo.flashId = flashChipPartNumber;

				length = sizeof(radioInfo);
				memcpy(&usbComSendBuf[3], &radioInfo, length);
				result = true;
			}
			break;
	}

	if (result)
	{
		usbComSendBuf[0] = com_requestbuffer[0];
		usbComSendBuf[1] = (length >> 8) & 0xFF;
		usbComSendBuf[2] = (length >> 0) & 0xFF;
		USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, usbComSendBuf, length + 3);
	}
	else
	{
		usbComSendBuf[0] = '-';
		USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, usbComSendBuf, 1);
	}

	watchdogRun(true);
}

static void cpsHandleWriteCommand(void)
{
	bool ok = false;

	watchdogRun(false);

	switch(com_requestbuffer[1])
	{
		case 1:
			if (sector == -1)
			{
				sector = (com_requestbuffer[2] << 16) + (com_requestbuffer[3] << 8) + (com_requestbuffer[4] << 0);

				if ((sector * 4096) == 0x30000) // start address of DMRIDs DB
				{
					flashingDMRIDs = true;
				}

				ok = SPI_Flash_read(sector * 4096, SPI_Flash_sectorbuffer, 4096);
			}
			break;
		case 2:
			if (sector >= 0)
			{
				uint32_t address = (com_requestbuffer[2] << 24) + (com_requestbuffer[3] << 16) + (com_requestbuffer[4] << 8) + (com_requestbuffer[5] << 0);
				uint32_t length = (com_requestbuffer[6] << 8) + (com_requestbuffer[7] << 0);

#if !defined(PLATFORM_GD77S)
				// Temporary hack to automatically set Prompt to Level 1
				// A better solution will be added to the CPS and firmware at a later date.
				if (address == VOICE_PROMPTS_FLASH_HEADER_ADDRESS || address == VOICE_PROMPTS_FLASH_OLD_HEADER_ADDRESS)
				{
					if (voicePromptsCheckMagicAndVersion((uint32_t *)&com_requestbuffer[8]))
					{
						nonVolatileSettings.audioPromptMode = AUDIO_PROMPT_MODE_VOICE_LEVEL_1;
					}
				}
#endif

				if (length > (COM_REQUESTBUFFER_SIZE - 8))
				{
					length = (COM_REQUESTBUFFER_SIZE - 8);
				}

				for (int i = 0; i < length; i++)
				{
					if (sector == (address + i) / 4096)
					{
						SPI_Flash_sectorbuffer[(address + i) % 4096] = com_requestbuffer[i + 8];
					}
				}

				ok = true;
			}
			break;
		case 3:
			if (sector >= 0)
			{
				ok = SPI_Flash_eraseSector(sector * 4096);
				if (ok)
				{
					// Write the 16 pages of the sector
					for (int i = 0; i < 16; i++)
					{
						ok = SPI_Flash_writePage(sector * 4096 + i * 256, SPI_Flash_sectorbuffer + i * 256);
						if (!ok)
						{
							break;
						}
					}
				}

				sector = -1;
			}
			break;
		case 4:
			{
				uint32_t address = (com_requestbuffer[2] << 24) + (com_requestbuffer[3] << 16) + (com_requestbuffer[4] << 8) + (com_requestbuffer[5] << 0);
				uint32_t length = (com_requestbuffer[6] << 8) + (com_requestbuffer[7] << 0);

				// Channel is going to be rewritten, will need to reset current zone/etc...
				if (address == CODEPLUG_ADDR_CHANNEL_HEADER_EEPROM)
				{
					channelsRewritten = true;
				}

				if (length > (COM_REQUESTBUFFER_SIZE - 8))
				{
					length = (COM_REQUESTBUFFER_SIZE - 8);
				}


				// Temporary hack to prevent the QuickKeys getting overwritten by the codeplug
				const int QUICKKEYS_BLOCK_END = (CODEPLUG_ADDR_QUICKKEYS + (CODEPLUG_QUICKKEYS_SIZE * sizeof(uint16_t)) - 1);
				int end = (address + length) - 1;

				if (((address >= CODEPLUG_ADDR_QUICKKEYS) && (address <= QUICKKEYS_BLOCK_END))
						|| ((end >= CODEPLUG_ADDR_QUICKKEYS) && (end <= QUICKKEYS_BLOCK_END))
						|| ((address < CODEPLUG_ADDR_QUICKKEYS) && (end > QUICKKEYS_BLOCK_END)))
				{
					if (address < CODEPLUG_ADDR_QUICKKEYS)
					{
						ok = EEPROM_Write(address, (uint8_t*)com_requestbuffer + 8, (CODEPLUG_ADDR_QUICKKEYS - address));

						if (ok && (end > QUICKKEYS_BLOCK_END))
						{
							ok = EEPROM_Write((QUICKKEYS_BLOCK_END + 1), (uint8_t *)com_requestbuffer + 8 + ((QUICKKEYS_BLOCK_END + 1) - address),  (end - QUICKKEYS_BLOCK_END));
						}

					}
					else
					{
						if ((address <= QUICKKEYS_BLOCK_END) && (end > QUICKKEYS_BLOCK_END))
						{
							ok = EEPROM_Write((QUICKKEYS_BLOCK_END + 1), (uint8_t *)com_requestbuffer + 8 + ((QUICKKEYS_BLOCK_END + 1) - address), (end - QUICKKEYS_BLOCK_END));
						}
						else
						{
							ok = true;
						}
					}
					//	SEGGER_RTT_printf(0, "0x%06x\t0x%06x\n",address,end);
				}
				else
				{
					ok = EEPROM_Write(address, (uint8_t *)com_requestbuffer + 8, length);
				}
			}
			break;
		case CPS_ACCESS_WAV_BUFFER:// write to raw audio buffer
			{
				uint32_t address = (com_requestbuffer[2] << 24) + (com_requestbuffer[3] << 16) + (com_requestbuffer[4] << 8) + (com_requestbuffer[5] << 0);
				uint32_t length = (com_requestbuffer[6] << 8) + (com_requestbuffer[7] << 0);

				wavbuffer_count = (address + length) / WAV_BUFFER_SIZE;
				memcpy((uint8_t *)&audioAndHotspotDataBuffer.rawBuffer[address], (uint8_t *)&com_requestbuffer[8], length);
				ok = true;
			}
			break;
		case CPS_ACCESS_RADIO_INFO:
			break;
	}

	if (ok)
	{
		usbComSendBuf[0] = com_requestbuffer[0];
		usbComSendBuf[1] = com_requestbuffer[1];
		USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, usbComSendBuf, 2);
	}
	else
	{
		sector = -1;
		usbComSendBuf[0] = '-';
		USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, usbComSendBuf, 1);
	}

	watchdogRun(true);
}

#if defined(LOOKUP_ENABLED)
static void cpsHandleLookup(void)
{
	bool ok = false;
	uint32_t ID = 0;
	dmrIdDataStruct_t ctx;
	char buf[80];

	ID = atoi((char *)&com_requestbuffer[1]);

	if (dmrIDLookup(ID, &ctx))
	{
		snprintf(buf, sizeof(buf), "L%d %s\n", ID, ctx.text);
		memcpy(usbComSendBuf, buf, strlen(buf));
		ok = true;
	}

	if (ok)
	{
		USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, usbComSendBuf, strlen(buf));
	}
	else
	{
		usbComSendBuf[0] = '-';
		usbComSendBuf[1] = '\n';
		usbComSendBuf[2] = 0;
		USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, usbComSendBuf, 3);
	}
}
#endif

static void cpsHandleCommand(void)
{
	int command = com_requestbuffer[1];
	switch(command)
	{
		case 0:
			// Show CPS screen
			menuSystemPushNewMenu(UI_CPS);
			break;
		case 1:
			// Clear CPS screen
			uiCPSUpdate(CPS2UI_COMMAND_CLEARBUF, 0, 0, FONT_SIZE_1, TEXT_ALIGN_LEFT, 0, NULL);
			break;
		case 2:
			// Write a line of text to CPS screen
			uiCPSUpdate(CPS2UI_COMMAND_PRINT, com_requestbuffer[2], com_requestbuffer[3], (ucFont_t)com_requestbuffer[4], (ucTextAlign_t)com_requestbuffer[5], com_requestbuffer[6], (char *)&com_requestbuffer[7]);
			break;
		case 3:
			// Render CPS screen
			uiCPSUpdate(CPS2UI_COMMAND_RENDER_DISPLAY, 0, 0, FONT_SIZE_1, TEXT_ALIGN_LEFT, 0, NULL);
			break;
		case 4:
			// Turn on the display backlight
			uiCPSUpdate(CPS2UI_COMMAND_BACKLIGHT, 0, 0, FONT_SIZE_1, TEXT_ALIGN_LEFT, 0, NULL);
			break;
		case 5:
			// Close
			if (flashingDMRIDs)
			{
				dmrIDCacheInit();
				flashingDMRIDs = false;
			}
			isCompressingAMBE = false;
			rxPowerSavingSetLevel(nonVolatileSettings.ecoLevel);
			uiCPSUpdate(CPS2UI_COMMAND_END, 0, 0, FONT_SIZE_1, TEXT_ALIGN_LEFT, 0, NULL);
			break;
		case 6:
			{
				int subCommand = com_requestbuffer[2];
				uint32_t m = ticksGetMillis();

				// Do some other processing
				switch(subCommand)
				{
					case 0:
						// Channels has be rewritten, switch currentZone to All Channels
						if (channelsRewritten)
						{
							uint16_t firstContact = 1;

							//
							// Give it a bit of time before reading the zone count as DM-1801 EEPROM looks slower
							// than GD-77 to write
							m = ticksGetMillis();
							while (1U)
							{
								if ((ticksGetMillis() - m) > 50)
								{
									break;
								}
							}

							codeplugAllChannelsInitCache(); // Rebuild channels cache
							nonVolatileSettings.currentZone = (int16_t) (codeplugZonesGetCount() - 1); // Set to All Channels zone

							// Search for the first assigned contact
							for (uint16_t i = CODEPLUG_CHANNELS_MIN; i <= CODEPLUG_CHANNELS_MAX; i++)
							{
								if (codeplugAllChannelsIndexIsInUse(i))
								{
									firstContact = i;
									break;
								}

								// Call tick_watchdog() ??
							}

							nonVolatileSettings.currentChannelIndexInAllZone = firstContact;
							nonVolatileSettings.currentChannelIndexInZone = 0;
						}

						// save current settings and reboot
						m = ticksGetMillis();
						settingsSaveSettings(false);// Need to save these channels prior to reboot, as reboot does not save

						// Give it a bit of time before pulling the plug as DM-1801 EEPROM looks slower
						// than GD-77 to write, then quickly power cycling triggers settings reset.
						while (1U)
						{
							if ((ticksGetMillis() - m) > 50)
							{
								break;
							}
						}
						watchdogReboot();
					break;
					case 1:
						watchdogReboot();
						break;
					case 2:
						// Save settings VFO's to codeplug
						settingsSaveSettings(true);
						break;
					case 3:
						// flash green LED
						uiCPSUpdate(CPS2UI_COMMAND_GREEN_LED, 0, 0, FONT_SIZE_1, TEXT_ALIGN_LEFT, 0, NULL);
						break;
					case 4:
						// flash red LED
						uiCPSUpdate(CPS2UI_COMMAND_RED_LED, 0, 0, FONT_SIZE_1, TEXT_ALIGN_LEFT, 0, NULL);
						break;
					case 5:
						rxPowerSavingSetLevel(0);
						isCompressingAMBE = true;
						codecInitInternalBuffers();
						break;
					case 6:
						soundInit();// Resets the sound buffers
						memset((uint8_t *)&audioAndHotspotDataBuffer.rawBuffer[0], 0, (6 * WAV_BUFFER_SIZE));// clear 1 dmr frame size of wav buffer memory
						break;
					case 7:
						memcpy(&uiDataGlobal.dateTimeSecs, (uint8_t *)&com_requestbuffer[3], sizeof(uint32_t));// update date with data from the CPS
						menuSatelliteScreenClearPredictions(true);
						break;
					// 8:
					// 9:
					case 10: // wait 10ms
						m = ticksGetMillis();
						while (1U)
						{
							if ((ticksGetMillis() - m) > 10)
							{
								break;
							}
						}
						break;
					default:
						break;
				}
			}
			break;
		default:
			break;
	}
	// Send something generic back.
	// Probably need to send a response code in the future
	usbComSendBuf[0] = '-';
	USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, usbComSendBuf, 1);
}

static void handleCPSRequest(void)
{
	//Handle read
	switch(com_requestbuffer[0])
	{
		case 'R':
			cpsHandleReadCommand();
			break;
		case 'W':
			cpsHandleWriteCommand();
			break;
		case 'C':
			cpsHandleCommand();
			break;
#if defined(LOOKUP_ENABLED)
		case 'L':
			cpsHandleLookup();
			break;
#endif
		default:
			usbComSendBuf[0] = '-';
			USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, usbComSendBuf, 1);
			break;
	}
}

#if 0
__attribute__((section(".data.$RAM2"))) volatile uint8_t com_buffer[COM_BUFFER_SIZE];
int com_buffer_write_idx = 0;
int com_buffer_read_idx = 0;
volatile int com_buffer_cnt = 0;


void send_packet(uint8_t val_0x82, uint8_t val_0x86, int ram)
{
	taskENTER_CRITICAL();
	if ((HR_C6000_datalogging) && ((com_buffer_cnt+8+(ram+1))<=COM_BUFFER_SIZE))
	{
		add_to_commbuffer((com_buffer_cnt >> 8) & 0xff);
		add_to_commbuffer((com_buffer_cnt >> 0) & 0xff);
		add_to_commbuffer(val_0x82);
		add_to_commbuffer(val_0x86);
		add_to_commbuffer(tmp_val_0x51);
		add_to_commbuffer(tmp_val_0x52);
		add_to_commbuffer(tmp_val_0x57);
		add_to_commbuffer(tmp_val_0x5f);
		for (int i=0;i<=ram;i++)
		{
			add_to_commbuffer(DMR_frame_buffer[i]);
		}
	}
	taskEXIT_CRITICAL();
}

uint8_t tmp_ram1[256];
uint8_t tmp_ram2[256];

void send_packet_big(uint8_t val_0x82, uint8_t val_0x86, int ram1, int ram2)
{
	taskENTER_CRITICAL();
	if ((HR_C6000_datalogging) && ((com_buffer_cnt+8+(ram1+1)+(ram2+1))<=COM_BUFFER_SIZE))
	{
		add_to_commbuffer((com_buffer_cnt >> 8) & 0xff);
		add_to_commbuffer((com_buffer_cnt >> 0) & 0xff);
		add_to_commbuffer(val_0x82);
		add_to_commbuffer(val_0x86);
		add_to_commbuffer(tmp_val_0x51);
		add_to_commbuffer(tmp_val_0x52);
		add_to_commbuffer(tmp_val_0x57);
		add_to_commbuffer(tmp_val_0x5f);
		for (int i=0;i<=ram1;i++)
		{
			add_to_commbuffer(tmp_ram1[i]);
		}
		for (int i=0;i<=ram2;i++)
		{
			add_to_commbuffer(tmp_ram2[i]);
		}
	}
	taskEXIT_CRITICAL();
}

void add_to_commbuffer(uint8_t value)
{
	com_buffer[com_buffer_write_idx]=value;
	com_buffer_cnt++;
	com_buffer_write_idx++;
	if (com_buffer_write_idx==COM_BUFFER_SIZE)
	{
		com_buffer_write_idx=0;
	}
}
#endif

void USB_DEBUG_PRINT(char *str)
{
	strncpy((char *)usbComSendBuf, str, COM_BUFFER_SIZE);
	usbComSendBuf[COM_BUFFER_SIZE - 1] = 0; // SAFETY: strncpy won't NULL terminate the buffer if length was exceeding.

	USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, usbComSendBuf, strlen((char *)usbComSendBuf));
}

void USB_DEBUG_printf(const char *format, ...)
{
	  char buf[COM_BUFFER_SIZE];
	  va_list params;

	  va_start(params, format);
	  vsnprintf(buf, (sizeof(buf) - 2), format, params);
	  strcat(buf, "\n");
	  va_end(params);
	  USB_DEBUG_PRINT(buf);
}
