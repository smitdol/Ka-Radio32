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
#include "rdsdecoder.h" 

unsigned short pRDSA;
 
void seekUp()
{
	RDA5807M_seek(true, false);
	initRds();	
}
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
 float getFrequency()
 {
 		unsigned long freq;
		RDA5807M_getFreq(&freq);	
		float temp = (freq)/1000.0;//MHz 		
		return temp;
}
 
void rda5807Task(void *pvParams)
{
	RDA5807M_BOOL pFlag;
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
	RDA5807M_setFreq(101700);
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
			RDA5807M_getBLERB(&pBLERB);
//todo: create overload RDA5807M_getBLER(&pBLERA,&pBLERB);
			if ((pBLERA >= 1) || (pBLERB >= 1)) {
				/*printf("BLE: %x  %x\n",pBLERA,pBLERB);*/
				continue;
			}
			RDA5807M_getRDSA(&pRDSA); //RDA5807M_REG_ADDR_0C = 0x0c
			xSemaphoreTake(semI2C, portMAX_DELAY); // get I2C semaphore
			RDA5807M_readRegOnly(pBlock, 3) ; //makes use of auto incrementing address counter
			pBlock[3]=pBlock[2];
			pBlock[2]=pBlock[1];
			pBlock[1]=pBlock[0];
			pBlock[0]=pRDSA;
			xSemaphoreGive(semI2C); // release I2C semaphore

			processData(pBlock);
		}

	}
	vTaskDelete( NULL ); 
}

void initRds(){
    rdsdecoder_reset();
}

void processData(unsigned short* block)
{
  if (block[0] == 0) {
	rdsdecoder_reset();
	return;
  } 

  //gnuradio
  rdsdecoder_parse(block);
  for (int i=0; i < 6; i++ ){
    if (newmessage[i]){
	   //kprintf(message[i]);
       newmessage[i]=false;
       memset(message[i], ' ', sizeof(message[i]));
    }
  }
}
