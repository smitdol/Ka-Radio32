/*
 * rda5807Task.c
 *
 *  Created on: 13.12.2017
 * 
 *		jp Cocatrix
 * Copyright (c) 2017, jp Cocatrix
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "rda5807Task.h"
#include "interface.h"
 
  // ----- actual RDS values
  uint8_t rdsGroupType;
  uint16_t rdsTP, rdsPTY;
  uint8_t _textAB, _last_textAB, _lastTextIDX;

  // Program Service Name
  char _PSName1[10]; // including trailing '\00' character.
  char _PSName2[10]; // including trailing '\00' character.
  char programServiceName[10];    // found station name or empty. Is max. 8 character long.

  uint16_t _lastRDSMinutes; ///< last RDS time send to callback.

  char _RDSText[64 + 2];
 
unsigned short pRDSA;
unsigned short pRDSB;
unsigned short pRDSC;
unsigned short pRDSD;
 
 // some usefull functions
 //-----------------------
 // seek up
 void seekUp()
 {
	RDA5807M_seek(true, false);
	initRds();	
}
 // seek down
 void seekDown()
{
	RDA5807M_seek(false, false);
	initRds();	
 }
 // Wait for seek complete. false if no station found
 bool seekingComplete()
 {
 	RDA5807M_BOOL res;
	vTaskDelay(1);
	do{
		RDA5807M_isSeekingFailed(&res);
		if (res) return false;
		RDA5807M_isSeekingComplete(&res);		
	} while (!res);
	return true;
 }
 // get frequency
 float getFrequency()
 {
 		unsigned long freq;
		RDA5807M_getFreq(&freq);	
		float temp = (freq)/1000.0; // Calculate freq float in MHz		
		return temp;
}
 
////////////////////////////////
// main task
void rda5807Task(void *pvParams)
{
	RDA5807M_BOOL pFlag;
 // init the FM radio
	RDA5807M_SETTING rxSetting={
	.clkSetting={
		.isClkNoCalb=RDA5807M_FALSE,
		.isClkDirInp=RDA5807M_FALSE,
		.freq=RDA5807M_CLK_FREQ_32_768KHZ,
	},
	.useRDS=RDA5807M_TRUE,
	.useNewDMMethod=RDA5807M_TRUE,
	.isDECNST50us=RDA5807M_FALSE,
	.system={
		.band=RDA5807M_BAND_87_108_MHZ,
		.is6576Sys=RDA5807M_FALSE,
		.space=RDA5807M_SPACE_100_KHZ
	}
	};

	RDA5807M_init(&rxSetting);
	RDA5807M_setFreq(89900);
	RDA5807M_enableOutput(RDA5807M_TRUE);
	RDA5807M_setVolume(15);
	RDA5807M_unmute(RDA5807M_TRUE); 
 	
	while (1)
	{
		vTaskDelay(10);
		RDA5807M_isRDSReady(&pFlag);
		if (pFlag == RDA5807M_TRUE)
		{
			unsigned char pBLERA;
			unsigned char pBLERB;
			unsigned short pBlock[4];
			RDA5807M_getBLERA(&pBLERA);
			RDA5807M_getBLERB(&pBLERB); //todo: create overload RDA5807M_getBLER(&pBLERA,&pBLERB);
			if ((pBLERA >= 1) || (pBLERB >= 1)) {
				/*printf("BLE: %x  %x\n",pBLERA,pBLERB);*/
				continue;
			}
			RDA5807M_getRDSA(&pRDSA); //RDA5807M_REG_ADDR_0C = 0x0c
			xSemaphoreTake(semI2C, portMAX_DELAY); // get I2C semaphore
			RDA5807M_readRegOnly( pBlock,3) ; //makes use of auto incrementing address counter
			pRDSB = pBlock[0];
			pRDSC = pBlock[1];
			pRDSD = pBlock[2];
			xSemaphoreGive(semI2C); // release I2C semaphore

			processData(pRDSA, pRDSB, pRDSC, pRDSD);
		}

	}
	vTaskDelete( NULL ); 
}

void initRds(){
    // reset all the RDS info.
    strcpy(_PSName1, "--------");
    strcpy(_PSName2, _PSName1);
    strcpy(programServiceName, "        ");
    memset(_RDSText, 0, sizeof(_RDSText));
    _lastTextIDX = 0;
}

void processData(unsigned short block1, unsigned short block2, unsigned short block3, unsigned short block4)
{
  // DEBUG_FUNC0("process");
  unsigned char  idx; // index of rdsText
  char c1, c2;
//  char *p;

//  unsigned short mins; ///< RDS time in minutes
//  unsigned char off;   ///< RDS time offset and sign

  // Serial.print('('); Serial.print(block1, HEX); Serial.print(' '); Serial.print(block2, HEX); Serial.print(' '); Serial.print(block3, HEX); Serial.print(' '); Serial.println(block4, HEX);

  if (block1 == 0) {
    // reset all the RDS info.
	rdsdecoder_reset();
// Send out empty data
	printf("programServiceName: %s\n",programServiceName);
	printf("_RDSText: %s\n",_RDSText);
//    if (_sendServiceName) _sendServiceName(programServiceName);
//    if (_sendText)        _sendText("");
    return;
  } // if

  unsigned int group[4];
  group[0] = block1;
  group[1] = block2;
  group[2] = block3;
  group[3] = block4;
  
  //gnuradio
  rdsdecoder_parse(group);
  kprintf("##FM.NAME#: %s\n",programServiceName);
  for (int i=0; i < 6; i++ ){
		kprintf(message[i]);
  }


} // processData()
 