/*
 * softtimer.c
 *
 *  Created on: Apr 18, 2024
 *      Author: iot00
 */

#include <stdio.h>
#include "cmsis_os.h"

static osTimerId_t timer_hnd;

static void periodic_func(void *);
void (*timer_cbf)(void);

void softtimer_init(void)
{
	timer_hnd = osTimerNew(periodic_func, osTimerPeriodic, NULL, NULL);
	if (timer_hnd != NULL) {
		printf("Soft Timer Created...\r\n");
	} else {
		printf("Soft Timer Create Fail...\r\n");
		while (1);
	}
}

void softtimer_start(uint32_t ticks)
{
	osTimerStart(timer_hnd, ticks);
}

void softtimer_stop(void)
{
	osTimerStop(timer_hnd);
}

void softtimer_regcbf(void (*cbf)(void))
{
	timer_cbf = cbf;
}

static void periodic_func(void *arg)
{
	(void)arg;

	if (timer_cbf != NULL) timer_cbf();
}
