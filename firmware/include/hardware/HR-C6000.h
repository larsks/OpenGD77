/*
 * Copyright (C) 2019      Kai Ludwig, DG4KLU
 * Copyright (C) 2020-2021 Roger Clark, VK3KYY / G4KYF
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

#ifndef _OPENGD77_HR_C6000_H_
#define _OPENGD77_HR_C6000_H_

#include <FreeRTOS.h>
#include <task.h>

#include "io/keyboard.h"

#include "interfaces/hr-c6000_spi.h"
#include "usb/usb_com.h"

#include "dmr_codec/codec.h"


#define DMR_FRAME_BUFFER_SIZE 64

extern const uint8_t TG_CALL_FLAG;
extern const uint8_t PC_CALL_FLAG;
extern volatile int slot_state;
extern volatile uint8_t DMR_frame_buffer[DMR_FRAME_BUFFER_SIZE];
extern volatile bool updateLastHeard;
extern volatile int dmrMonitorCapturedTS;
extern char talkAliasText[33];
extern volatile uint32_t readDMRRSSI;

enum DMR_SLOT_STATE { DMR_STATE_IDLE, DMR_STATE_RX_1, DMR_STATE_RX_2, DMR_STATE_RX_END,
					  DMR_STATE_TX_START_1, DMR_STATE_TX_START_2, DMR_STATE_TX_START_3, DMR_STATE_TX_START_4, DMR_STATE_TX_START_5,
					  DMR_STATE_TX_1, DMR_STATE_TX_2, DMR_STATE_TX_END_1, DMR_STATE_TX_END_2, DMR_STATE_TX_END_3_RMO, DMR_STATE_TX_END_3_DMO,
					  DMR_STATE_REPEATER_WAKE_1, DMR_STATE_REPEATER_WAKE_2, DMR_STATE_REPEATER_WAKE_3,
					  DMR_STATE_REPEATER_WAKE_FAIL_1, DMR_STATE_REPEATER_WAKE_FAIL_2 };

enum WakingMode { WAKING_MODE_NONE, WAKING_MODE_WAITING, WAKING_MODE_FAILED };

enum DMR_Embedded_Data
{
	DMR_EMBEDDED_DATA_GROUP               = 0U,
	DMR_EMBEDDED_DATA_USER_USER           = 3U,
	DMR_EMBEDDED_DATA_TALKER_ALIAS_HEADER = 4U,
	DMR_EMBEDDED_DATA_TALKER_ALIAS_BLOCK1 = 5U,
	DMR_EMBEDDED_DATA_TALKER_ALIAS_BLOCK2 = 6U,
	DMR_EMBEDDED_DATA_TALKER_ALIAS_BLOCK3 = 7U,
	DMR_EMBEDDED_DATA_GPS_INFO            = 8U
};

void HRC6000_init(void);
void PORTC_IRQHandler(void);
void init_HR_C6000_interrupts(void);
void init_digital_state(void);
void init_digital_DMR_RX(void);
void reset_timeslot_detection(void);
void init_digital(void);
void terminate_digital(void);
void init_hrc6000_task(void);
void fw_hrc6000_task(void *data);
void tick_HR_C6000(void);

void clearIsWakingState(void);
int getIsWakingState(void);
void clearActiveDMRID(void);
void setMicGainDMR(uint8_t gain);
bool checkTalkGroupFilter(void);

int HRC6000GetReceivedTgOrPcId(void);
int HRC6000GetReceivedSrcId(void);
void HRC6000ClearTimecodeSynchronisation(void);
void HRC6000SetCCFilterMode(bool enable);

#endif /* _OPENGD77_HR_C6000_H_ */
