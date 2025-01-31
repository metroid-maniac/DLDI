/*
	M3DS DLDI based on r4tf_v2

	Copyright (c) 2010-2014 Taiju Yamada

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this
	list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <nds/ndstypes.h>

#include "common.h"

#include <stdio.h> //NULL

#define cardWriteCommand() (REG_AUXSPICNTH = CARD_CR1_ENABLE | CARD_CR1_IRQ)

static inline void cardWaitReady(u32 flags){
	bool ready = false;
	do{
		cardWriteCommand();
		REG_ROMCTRL = flags;
		do{
			if(REG_ROMCTRL & CARD_DATA_READY)
				if(!REG_CARD_DATA_RD)ready = true;
		}while(REG_ROMCTRL & CARD_BUSY);
	}while(!ready);
}

static inline void _cardPolledTransfer(u32 flags, u32 *destination, u32 length){
	u32 data;
	cardWriteCommand();
	REG_ROMCTRL = flags;
	u32 *target = destination + length;
	do{
		// Read data if available
		if(REG_ROMCTRL & CARD_DATA_READY){
			data=REG_CARD_DATA_RD;
			if(destination < target)
				*destination++ = data;
		}
	}while(REG_ROMCTRL & CARD_BUSY);
}

static inline void bytecardPolledTransfer(u32 flags, u32 *destination, u32 length){
	u32 data;
	cardWriteCommand();
	REG_ROMCTRL = flags;
	u32 *target = destination + length;
	do{
		// Read data if available
		if(REG_ROMCTRL & CARD_DATA_READY){
			data=REG_CARD_DATA_RD;
			if(destination < target){
				((u8*)destination)[0] = data & 0xff;
				((u8*)destination)[1] = (data >> 8) & 0xff;
				((u8*)destination)[2] = (data >> 16) & 0xff;
				((u8*)destination)[3] = (data >> 24) & 0xff;
				destination++;
			}
		}
	}while(REG_ROMCTRL & CARD_BUSY);
}

static inline void LogicCardRead(u32 address, u32 *destination, u32 length){
	REG_CARD_COMMAND[0] = 0xbd;
	REG_CARD_COMMAND[1] = (address >> 24) & 0xff;
	REG_CARD_COMMAND[2] = (address >> 16) & 0xff;
	REG_CARD_COMMAND[3] = (address >> 8)  & 0xff;
	REG_CARD_COMMAND[4] =  address        & 0xff;
	REG_CARD_COMMAND[5] = 0;
	REG_CARD_COMMAND[6] = 0;
	REG_CARD_COMMAND[7] = 0;

	cardWaitReady(0xa7586000);
	REG_CARD_COMMAND[0] = 0xba;
	if((u32)destination & 0x03)
		bytecardPolledTransfer(0xa1586000, destination, length);
	else
		_cardPolledTransfer(0xa1586000, destination, length);
}

static inline u32 ReadCardInfo(){
	u32 ret;

	REG_CARD_COMMAND[0] = 0xb0;
	REG_CARD_COMMAND[1] = 0;
	REG_CARD_COMMAND[2] = 0;
	REG_CARD_COMMAND[3] = 0;
	REG_CARD_COMMAND[4] = 0;
	REG_CARD_COMMAND[5] = 0;
	REG_CARD_COMMAND[6] = 0;
	REG_CARD_COMMAND[7] = 0;
	_cardPolledTransfer(0xa7586000, &ret, 1);
	return ret;
}

static inline void LogicCardWrite(u32 address, u32 *source, u32 length){
	u32 data = 0;

	REG_CARD_COMMAND[0] = 0xbe;
	REG_CARD_COMMAND[1] = (address >> 24) & 0xff;
	REG_CARD_COMMAND[2] = (address >> 16) & 0xff;
	REG_CARD_COMMAND[3] = (address >> 8)  & 0xff;
	REG_CARD_COMMAND[4] =  address        & 0xff;
	REG_CARD_COMMAND[5] = 0;
	REG_CARD_COMMAND[6] = 0;
	REG_CARD_COMMAND[7] = 0;
	cardWriteCommand();
	REG_ROMCTRL = 0xe1586100;
	// Write data if ready
	u32 *target = source + length;
	if((u32)source & 0x03){
		do{
			if(REG_ROMCTRL & CARD_DATA_READY){
				if(source < target){
					data = ((u8*)source)[0] | (((u8*)source)[1] << 8) | (((u8*)source)[2] << 16) | (((u8*)source)[3] << 24);
					source++;
				}
				REG_CARD_DATA_RD = data;
			}
		}while(REG_ROMCTRL & CARD_BUSY);
	}else{
		do{
			if(REG_ROMCTRL & CARD_DATA_READY){
				if(source < target)
					data = *source++;
				REG_CARD_DATA_RD = data;
			}
		}while(REG_ROMCTRL & CARD_BUSY);
	}
	REG_CARD_COMMAND[0] = 0xbc;
	cardWaitReady(0xa7586000);
}

// DLDI frontend
bool startup(){
	return ((ReadCardInfo() & 0x07) == 0x04);
}

// Stub
bool clearStatus(){
	return true;
}

__attribute__((alias("clearStatus"))) bool shutdown();

/*
bool shutdown(){
	return true;
}
*/

__attribute__((alias("startup"))) bool isInserted();

/*
bool isInserted(){
	return ((ReadCardInfo() & 0x07) == 0x04);
}
*/

// Actual disk access
bool readSectors(u32 sector, u32 numSecs, void* buffer){
	u32 *u32_buffer = (u32*)buffer, i;

	for (i = 0; i < numSecs; i++) {
		LogicCardRead(sector, u32_buffer, 128);
		sector++;
		u32_buffer += 128;
	}
	return true;
}

bool writeSectors(u32 sector, u32 numSecs, void* buffer){
	u32 *u32_buffer = (u32*)buffer, i;

	for (i = 0; i < numSecs; i++) {
		LogicCardWrite(sector, u32_buffer, 128);
		sector++;
		u32_buffer += 128;
	}
	return true;
}
