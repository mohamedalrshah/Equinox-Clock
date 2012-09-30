/*

*/

#ifndef CShiftPWM_h
#define CShiftPWM_h

#include "lpc_types.h"

typedef enum {
	red = 0b001,
	green = 0b010,
	blue = 0b100,
	all = 0b111,
} colour;

void TIMER0_IRQHandler(void);
void TIMER1_IRQHandler(void);
void LatchIn(void);
void WaitForSend(void);
void DMA_IRQHandler (void);
void LED_init(void);
void LED_test(void);
void LED_time(uint8_t HH, uint8_t MM, uint8_t SS);
void SetRGB(int32_t group, uint8_t v0, uint8_t v1, uint8_t v2);
void SetLEDColour(uint8_t led, colour col, uint8_t v0);
void SetLED(uint8_t led, uint8_t v0);
void resetLeds(void);
void calulateLEDMIBAMBits(void);


#endif
