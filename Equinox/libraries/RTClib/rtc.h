/*
 * rtc.h
 *
 *  Created on: 27 Jun 2012
 *      Author: gavin
 */

#ifndef RTC_H_
#define RTC_H_


#include "lpc_types.h"

typedef enum {
	DAY,
	NIGHT,
	RISE_ONLY,
	SET_ONLY,
	ALL_DAY,
	ALL_NIGHT
} Day_Night_Num;

#define DOW_LEN 3
#define DOW_LEN_MAX 10 // +1 to include NULL
#define NUM_DAYS_OF_WEEK 7
const char DayOfWeekName[NUM_DAYS_OF_WEEK][DOW_LEN_MAX] = {
"Monday",
"Tuesday",
"Wednesday",
"Thursday",
"Friday",
"Saturday",
"Sunday"
};

#define MONTH_LEN 3
#define MONTH_LEN_MAX 10 // +1 to include NULL
#define NUM_MONTHS 12
const char Month_of_the_year[NUM_MONTHS][MONTH_LEN_MAX] = {
"January",
"February",
"March",
"April",
"May",
"June",
"July",
"August",
"September",
"October",
"November",
"December"
};

const uint8_t daysInMonth [12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

struct {
	uint32_t unix;		// updated once a second
	uint32_t unix_utc;	// updated once a second
	uint16_t year;		// updated once a second //format 2xxx
	uint8_t month;		// updated once a second //format 1-12
	uint8_t dom;		// updated once a second //format 1-31
	uint8_t dow;		// updated once a second //format 0-6
	uint8_t doy;		// updated once a second //format 1-366
	uint8_t hh;			// updated once a second //format 0-23
	uint8_t mm;			// updated once a second //format 0-59
	uint8_t ss;			// updated once a second //format 0-59
	uint8_t hh_utc;			// updated once a second
	uint8_t mm_utc;			// updated once a second
	uint8_t ss_utc;			// updated once a second
	uint32_t DST_begin_calculated;	// updated once a year
	uint32_t DST_end_calculated;	// updated once a year
//	uint16_t dst_last_update_year;	// updated once a year
	int8_t dst_correction;		// updated once
	uint32_t sunrise_unix;	// updated once a day
	uint32_t sunrise_unix_utc;	// updated once a day
	uint32_t sunset_unix;	// updated once a day
	uint32_t sunset_unix_utc;	// updated once a day
	uint32_t noon_unix;  	// updated once a day
	uint32_t noon_unix_utc;  	// updated once a day
	uint8_t day_night;  	// updated once a day
	uint8_t no_set_rise;  	// updated once a day
	uint8_t second_inc;		// updated once a second
//	uint32_t use_utc;
} time;

void RTC_IRQHandler(void);
void RTC_time_Init();

// Interrupt checks
void secondlyCheck(void);
void hourlyCheck(void) ;
void minutelyCheck(void);
void dailyCheck(void);
void weeklyCheck(void);
void monthlyCheck(void);
void yearlyCheck(void);

uint32_t GetUNIX();
uint16_t GetY();
uint8_t GetM();
uint8_t GetDOM();
uint8_t GetDOW();
uint8_t GetDOY();
uint8_t GetHH();
uint8_t GetMM();
uint8_t GetSS();
void GetSunRiseHH_MM_SS(char * str);
void GetSunSetHH_MM_SS(char * str);
void GetNoonHH_MM_SS(char * str);
int8_t GetDST_correction();
void RTC_time_SetTime(uint16_t year, uint8_t month, uint8_t dayOfM, uint8_t hour, uint8_t min, uint8_t sec, int8_t st);
// number of days since 20xx/01/01, valid for 2001..2099
uint16_t days_from_20xx(uint16_t year, uint8_t month, uint8_t dayOfM);
// number of days since 2000/01/01, valid for 2001..2099
uint16_t days_from_2000(uint16_t year, uint8_t month, uint8_t dayOfM);
// convert date into seconds
long time2long(uint16_t days, uint8_t hour, uint8_t min, uint8_t sec);
// find day of the week
uint8_t dayOfWeekManual(uint16_t year, uint8_t month, uint8_t dayOfM);
// convert char to uint8_t for time
uint8_t conv2d(const char* p);
uint32_t RTC_time_GetUnixtime();
uint32_t RTC_time_FindUnixtime(uint16_t year, uint8_t month, uint8_t dayOfM, uint8_t hour, uint8_t min, uint8_t sec);
void DST_check_and_correct();
void RTC_set_default_time_to_compiled(void);
void RTC_print_time(void);
void unix_to_hh_mm_ss(uint32_t t, uint8_t * hh, uint8_t * mm, uint8_t * ss);
uint32_t dst_correction_needed();
uint32_t dst_correction_needed_t_y(uint32_t t, uint16_t year);
void DSTyearly();


#endif /* RTC_H_ */
