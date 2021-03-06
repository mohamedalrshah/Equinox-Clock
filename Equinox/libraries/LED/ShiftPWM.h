/*

*/

#ifndef CShiftPWM_h
#define CShiftPWM_h

#include "lpc_types.h"

#define MAX_DELAY 1024 // delay uses number squared

#define MAX_PATTERNS 11
#define MAX_PATTERNS_LETTERS 40
const char LED_PATTERN_NAME[MAX_PATTERNS][MAX_PATTERNS_LETTERS] = {
	"All LED's Off",
	"Clock",
	"One by one",
	"Rainbow Rotating",
	"Simple all colors",
	"One by one smooth all on all off red",
	"One by one smooth all on all off green",
	"One by one smooth all on all off blue",
	"Rainbow all LED's the same colour",
	"Pause",
	"Play"
};

#define LEDP_off 0
#define LEDP_time 1
#define LEDP_one_by_one 2
#define LEDP_rotating_rainbow 3
#define LEDP_simple_all_colors 4
#define LEDP_one_by_one_smooth_all_on_all_off0 5
#define LEDP_one_by_one_smooth_all_on_all_off1 6
#define LEDP_one_by_one_smooth_all_on_all_off2 7
#define LEDP_rainbow_all 8
#define LEDP_Pause 9
#define LEDP_Play 10

#define LEDP_RAW 99

void TIMER0_IRQHandler(void);
void TIMER1_IRQHandler(void);
void TIMER2_IRQHandler(void);
void DMA_IRQHandler (void);
void LatchIn(void);
void WaitForSend(void);
void LED_init(void);
void SetHue(uint32_t led, uint32_t hue);
void SetRGB(int32_t group, uint8_t r, uint8_t g, uint8_t b);
void SetRGBALL(uint8_t r, uint8_t g, uint8_t b);
void SetLED(uint8_t led, uint8_t v0);
void resetLeds(void);
void calulateLEDMIBAMBits(void);
void Set_LED_Pattern(uint8_t no, uint16_t delay, uint8_t bri);
void Get_LED_Pattern(uint8_t * no, uint16_t * delay, uint8_t * bri);
uint8_t SetBrightness(uint8_t bri);
uint8_t GetBrightness(void);
void LED_off(void);
void LED_on(void);
void LED_loop(void);

#endif
