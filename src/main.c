/***************************************************************************//**
 * @file
 * @brief Capacitive touch example for EFM32ZG-STK3200
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "em_device.h"
#include "em_adc.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "bspconfig.h"
#include "sl_sleeptimer.h"

#include "capsense.h"
#include "display.h"
#include "render.h"

#include "../img/deadline.xbm"
#include "../img/desk.xbm"
#include "../img/cup.xbm"
#include "../img/splash.xbm"

#define DISPLAY_DEVICE_NO       (0)
#define MINUTES_X				70
#define MINUTES_Y				110

#define STATUS_IDLE				0
#define STATUS_WORK_BUZZ		1
#define STATUS_BREAK_BUZZ		2
#define STATUS_SETTING			3
#define STATUS_WORKING			4
#define STATUS_BREAK			5

#define PROGRESS_START_X		0
#define PROGRESS_START_Y		70
#define PROGRESS_END_X			DISPLAY0_WIDTH
#define PROGRESS_END_Y			(PROGRESS_START_Y+20)
#define PROGRESS_WIDTH			(PROGRESS_END_X - PROGRESS_START_X)

#define BUZZ_STEP_MS			1
#define SHORT_STEP_MS			100
#define LONG_STEP_MS			1000

#define SPLASH_DELAY_MS			2000
#define BUZZ_DELAY_MS			2000
#define REFRESH_DELAY_MS		(60*1000)

//#define USE_LED

static volatile uint32_t msTicks; /* counts 1ms timeTicks */
static DISPLAY_Device_t displayDevice;

static int workMinutes;
static int breakMinutes;
static int currMinute;
static int msecCounter;
static int msecStep;

/** Counts 1ms timeTicks */
static volatile uint32_t msTicks;

/** Timer used for bringing the system back to EM0. */
static sl_sleeptimer_timer_handle_t timeout_timer;
static bool timeout_event = false;

/***************************************************************************//**
 * @brief Callback function for timeout
 ******************************************************************************/
void timeout_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  timeout_event = true;
}

/***************************************************************************//**
 * @brief function for sleeping in energy mode 2
 ******************************************************************************/
void sleep_em2_ms(uint32_t ms)
{
  sl_sleeptimer_start_timer_ms(&timeout_timer, ms, timeout_callback, NULL, 0, 0);
  while (!timeout_event) {
    EMU_EnterEM2(true);
  }
  timeout_event = false;
}

/***************************************************************************//**
 * @brief function for sleeping in energy mode 1
 ******************************************************************************/
void sleep_em1_ms(uint32_t ms)
{
  sl_sleeptimer_start_timer_ms(&timeout_timer, ms, timeout_callback, NULL, 0, 0);
  while (!timeout_event) {
    EMU_EnterEM1();
  }
  timeout_event = false;
}

/***************************************************************************//**
 * @brief SysTick_Handler
 * Interrupt Service Routine for system tick counter
 ******************************************************************************/
void SysTick_Handler(void) {
	msTicks++; /* increment counter necessary in Delay()*/
}

/***************************************************************************//**
 * @brief Delays number of msTick Systicks (typically 1 ms)
 * @param dlyTicks Number of ticks to delay
 ******************************************************************************/
static void Delay(uint32_t dlyTicks) {
	uint32_t curTicks;

	curTicks = msTicks;
	while ((msTicks - curTicks) < dlyTicks)
		;
}

/***************************************************************************//**
 * @brief SysTick_Disable
 * Disable systick interrupts
 ******************************************************************************/
static void SysTickDisable(void)
{
  SysTick->CTRL = 0x0000000;
}

void POMO_Splash()
{
	RENDER_DrawSprite(0, 0, splash_width, splash_height, splash);
	RENDER_UpdateDisplay(true, &displayDevice);
}

void POMO_DrawWork(int currMinute, int workMinutes)
{
	char text[10];
	int xend;
	int xdead;

	sprintf(text, "%3d / %3d", currMinute, workMinutes);
	RENDER_DrawBackdrop();
	RENDER_Write(MINUTES_X, MINUTES_Y, text);

	// progress bar
	xend = PROGRESS_START_X + ((PROGRESS_WIDTH * currMinute) / workMinutes);
	RENDER_FillRect(PROGRESS_START_X, PROGRESS_START_Y, xend, PROGRESS_END_Y, 1);

	// sprites
	RENDER_DrawSprite(PROGRESS_END_X - desk_width, PROGRESS_START_Y-desk_height, desk_width, desk_height, desk_bits);

	xdead = xend-deadline_width;
	if (xdead < 0)
		xdead = 0;

	RENDER_DrawSprite(xdead, PROGRESS_START_Y-deadline_height, deadline_width, deadline_height, deadline_bits);

	RENDER_UpdateDisplay(true, &displayDevice);
}

void POMO_DrawBreak(int currMinute, int breakMinutes)
{
	char text[10];
	int xend;

	sprintf(text, "%3d / %3d", currMinute, breakMinutes);
	RENDER_DrawBackdrop();
	RENDER_Write(MINUTES_X, MINUTES_Y, text);

	// progress bar
	xend = PROGRESS_START_X + ((PROGRESS_WIDTH * currMinute) / breakMinutes);
	RENDER_FillRect(PROGRESS_START_X, PROGRESS_START_Y, xend, PROGRESS_END_Y, 1);

	// sprites
	RENDER_DrawSprite(PROGRESS_END_X - cup_width, PROGRESS_START_Y-cup_height, cup_width, cup_height, cup_bits);

	RENDER_UpdateDisplay(true, &displayDevice);
}

void POMO_DrawSettings(int workMinutes)
{
	char text[10];

	sprintf(text, "      %3d", workMinutes);
	RENDER_DrawBackdrop();
	RENDER_Write(MINUTES_X, MINUTES_Y, text);
	RENDER_UpdateDisplay(true, &displayDevice);
}

static void gpioSetup(void)
{
  /* Enable GPIO clock */
  CMU_ClockEnable(cmuClock_GPIO, true);

#ifdef USE_LED
  GPIO_DriveModeSet(gpioPortC, gpioDriveModeStandard);
  GPIO_PinModeSet(gpioPortC, 10, gpioModePushPull, 0);
#endif

  GPIO_DriveModeSet(gpioPortF, gpioDriveModeHigh);
  GPIO_PinModeSet(gpioPortF, 2, gpioModePushPullDrive, 0);
}

/***************************************************************************//**
 * @brief  Main function
 ******************************************************************************/
int main(void) {
	EMSTATUS status;
	int currStatus = STATUS_IDLE;

	/* Chip errata */
	CHIP_Init();

	/* Setup SysTick Timer for 1 msec interrupts  */
	if (SysTick_Config(CMU_ClockFreqGet(cmuClock_CORE) / 1000)) {
		while (1) ;
	}

	/* Initialize the display module. */
	DISPLAY_Init();

	/* Retrieve the properties of the DISPLAY. */
	status = DISPLAY_DeviceGet(DISPLAY_DEVICE_NO, &displayDevice);
	if (DISPLAY_EMSTATUS_OK != status) {
		return -1;
	}

	/* Start capacitive sense buttons */
	CAPSENSE_Init();

	workMinutes = 25;
	breakMinutes = 10;
	msecCounter = 0;
	currStatus = STATUS_IDLE;
	msecStep = SHORT_STEP_MS;

	POMO_Splash();

	/* Use LFXO for rtc used by the sleeptimer */
	CMU_ClockEnable(cmuClock_HFLE, true);
	CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);

	/* Initialize sleeptimer driver. */
	sl_sleeptimer_init();
	gpioSetup();

	SysTickDisable();

	while (1) {
		sleep_em2_ms(msecStep);

		CAPSENSE_Sense();

		if (CAPSENSE_getPressed(BUTTON1_CHANNEL) && !CAPSENSE_getPressed(BUTTON0_CHANNEL)) {
			currStatus = STATUS_SETTING;
			msecStep = SHORT_STEP_MS;

			workMinutes += 5;
			if (workMinutes > 60)
				workMinutes = 5;

			POMO_DrawSettings(workMinutes);
		} else if (CAPSENSE_getPressed(BUTTON0_CHANNEL) && !CAPSENSE_getPressed(BUTTON1_CHANNEL)) {
			// start
			msecCounter = 0;
			currMinute = 0;
			currStatus = STATUS_WORKING;
			msecStep = LONG_STEP_MS;

			POMO_DrawWork(currMinute, workMinutes);
		}

		switch (currStatus)
		{
		case STATUS_IDLE:
			msecCounter += SHORT_STEP_MS;
			if (msecCounter > SPLASH_DELAY_MS)
			{
				msecCounter = 0;
				currMinute = 0; //workMinutes - 1;
				currStatus = STATUS_WORKING;
				msecStep = LONG_STEP_MS;

				POMO_DrawWork(currMinute, workMinutes);
			}
			break;

		case STATUS_WORKING:
#ifdef USE_LED
			GPIO_PinOutToggle(gpioPortC, 10);
#endif

			msecCounter += LONG_STEP_MS;
			if (msecCounter > REFRESH_DELAY_MS)
			{
				msecCounter = 0;
				currMinute ++;
				if (currMinute >= workMinutes)
				{
					currMinute = 0;
					currStatus = STATUS_WORK_BUZZ;
					msecStep = BUZZ_STEP_MS;

					POMO_DrawBreak(currMinute, breakMinutes);
				}
				else
				{
					POMO_DrawWork(currMinute, workMinutes);
				}
			}
			break;

		case STATUS_BREAK:
#ifdef USE_LED
			GPIO_PinOutToggle(gpioPortC, 10);
#endif

			msecCounter += LONG_STEP_MS;
			if (msecCounter > REFRESH_DELAY_MS)
			{
				msecCounter = 0;
				currMinute ++;
				if (currMinute >= breakMinutes)
				{
					currMinute = 0;
					currStatus = STATUS_BREAK_BUZZ;
					msecStep = BUZZ_STEP_MS;

					POMO_DrawWork(currMinute, workMinutes);
				}
				else
				{
					POMO_DrawBreak(currMinute, breakMinutes);
				}
			}
			break;

		case STATUS_BREAK_BUZZ:
		case STATUS_WORK_BUZZ:
			msecCounter += BUZZ_STEP_MS;
			if (msecCounter > BUZZ_DELAY_MS)
			{
				GPIO_PinOutClear(gpioPortF, 2);

				currMinute = 0;
				msecStep = LONG_STEP_MS;
				if (currStatus == STATUS_BREAK_BUZZ)
				{
					currStatus = STATUS_WORKING;
					POMO_DrawWork(currMinute, workMinutes);
				}
				else
				{
					currStatus = STATUS_BREAK;
					POMO_DrawBreak(currMinute, breakMinutes);
				}
			}
			else
			{
				GPIO_PinOutToggle(gpioPortF, 2);
			}
			break;
		}
	}
}
