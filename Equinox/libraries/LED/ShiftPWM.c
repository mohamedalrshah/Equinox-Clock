/* Copyright (c) 2011 Jamie Clarke - jamie.clarke.jc@gmail.com       */
/* All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#include "ShiftPWM.h"
#include "pinout.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_nvic.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_timer.h"
#include "hsv2rgb.h"
#include "comm.h"
#include "sys_timer.h"
//#include "rtc.h"	not needed if calling c++ functions - links ok without

#define DMA
//#define RxDMA

//#define MAX_BAM_BITS 16
#define MAX_BAM_BITS 8		// number of BITORDER bits to cycle through
#define SSP_SPEED 30000000
#define DelayLatchIn 350*(30000000/SSP_SPEED)	// delay before chip select is toggled
uint32_t volatile ticks = 0;
uint32_t volatile ticks_at_DMA_start = 0;
uint32_t volatile ticks_after_DMA_finish = 0;
uint32_t volatile ticks_at_LE_start = 0;
uint32_t volatile ticks_at_LE_finish = 0;
uint32_t volatile ticks_at_OE_start = 0;

const uint8_t BITORDER[] = { 0,1,2,3,4,5,6,7,7,6,5,4,3,2,1,0 };		// which of the 8 bits to send
//const uint8_t BITORDER[] = { 0,7,2,5,4,3,6,1,1,6,3,4,5,2,7,0 };
//const uint8_t BITORDER[] = { 5,3,1,7,2,4,6,0,5,3,1,7,2,4,6 };

//#define START_TIME 16384	// too high
//#define START_TIME 8192	// slight flicker
#define START_TIME 4096
//#define START_TIME 2048	// too low

//#define START_TIME 32 //smallest time inteval 33us
//#define START_TIME 32/2 //fastest possible
//#define START_TIME 32*1000 //for uart

volatile uint8_t SENDSEQ;
volatile uint32_t DELAY_TIME; 		//so RIT ms is not set to 0
volatile uint32_t NEXT_DELAY_TIME; 	//so RIT ms is not set to 0
volatile uint8_t SEND_BIT;			// which of the 8 bits to send
volatile uint8_t NEXT_SEND_BIT;
//volatile uint32_t LED_SEND;

const uint32_t BITTIME[] = {
		START_TIME/128, //Bit 0 time (LSB)
		START_TIME/64, //Bit 1 time
		START_TIME/32, //Bit 2 time
		START_TIME/16, //Bit 3 time
		START_TIME/8, //Bit 4 time
		START_TIME/4, //Bit 5 time
		START_TIME/2, //Bit 6 time
		START_TIME //Bit 7 time (MSB)
};

// for single board led 30 is led 1
#define REGS 12
#define RGBS 60
#define LEDS RGBS*3
#define LEDS16 REGS*16
#define BITS 8
uint16_t SEQ_BIT[16];
uint32_t SEQ_TIME[16];

#define BUFFERS 1									// Number of buffers
volatile uint8_t LED_RAW[LEDS];						// 8bit brightness
volatile uint16_t LED_PRECALC[REGS][BITS][BUFFERS];	// 16bit value sent to LED drivers with high bit staying on for longest

GPDMA_LLI_Type LinkerList[REGS][BITS][BUFFERS];
GPDMA_Channel_CFG_Type GPDMACfg;

volatile uint32_t BufferNo;
//volatile uint32_t UPDATE_COUNT;
//volatile uint32_t LED_UPDATE_REQUIRED;

volatile uint8_t LED_PATTERN;
volatile uint8_t LED_SPEED;

#define MAX_PATTERNS 3
#define MAX_PATTERNS_LETTERS 10 // (inc \0)
const char LED_PATTERN_NAME[MAX_PATTERNS][MAX_PATTERNS_LETTERS] = {
	"Clock",
	"Test",
	"Name3"
};


extern volatile uint32_t LED_UPDATE_REQUIRED;
extern volatile uint32_t LED_SEND;
extern volatile uint32_t USER_MILLIS;
extern volatile uint32_t SEC_MILLIS;

void TIMER0_IRQHandler(void){
//	xprintf("TIMER0_IRQ");
	if (TIM_GetIntStatus(LPC_TIM0,TIM_MR0_INT)){
#if 0
		if(TOG[0])
//			FIO_SetValue(LED_LE_PORT, LED_LE_BIT);
			GPIO_SetValue(LED_4_PORT, LED_4_BIT);
		else
//			FIO_ClearValue(LED_LE_PORT, LED_LE_BIT);
			GPIO_ClearValue(LED_4_PORT, LED_4_BIT);
		TOG[0]=!TOG[0];
//		TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);
//		return;
#endif
//		xprintf(INFO "RIT N=%d B=%x NXT_T=%6d TX=%03b\n",SENDSEQ,SEND_BIT,DELAY_TIME,LED_PRECALC[0][SEND_BIT]);

		//Set bit/time
		DELAY_TIME=NEXT_DELAY_TIME;
		SEND_BIT=NEXT_SEND_BIT;

		//Retart sequence if required
		SENDSEQ+=1;
		SENDSEQ>=MAX_BAM_BITS ? SENDSEQ=0 : 0;

		//Setup new timing for next Timer
		NEXT_DELAY_TIME=SEQ_TIME[SENDSEQ];
		NEXT_SEND_BIT=SEQ_BIT[SENDSEQ];

		TIM_UpdateMatchValue(LPC_TIM0,0,NEXT_DELAY_TIME);
		FIO_SetValue(LED_OE_PORT, LED_OE_BIT);
#ifdef DMA
		GPDMACfg.DMALLI = (uint32_t) &LinkerList[0][SEND_BIT][BufferNo];
		GPDMA_Setup(&GPDMACfg);
		GPDMA_ChannelCmd(0, ENABLE);
#ifdef RxDMA
		GPDMA_ChannelCmd(1, ENABLE);
#endif
#else
		uint8_t reg;
		for(reg=6; 0<reg;reg--){
			xprintf("%d ",reg-1);
#if 0
			if(BUFFER==1)
				SSP_SendData(LED_SPI_CHN, LED_PRECALC1[reg][SEND_BIT]);
			else
				SSP_SendData(LED_SPI_CHN, LED_PRECALC2[reg][SEND_BIT]);
#endif
			//WaitForSend();//Wait if TX buffer full
			//while(LED_SPI_CHN->SR & SSP_STAT_BUSY);
			while(SSP_GetStatus(LED_SPI_CHN, SSP_STAT_BUSY)){
			};
			SSP_SendData(LED_SPI_CHN, LED_PRECALC[reg-1][SEND_BIT]);
			xprintf("%4x ",(LED_PRECALC[reg-1][SEND_BIT]));
		}
		for(reg=12; reg>6;reg--){
			xprintf("%d ",reg-1);
#if 0
			if(BUFFER==1)
				SSP_SendData(LED_SPI_CHN, LED_PRECALC1[reg][SEND_BIT]);
			else
				SSP_SendData(LED_SPI_CHN, LED_PRECALC2[reg][SEND_BIT]);
#endif
			//WaitForSend();//Wait if TX buffer full
			while(SSP_GetStatus(LED_SPI_CHN, SSP_STAT_BUSY)){
			}
			SSP_SendData(LED_SPI_CHN, LED_PRECALC[reg-1][SEND_BIT]);
//			if (reg==7){
				xprintf("%4x ",(LED_PRECALC[reg-1][SEND_BIT]));
//			}
		}
		LatchIn();
#endif
/*		UPDATE_COUNT+=1;
		if(UPDATE_COUNT>=(1600/MAX_BAM_BITS)){
			UPDATE_COUNT=0;
			LED_UPDATE_REQUIRED=1;
		}*/
	}
	TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);
}

void TIMER1_IRQHandler(void){
//	xprintf("TIMER1_IRQ");
	if (TIM_GetIntStatus(LPC_TIM1,TIM_MR0_INT)){
		LatchIn();
	}
	TIM_ClearIntPending(LPC_TIM1, TIM_MR0_INT);
}

void TIMER2_IRQHandler(void){
//	xprintf("TIMER2_IRQ");
	if (TIM_GetIntStatus(LPC_TIM2,TIM_MR0_INT)){
		ticks+=1;
	}
	TIM_ClearIntPending(LPC_TIM2, TIM_MR0_INT);
}

inline void LatchIn(void){
#if 0
	for(uint32_t i=0;i<DelayLatchIn;i++){
		asm("nop");
	}
#endif
//	ticks_at_LE_start = ticks;
	FIO_SetValue(LED_LE_PORT, LED_LE_BIT);
#if 0
	for(uint32_t i=0;i<5;i++){
		asm("nop");
	}
#endif
//	ticks_at_LE_finish = ticks;
	FIO_ClearValue(LED_LE_PORT, LED_LE_BIT);
#if 0
	for(uint32_t i=0;i<10;i++){
		asm("nop");
	}
#endif
//	ticks_at_OE_start = ticks;
	FIO_ClearValue(LED_OE_PORT, LED_OE_BIT);
/*	xprintf("SB:%d ",SEND_BIT);
	xprintf("DMS_S:%d ",ticks_at_DMA_start);
	xprintf("DMS_F:%d ",ticks_after_DMA_finish);
	xprintf("DMS_LE_S:%d ",ticks_at_LE_start);
	xprintf("DMS_LE_F:%d ",ticks_at_LE_finish);
	xprintf("DMS_OE_S:%d\n",ticks_at_OE_start);*/
}

inline void WaitForSend(void){
	while(SSP_GetStatus(LED_SPI_CHN,SSP_STAT_BUSY)){
#if 0
		for(uint32_t i=0;i<20;i++){
			asm("mov r0,r0");
		}
#endif
	}
//	while(!SSP_GetStatus(LED_SPI_CHN,SSP_STAT_TXFIFO_EMPTY));
}

void DMA_IRQHandler (void)
{
//	xprintf("DMA_IRQ");
//	ticks_at_DMA_start = ticks;
	// check GPDMA interrupt on channel 0
	if (GPDMA_IntGetStatus(GPDMA_STAT_INT, 0)){
		// Check counter terminal status
		if(GPDMA_IntGetStatus(GPDMA_STAT_INTTC, 0)){
			// Clear terminate counter Interrupt pending
			GPDMA_ClearIntPending (GPDMA_STATCLR_INTTC, 0);
//			ticks_after_DMA_finish = ticks;
			TIM_Cmd(LPC_TIM1,ENABLE);
		}
		// Check error terminal status
		if (GPDMA_IntGetStatus(GPDMA_STAT_INTERR, 0)){
			// Clear error counter Interrupt pending
			GPDMA_ClearIntPending (GPDMA_STATCLR_INTERR, 0);
		}
	}
#ifdef RxDMA
	// check GPDMA interrupt on channel 1
	if (GPDMA_IntGetStatus(GPDMA_STAT_INT, 1)){
		// Check counter terminal status
		if(GPDMA_IntGetStatus(GPDMA_STAT_INTTC, 1)){
			// Clear terminate counter Interrupt pending
			GPDMA_ClearIntPending (GPDMA_STATCLR_INTTC, 1);
				Channel1_TC++;
//				xprintf("ch1_TC:%d\n",Channel0_TC);
		}
		// Check error terminal status
		if (GPDMA_IntGetStatus(GPDMA_STAT_INTERR, 1)){
			// Clear error counter Interrupt pending
			GPDMA_ClearIntPending (GPDMA_STATCLR_INTERR, 1);
			Channel1_Err++;
//			xprintf("ch1_Err:%d\n",Channel0_Err);
		}
	}
	xprintf("Rx:0x%x ",LED_PRECALC1[0][0]);
#endif
}

void LED_init(){
	uint8_t pot = 0x50;
	xprintf(INFO "Pot set to:%d",pot);FFL_();
	setPot(pot);
	uint8_t tmp;

	SENDSEQ=0;
	DELAY_TIME=1; //so RIT ms is not set to 0
	NEXT_DELAY_TIME=1; //so RIT ms is not set to 0
	SEND_BIT=0;
	NEXT_SEND_BIT=0;
	BufferNo = 0;
//	UPDATE_COUNT=0;
//	LED_UPDATE_REQUIRED=0;
//	LED_SEND=0;

	GPIO_SetDir(LED_OE_PORT, LED_OE_BIT, 1);
	GPIO_SetValue(LED_OE_PORT, LED_OE_BIT);//turn off leds active low
	LatchIn();//reset
	GPIO_SetDir(LED_LE_PORT, LED_LE_BIT, 1);
	GPIO_ClearValue(LED_LE_PORT, LED_LE_BIT);

	//reset all arrays
	for (tmp=0;tmp<MAX_BAM_BITS;tmp++){
		SEQ_BIT[tmp] = BITORDER[tmp];
		SEQ_TIME[tmp] = BITTIME[BITORDER[tmp]];
	}
	for(uint8_t l=0; l<LEDS; l++){
		LED_RAW[l]=0;
	}
	for(uint8_t rbuf=0; rbuf<BUFFERS; rbuf++){
		for (uint8_t bit=0;bit<BITS;bit++){
			for(uint8_t r=0; r<REGS; r++){
				LED_PRECALC[r][bit][rbuf]=0;
			}
		}
	}
	calulateLEDMIBAMBits();

	// Initialize SPI pin connect
	PINSEL_CFG_Type PinCfg;
	/* LE1 */
	PinCfg.Funcnum   = PINSEL_FUNC_0;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode   = PINSEL_PINMODE_PULLDOWN;
	PinCfg.Pinnum    = LED_LE_PIN;
	PinCfg.Portnum   = LED_LE_PORT;
	PINSEL_ConfigPin(&PinCfg);
	/* SSEL1 */
	PinCfg.Funcnum   = PINSEL_FUNC_0;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode   = PINSEL_PINMODE_PULLDOWN;
	PinCfg.Pinnum    = LED_OE_PIN;
	PinCfg.Portnum   = LED_OE_PORT;
	PINSEL_ConfigPin(&PinCfg);
	/* SCK1 */
	PinCfg.Funcnum   = PINSEL_FUNC_2;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode   = PINSEL_PINMODE_PULLUP;
	PinCfg.Pinnum    = LED_SCK_PIN;
	PinCfg.Portnum   = LED_SCK_PORT;
	PINSEL_ConfigPin(&PinCfg);
	/* MISO1 */
	PinCfg.Funcnum   = PINSEL_FUNC_2;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode   = PINSEL_PINMODE_PULLUP;
	PinCfg.Pinnum    = LED_MISO_PIN;
	PinCfg.Portnum   = LED_MISO_PORT;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum   = PINSEL_FUNC_2;
	/* MOSI1 */
	PinCfg.Funcnum   = PINSEL_FUNC_2;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode   = PINSEL_PINMODE_PULLUP;
	PinCfg.Pinnum    = LED_MOSI_PIN;
	PinCfg.Portnum   = LED_MOSI_PORT;
	PINSEL_ConfigPin(&PinCfg);

	/* initialize SSP configuration structure */
	SSP_CFG_Type SSP_ConfigStruct;
	SSP_ConfigStruct.CPHA = SSP_CPHA_SECOND;
	SSP_ConfigStruct.CPOL = SSP_CPOL_LO;
	SSP_ConfigStruct.ClockRate = SSP_SPEED; // TLC5927 max freq = 30Mhz
	SSP_ConfigStruct.FrameFormat = SSP_FRAME_SPI;
	SSP_ConfigStruct.Databit = SSP_DATABIT_16;
	SSP_ConfigStruct.Mode = SSP_MASTER_MODE;
	SSP_Init(LED_SPI_CHN, &SSP_ConfigStruct);
	SSP_Cmd(LED_SPI_CHN, ENABLE);	// Enable SSP peripheral

	// Turn all LEDS off
	for (uint8_t i=0;i<REGS;i++){
		while(LED_SPI_CHN->SR & SSP_STAT_BUSY);
		SSP_SendData(LED_SPI_CHN, 0x0000);
	};
	LatchIn();

	xprintf(INFO "LED TIM0_ConfigMatch");FFL_();
	// Setup LED interupt
	TIM_TIMERCFG_Type TIM0_ConfigStruct;
	TIM_MATCHCFG_Type TIM0_MatchConfigStruct;
	TIM0_ConfigStruct.PrescaleOption = TIM_PRESCALE_USVAL;	// Initialize timer 0, prescale count time of 1us //1000000uS = 1S
	TIM0_ConfigStruct.PrescaleValue	= 1;
	TIM0_MatchConfigStruct.MatchChannel = 0;		// use channel 0, MR0
	TIM0_MatchConfigStruct.IntOnMatch   = TRUE;	// Enable interrupt when MR0 matches the value in TC register
	TIM0_MatchConfigStruct.ResetOnMatch = TRUE;	//Enable reset on MR0: TIMER will reset if MR0 matches it
	TIM0_MatchConfigStruct.StopOnMatch  = FALSE;	//Stop on MR0 if MR0 matches it
	TIM0_MatchConfigStruct.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
	TIM0_MatchConfigStruct.MatchValue   = BITTIME[1];		// Set Match value, count value of 1000000 (1000000 * 1uS = 1000000us = 1s --> 1 Hz)
	TIM_Init(LPC_TIM0, TIM_TIMER_MODE,&TIM0_ConfigStruct);	// Set configuration for Tim_config and Tim_MatchConfig
	TIM_ConfigMatch(LPC_TIM0,&TIM0_MatchConfigStruct);
	xprintf(OK "LED TIM0_ConfigMatch");FFL_();
	NVIC_SetPriority(TIMER0_IRQn, 0);
	NVIC_EnableIRQ(TIMER0_IRQn);
//	TIM_Cmd(LPC_TIM0,ENABLE);	// Turned on after Timer 1 is turned on

	// Setup LED Latch interupt
	TIM_TIMERCFG_Type TIM1_ConfigStruct;
	TIM_MATCHCFG_Type TIM1_MatchConfigStruct;
	TIM1_ConfigStruct.PrescaleOption = TIM_PRESCALE_TICKVAL;	// Initialize timer 0, prescale count time of 1us //1000000uS = 1S
	TIM1_ConfigStruct.PrescaleValue	= 1;
	TIM1_MatchConfigStruct.MatchChannel = 0;	// use channel 0, MR0
	TIM1_MatchConfigStruct.IntOnMatch   = TRUE;	// Enable interrupt when MR0 matches the value in TC register
	TIM1_MatchConfigStruct.ResetOnMatch = TRUE;	//Enable reset on MR0: TIMER will reset if MR0 matches it
	TIM1_MatchConfigStruct.StopOnMatch  = TRUE;	//Stop on MR0 if MR0 matches it
	TIM1_MatchConfigStruct.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
	TIM1_MatchConfigStruct.MatchValue   = DelayLatchIn;		// Set Match value, count value of 1000000 (1000000 * 1uS = 1000000us = 1s --> 1 Hz)
	TIM_Init(LPC_TIM1, TIM_TIMER_MODE,&TIM1_ConfigStruct);	// Set configuration for Tim_config and Tim_MatchConfig
	TIM_ConfigMatch(LPC_TIM1,&TIM1_MatchConfigStruct);
	xprintf(OK "LED Latch TIM1_ConfigMatch");FFL_();
	NVIC_SetPriority(TIMER1_IRQn, 0);
	NVIC_EnableIRQ(TIMER1_IRQn);
	TIM_Cmd(LPC_TIM0,ENABLE);	// To start timer 0
//	TIM_Cmd(LPC_TIM1,ENABLE);	// To start timer 1

	// Counter interupt
/*	TIM_TIMERCFG_Type TIM2_ConfigStruct;
	TIM_MATCHCFG_Type TIM2_MatchConfigStruct;
	TIM2_ConfigStruct.PrescaleOption = TIM_PRESCALE_TICKVAL;	// Initialize timer 0, prescale count time of 1us //1000000uS = 1S
	TIM2_ConfigStruct.PrescaleValue	= 1;
	TIM2_MatchConfigStruct.MatchChannel = 0;	// use channel 0, MR0
	TIM2_MatchConfigStruct.IntOnMatch   = TRUE;	// Enable interrupt when MR0 matches the value in TC register
	TIM2_MatchConfigStruct.ResetOnMatch = TRUE;	//Enable reset on MR0: TIMER will reset if MR0 matches it
	TIM2_MatchConfigStruct.StopOnMatch  = FALSE;	//Stop on MR0 if MR0 matches it
	TIM2_MatchConfigStruct.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
	TIM2_MatchConfigStruct.MatchValue   = 50;		// Set Match value, count value of 1000000 (1000000 * 1uS = 1000000us = 1s --> 1 Hz)
	TIM_Init(LPC_TIM2, TIM_TIMER_MODE,&TIM2_ConfigStruct);	// Set configuration for Tim_config and Tim_MatchConfig
	TIM_ConfigMatch(LPC_TIM2,&TIM2_MatchConfigStruct);
	xprintf(OK "LED Latch TIM2_ConfigMatch");FFL_();
	NVIC_SetPriority(TIMER2_IRQn, 0);
	NVIC_EnableIRQ(TIMER2_IRQn);
	TIM_Cmd(LPC_TIM2,ENABLE);	// To start timer 2
	TIM_Cmd(LPC_TIM0,ENABLE);	// To start timer 0*/

#ifdef DMA
//	GPDMA_Channel_CFG_Type GPDMACfg;
	NVIC_SetPriority(DMA_IRQn, 0);	// set according to main.c
	NVIC_EnableIRQ(DMA_IRQn);
	GPDMA_Init();				// Initialize GPDMA controller */
	NVIC_DisableIRQ (DMA_IRQn);	// Disable interrupt for DMA
	NVIC_SetPriority(DMA_IRQn, 0);	// set according to main.c

	uint8_t reg, bit, linkerListNo, buf;
	GPDMACfg.ChannelNum = 0;	// DMA Channel 0
	GPDMACfg.SrcMemAddr = 0;	// Source memory - not used - will be sent in interrupt so independent bit Linker Lists can be chosen
	GPDMACfg.DstMemAddr = 0;	// Destination memory - not used - only used when destination is memory
	GPDMACfg.TransferSize = 1;	// Transfer size
	GPDMACfg.TransferWidth = GPDMA_WIDTH_HALFWORD;	// Transfer width - not used
	GPDMACfg.TransferType = GPDMA_TRANSFERTYPE_M2P;	// Transfer type
	GPDMACfg.SrcConn = 0;		// Source connection - not used
	GPDMACfg.DstConn = GPDMA_CONN_SSP0_Tx;	// Destination connection - not used
	GPDMACfg.DMALLI = (uint32_t) &LinkerList[0][0];	// Linker List Item - Pointer to linker list
	GPDMA_Setup(&GPDMACfg);		// Setup channel with given parameter

	// Linker list 32bit Control
	uint32_t LinkerListControl = 0;
	LinkerListControl = GPDMA_DMACCxControl_TransferSize((uint32_t)GPDMACfg.TransferSize) \
						| GPDMA_DMACCxControl_SBSize((uint32_t)GPDMA_BSIZE_1) \
						| GPDMA_DMACCxControl_DBSize((uint32_t)GPDMA_BSIZE_1) \
						| GPDMA_DMACCxControl_SWidth((uint32_t)GPDMACfg.TransferWidth) \
						| GPDMA_DMACCxControl_DWidth((uint32_t)GPDMACfg.TransferWidth) \
						| GPDMA_DMACCxControl_SI;

	for (buf=0;buf<BUFFERS;buf++){
		for (bit=0;bit<BITS;bit++){
			linkerListNo=0;
			for (reg=5; 0<reg;reg--,linkerListNo++){
//				xprintf("bit:%d reg:%d SrcAddr:0x%x DstAddr:0x%x NextLLI:0x%x linkerListNo:0x%x\n",bit,reg,(uint32_t) &LED_PRECALC[reg][bit],(uint32_t) &LPC_SSP0->DR,(uint32_t) &LinkerList[linkerListNo+1][bit],linkerListNo);
				LinkerList[linkerListNo][bit][buf].SrcAddr = (uint32_t) &LED_PRECALC[reg][bit][buf];	/**< Source Address */
				LinkerList[linkerListNo][bit][buf].DstAddr = (uint32_t) &LPC_SSP0->DR;			/**< Destination address */
				LinkerList[linkerListNo][bit][buf].NextLLI = (uint32_t) &LinkerList[linkerListNo+1][bit][buf];	/**< Next LLI address, otherwise set to '0' */
				LinkerList[linkerListNo][bit][buf].Control = LinkerListControl;
//				xprintf("SrcAddr:0x%x DstAddr:0x%x NextLLI:0x%x\n",(uint32_t) &LinkerList[linkerListNo][bit].SrcAddr,(uint32_t) &LinkerList[linkerListNo][bit].DstAddr,(uint32_t) &LinkerList[linkerListNo][bit].NextLLI,(uint32_t) &LinkerList[linkerListNo][bit].Control);
			}
//			if (reg==0){
//			xprintf("bit:%d reg:%d SrcAddr:0x%x DstAddr:0x%x NextLLI:0x%x linkerListNo:0x%x\n",bit,reg,(uint32_t) &LED_PRECALC[reg][bit],(uint32_t) &LPC_SSP0->DR,(uint32_t) &LinkerList[linkerListNo+1][bit],linkerListNo);
			LinkerList[linkerListNo][bit][buf].SrcAddr = (uint32_t) &LED_PRECALC[reg][bit][buf];	/**< Source Address */
			LinkerList[linkerListNo][bit][buf].DstAddr = (uint32_t) &LPC_SSP0->DR;			/**< Destination address */
			LinkerList[linkerListNo][bit][buf].NextLLI = (uint32_t) &LinkerList[linkerListNo+1][bit][buf];/**< Next LLI address, otherwise set to '0' */
			LinkerList[linkerListNo][bit][buf].Control = LinkerListControl;
			linkerListNo++;
//			xprintf("SrcAddr:0x%x DstAddr:0x%x NextLLI:0x%x\n",(uint32_t) &LinkerList[linkerListNo][bit].SrcAddr,(uint32_t) &LinkerList[linkerListNo][bit].DstAddr,(uint32_t) &LinkerList[linkerListNo][bit].NextLLI,(uint32_t) &LinkerList[linkerListNo][bit].Control);
//			}
			for (reg=11; reg>6;reg--,linkerListNo++){
//				xprintf("bit:%d reg:%d SrcAddr:0x%x DstAddr:0x%x NextLLI:0x%x linkerListNo:0x%x\n",bit,reg,(uint32_t) &LED_PRECALC[reg][bit],(uint32_t) &LPC_SSP0->DR,(uint32_t) &LinkerList[linkerListNo+1][bit],linkerListNo);
				LinkerList[linkerListNo][bit][buf].SrcAddr = (uint32_t) &LED_PRECALC[reg][bit][buf];	/**< Source Address */
				LinkerList[linkerListNo][bit][buf].DstAddr = (uint32_t) &LPC_SSP0->DR;			/**< Destination address */
				LinkerList[linkerListNo][bit][buf].NextLLI = (uint32_t) &LinkerList[linkerListNo+1][bit][buf];	/**< Next LLI address, otherwise set to '0' */
				LinkerList[linkerListNo][bit][buf].Control = LinkerListControl;
//				xprintf("SrcAddr:0x%x DstAddr:0x%x NextLLI:0x%x\n",(uint32_t) &LinkerList[linkerListNo][bit].SrcAddr,(uint32_t) &LinkerList[linkerListNo][bit].DstAddr,(uint32_t) &LinkerList[linkerListNo][bit].NextLLI,(uint32_t) &LinkerList[linkerListNo][bit].Control);
			}
//			if (reg==7){
//			xprintf("bit:%d reg:%d SrcAddr:0x%x DstAddr:0x%x NextLLI:0x%x linkerListNo:0x%x\n",bit,reg,(uint32_t) &LED_PRECALC[reg][bit],(uint32_t) &LPC_SSP0->DR,(uint32_t) &LinkerList[linkerListNo+1][bit],linkerListNo);
			LinkerList[linkerListNo][bit][buf].SrcAddr = (uint32_t) &LED_PRECALC[reg][bit][buf];	/**< Source Address */
			LinkerList[linkerListNo][bit][buf].DstAddr = (uint32_t) &LPC_SSP0->DR;			/**< Destination address */
			LinkerList[linkerListNo][bit][buf].NextLLI = 0;									/**< Next LLI address, otherwise set to '0' */
			LinkerList[linkerListNo][bit][buf].Control = LinkerListControl;
			linkerListNo++;
//			xprintf("SrcAddr:0x%x DstAddr:0x%x NextLLI:0x%x NextLLI_V:0x%x\n",(uint32_t) &LinkerList[linkerListNo][bit].SrcAddr,(uint32_t) &LinkerList[linkerListNo][bit].DstAddr,(uint32_t) &LinkerList[linkerListNo][bit].NextLLI,LinkerList[linkerListNo][bit].NextLLI);
//			}
		}
	}
	SSP_DMACmd (LED_SPI_CHN, SSP_DMA_TX, ENABLE);	// Enable Tx DMA on SSP0
//	GPDMA_ChannelCmd(0, ENABLE);	// Enable GPDMA channel 0
	NVIC_EnableIRQ (DMA_IRQn);		// Enable interrupt for DMA
	xprintf(OK "DMA Setup");FFL_();
#endif
#ifdef RxDMA // SSP Rx DMA
	GPDMA_Channel_CFG_Type GPDMACfg1;
	/* Configure GPDMA channel 1 -------------------------------------------------------------*/
	GPDMACfg1.ChannelNum = 1;	// DMA Channel 0
	GPDMACfg1.SrcMemAddr = 0;	// Source memory - not used - will be sent in interrupt so independent bit Linker Lists can be chosen
	GPDMACfg1.DstMemAddr = (uint32_t) &LED_PRECALC1[0][0];	// Destination memory - not used - only used when destination is memory
	GPDMACfg1.TransferSize = 1;	// Transfer size
	GPDMACfg1.TransferWidth = GPDMA_WIDTH_HALFWORD;	// Transfer width
	GPDMACfg1.TransferType = GPDMA_TRANSFERTYPE_P2M;	// Transfer type
	GPDMACfg1.SrcConn = GPDMA_CONN_SSP0_Rx;		// Source connection - not used
	GPDMACfg1.DstConn = 0;	// Destination connection - not used
	GPDMACfg1.DMALLI = 0;	// Linker List Item - Pointer to linker list
	GPDMA_Setup(&GPDMACfg1);		// Setup channel with given parameter
	Channel1_TC = 0;			// Reset terminal counter
	Channel1_Err = 0;			// Reset Error counter
	xprintf(OK "DMA Rx Setup");FFL_();
//	SSP_DMACmd (LED_SPI_CHN, SSP_DMA_RX, ENABLE);	// Enable Tx DMA on SSP0
//	GPDMA_ChannelCmd(1, ENABLE);	// Enable GPDMA channel 0
#endif
}

void LED_time(){
	resetLeds();
		uint8_t HH = GetHH();
		uint8_t MM = GetMM();
		uint8_t SS = GetSS();
		HH>11 ? HH-=12 : 0;

		// Remove the tails
		SS<5 ? SetLED((SS+60)*3-13,0) : SetLED(SS*3-13,0);
		MM<4 ? SetLED((MM+60)*3-11,0) : SetLED(MM*3-11,0);
		HH<2 ? SetLED((HH+11)*3*5,0) : SetLED((HH-1)*3*5,0);
		HH<2 ? SetLED((HH+11)*3*5-3,0) : SetLED((HH-1)*3*5-3,0);
		HH<2 ? SetLED((HH+11)*3*5-6,0) : SetLED((HH-1)*3*5-6,0);

		// Seconds Blue
//		for (SS=0;SS<60;SS++){
			SetLED(SS*3+2,0xff);
			SS<1 ? SetLED((SS+60)*3-1,0x66/5*4) : SetLED(SS*3-1,0x66/5*4);
			SS<2 ? SetLED((SS+60)*3-4,0x4c/5*3) : SetLED(SS*3-4,0x4c/5*3);
			SS<3 ? SetLED((SS+60)*3-7,0x33/5*2) : SetLED(SS*3-7,0x33/5*2);
			SS<4 ? SetLED((SS+60)*3-10,0x19/5) : SetLED(SS*3-10,0x19/5);
//			calulateLEDMIBAMBits();
//			delay_ms(100);
//			SetLED(SS*3+2,0);
//			SS<1 ? SetLED((SS+60)*3-1,0) : SetLED(SS*3-1,0);
//			SS<2 ? SetLED((SS+60)*3-4,0) : SetLED(SS*3-4,0);
//			SS<3 ? SetLED((SS+60)*3-7,0) : SetLED(SS*3-7,0);
//			SS<4 ? SetLED((SS+60)*3-10,0) : SetLED(SS*3-10,0);
//			calulateLEDMIBAMBits();
//		}
		// Minutes Green
//		for (MM=0;MM<60;MM++){
			SetLED(MM*3+1,0xff);
			MM<1 ? SetLED((MM+60)*3-2,0x66/5*4) : SetLED(MM*3-2,0x66/5*4);
			MM<2 ? SetLED((MM+60)*3-5,0x4c/5*3) : SetLED(MM*3-5,0x4c/5*3);
			MM<3 ? SetLED((MM+60)*3-8,0x33/5*2) : SetLED(MM*3-8,0x33/5*2);
//			MM<4 ? SetLED((MM+60)*3-11,0x19/5) : SetLED(MM*3-11,0x19/5);
//			calulateLEDMIBAMBits();
//			delay_ms(100);
//			SetLED(MM*3+1,0);
//			MM<1 ? SetLED((MM+60)*3-2,0) : SetLED(MM*3-2,0);
//			MM<2 ? SetLED((MM+60)*3-5,0) : SetLED(MM*3-5,0);
//			MM<3 ? SetLED((MM+60)*3-8,0) : SetLED(MM*3-8,0);
//			MM<4 ? SetLED((MM+60)*3-11,0) : SetLED(MM*3-11,0);
//			calulateLEDMIBAMBits();
//		}
		//Hours red
//		for (HH=0;HH<12;HH++){
			SetLED(HH*3*5,0xff);
			HH<1 ? SetLED((HH+12)*3*5-3,0x66/5*4) : SetLED(HH*3*5-3,0x66/5*4);
			HH<1 ? SetLED((HH+12)*3*5-6,0x4c/5*3) : SetLED(HH*3*5-6,0x4c/5*3);
//			HH<1 ? SetLED((HH+12)*3*5-9,0x33/5*2) : SetLED(HH*3*5-9,0x33/5*2);
//			HH<1 ? SetLED((HH+12)*3*5-12,0x19/5) : SetLED(HH*3*5-12,0x19/5);
//			calulateLEDMIBAMBits();
//			delay_ms(1000);
//			SetLED(HH*3*5,0);
//			HH<1 ? SetLED((HH+12)*3*5-3,0) : SetLED(HH*3*5-3,0);
//			HH<1 ? SetLED((HH+12)*3*5-6,0) : SetLED(HH*3*5-6,0);
//			HH<1 ? SetLED((HH+12)*3*5-9,0) : SetLED(HH*3*5-9,0);
//			HH<1 ? SetLED((HH+12)*3*5-12,0) : SetLED(HH*3*5-12,0);
//		}
//	}
	calulateLEDMIBAMBits();
}

void LED_test(){
#if 0 // Turn on then off every LED
	for(uint8_t led=0; led<LEDS;led++){
		SetLED(led,0xff);
		calulateLEDMIBAMBits();
		delay_ms(100);
		SetLED(led,0x00);
	}
#endif
#if 0 // All smooth on then all smooth off
/*	SetLED(173,0x60);
	SetLED(176,0x60);
	SetLED(179,0x60);
	SetLED(170,0x40);
	SetLED(167,0x40);
	SetLED(164,0x40);
	SetLED(161,0x20);
	SetLED(158,0x20);
	SetLED(155,0x20);
	SetLED(152,0x10);
	SetLED(149,0x10);
	SetLED(146,0x10);*/
	for(uint8_t led=0;led<LEDS;led+=3){
		for(int16_t b=0;b<=0x7f;b++){
			SetLED(led,b);
			calulateLEDMIBAMBits();
			delay_ms(10);
		}
	}
	for(uint8_t led=0;led<LEDS;led+=3){
		for(int16_t b=0x7f;b>=0;b--){
			SetLED(led,b);
			calulateLEDMIBAMBits();
			delay_ms(10);
		}
	}
#endif
#if 0 // All smooth on then all smooth off
	for(uint8_t led=0,bri=0;led<LEDS;led+=3,bri++){
		SetLED(led,bri);
		calulateLEDMIBAMBits();
		delay_ms(10);
	}
#endif
}

void SetRGB(int32_t group, uint8_t v0, uint8_t v1, uint8_t v2){
//	if(group<0)
//		group = RGBS + group;
/*
	if(group==-1)
		group = 59;
	if(group==-2)
		group = 58;
	if(group==-3)
		group = 57;
	if(group==-4)
		group = 56;
	if(group==-5)
		group = 55;
	if(group==-6)
		group = 54;
*/
	LED_RAW[group*3]=v0 & 0x7F;
	LED_RAW[group*3+1]=v1 & 0x7F;
	LED_RAW[group*3+2]=v2 & 0x7F;
}
void SetLED(uint8_t led, uint8_t v0){
	LED_RAW[led]=v0 & 0x7F;
}

void resetLeds(void){
	//clear led bits
	for(uint8_t a=0; a<LEDS; a++){
		LED_RAW[a]=0;
	}
//	calulateLEDMIBAMBits();
}

void calulateLEDMIBAMBits(){
//	xprintf(INFO "calulateLEDMIBAMBits()");
	uint8_t led,bitinreg;
//	xprintf("bit reg LED_PRECALC[bit][reg]\n");
	for(uint8_t bit=0; bit<BITS; bit++){
		led=0;
		for(uint8_t reg=0; reg<REGS; reg++){
			bitinreg=0;
/*
			_DBG("[INFO]-LED_RAW[0]= ");_DBH(LED_RAW[0]);_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-bitinreg= ");_DBH(bitinreg);_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
			_DBG("[INFO]-(LED_RAW[led++]<<bitinreg)&bitinreg++= ");_DBH16((LED_RAW[led++]<<bitinreg)&(1<<bitinreg++));_DBG("\r\n");//;_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
*/
			uint16_t tt[16],l;
			for(uint8_t t=0;t<15;t++,led++){
				l=LED_RAW[led]>>(bit);
				tt[t] = (l<<bitinreg)&(1<<bitinreg);
				tt[15] = 0;
				bitinreg++;
			}
			LED_PRECALC[reg][bit][0] =
				tt[0] |
				tt[1] |
				tt[2] |
				tt[3] |
				tt[4] |
				tt[5] |
				tt[6] |
				tt[7] |
				tt[8] |
				tt[9] |
				tt[10] |
				tt[11] |
				tt[12] |
				tt[13] |
				tt[14] |
				tt[15];
		}
	}
	//	UPDATE_REQUIRED=true;
//	_DBG("[INFO]-MIBAM Precal time = ");_DBD32(end-start);_DBG(" (");_DBG(__FILE__);_DBG(":");_DBD16(__LINE__);_DBG(")\r\n");
}

void Set_LED_Pattern(uint8_t no,uint8_t speed){
	LED_PATTERN = no;
	LED_SPEED = speed;
}

void LED_loop(void){
	switch(LED_PATTERN){
		case 0:
			LED_time();
			break;
		case 1:
			LED_test();
			break;
		case 2:
			Rainbow();
			break;
		default:
			LED_time();
			break;
	}
}


long colorshift=0;

void Rainbow( void ) {
#ifndef DEV
	#define LEDDELAY 10
	int hue, sat, val;
	unsigned char red, green, blue;
	if(USER_MILLIS>=10){
//	if(LED_UPDATE_REQUIRED){
		LED_UPDATE_REQUIRED=0;
		USER_MILLIS=0;
		colorshift+=1;
		if(colorshift==360)
			colorshift=0;
		// TODO move hue to calulateLEDMIBAMBits();!!!!!!!!
//		for(int cycle=0;cycle<numCycles;cycle++){ // shift the raibom numCycles times
			for(int led=0;led<RGBS;led++){ // loop over all LED's
//			for(int led=0;led<1;led++){ // loop over all LED's
				hue = ((led*1)*360/1+colorshift)%360; // Set hue from 0 to 360 from first to last led and shift the hue
				sat = 255;
				val = 255;
//				hsv2rgb(hue, sat, val, &red, &green, &blue, maxBrightness); // convert hsv to rgb values
				hsv2rgb(hue, sat, val, &red, &green, &blue, 0x7f); // convert hsv to rgb values
				SetRGB(led, red, green, blue); // write rgb values
			}
			calulateLEDMIBAMBits();
//		}
	}
#endif
}


#if 0
		if(timeUpdate()) {
			resetLeds();
	//	colourToRGBled(time_now.hour12()*5,RED1,RED2,RED3,false,0);
	//	colourToRGBled(time_now.minute(),GREEN1,GREEN2,GREEN3,false,0);
	//	colourToRGBled(time_now.second(),BLUE1,BLUE2,BLUE3,false,0);

	/* good
			SetRGB(time_now.second()-4,0,0,1);
			SetRGB(time_now.second()-3,0,0,2);
			SetRGB(time_now.second()-2,0,0,4);
			SetRGB(time_now.second()-1,0,0,100);
			SetRGB(time_now.second()-0,0,0,255);

			SetRGB(time_now.minute()-2,3,0,5);
			SetRGB(time_now.minute()-1,10,0,25);
			SetRGB(time_now.minute()-0,20,0,180);

			SetRGB(time_now.hour12()*5+(time_now.minute()/12),85,0,130);
	*/

			SetRGB(time_now.second()-0,0,0,1);
	//		SetRGB(time_now.second()-3,0,0,2);
	//		SetRGB(time_now.second()-2,0,0,4);
	//		SetRGB(time_now.second()-1,0,0,100);
	//		SetRGB(time_now.second()-0,0,0,255);

			SetRGB(time_now.minute()-1,1,0,1);
			SetRGB(time_now.minute()-0,1,0,1);
	//		SetRGB(time_now.minute()-0,20,0,180);

			SetRGB(time_now.hour12()*5+(time_now.minute()/12),1,1,1);


	//		delay(100);
		}
#endif


/*
static inline void calulateLEDMIBAMBit(uint8 LED){
	uint8_t bitinreg = BITINREG[LED];
	uint8_t whichreg = WHICHREG[LED];
//	for(uint32_t bits=0; bit<BITS; bit++){
	LED_PRECALC[whichreg][bit] = LED_RAW[led++]<<bitinreg &
									LED_RAW[led++]<<bitinreg &;
//	}
}
*/
/*
16bitcol
16bit*60=120bytes
120hz
8ms/refresh
8mhz

sensors

battery
========
highest output current with pot set to 1 = 48mA/led
3 on high	= 144mA
3 of 4/5	= 115.2mA
3 of 3/5	= 86.4mA
2 of 2/5	= 38.4mA
1 of 1/5	= 9.6mA
LPC1768		= non-configured state should draw a maximum of 100 mA
LPC1768		= The maximum value is 500 mA
LPC1768		= 100mA	@ 3.3V = 0.33W
LPC1768		= 500mA	@ 3.3V = 1.65W
Wifi		= 100mA	@ 3.3V = 0.33W
Total		= 393.6mA @ 5V = 2.298W
battery at 2300mAh * 1.2V = 2.76Wh = ~2 hours battery

if pot at 0x30 =  1mA/led
3 on high	= 3mA
3 of 4/5	= 2.4mA
3 of 3/5	= 1.8mA
2 of 2/5	= 0.8mA
1 of 1/5	= 0.2mA
Total		= 8.2mA @ 5V = 0.041W
LPC1768		= non-configured state should draw a maximum of 100 mA
LPC1768		= The maximum value is 500 mA
LPC1768		= 100mA	@ 3.3V = 0.33W
LPC1768		= 500mA	@ 3.3V = 1.65W
Wifi		= 100mA	@ 3.3V = 0.33W
LPC + leds	= ~ 0.88W
battery at 2300mAh * 1.2V = 2.76Wh = 5 hours battery



128	160
64	30
32	10
16	4
8	2
4	1.5
2	1.2
1	1

0.0000326797	1/120/255
16000000	16mhz
0.00000006	1/16mhz
522.875817	clocks
*/

/*=======================================================
buffer size = 8
proc clk	= 100,000,000 Mhz
SSP speed	= 25,000,000 MB/s
SSP speed /	16	= 1,562,500 16bit cyles per sec
above / buffer size = 195,312.5

proc time for 32 bits * buffer size of 8
============================================
1sec / 100,000,000 = 0.000,000,01 = 10ns 1 proc clock
10ns * buffer 8 = 80ns]

timer 1 calc
============================================
500nops = 1600 proc ticks

transfer time for 16 bits * buffer size of 8
============================================
1sec / 25,000,000 = 0.000,000,04 = 40ns to transfer 1 bit
40ns * 16bits * buffer 8 = 5.12us + 50% = 7.86
==========================================================*/
