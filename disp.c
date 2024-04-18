/*
 * disp.c
 *
 *  Created on: Apr 18, 2024
 *      Author: iot00
 */

#include <stdio.h>
#include "cmsis_os.h"
#include "i2c_hd44780.h"

// debug mode, -DEBUG
#ifdef EBUG
#define FNAME() printf("\n%s (%s, line %d)\n", __func__, __FILE__, __LINE__)

#define DBG(...) \
do { \
	printf("%s (%s, line %d): ", __func__, __FILE__, __LINE__); \
    printf(__VA_ARGS__); \
    printf("\n");  \
} while (0)
#else
    #define FNAME()  do{}while(0)
    #define DBG(...) do{}while(0)
#endif //EBUG

static osMutexId_t disp_mutex;

static const osMutexAttr_t disp_mutex_attr = {
	"Disp Mutex",
	osMutexPrioInherit,    // attr_bits
	NULL,                                     // memory for control block
	0U                                        // size for control block
};

#define disp_mutex_acq()		osMutexAcquire(disp_mutex, osWaitForever)
#define disp_mutex_rls()		osMutexRelease(disp_mutex)
#define disp_printf(fmt, args...) do { disp_mutex_acq(); lcd_printf(fmt, ##args); sp_mutex_rls();   } while(0)

void disp_init(void)
{
	disp_mutex = osMutexNew(&disp_mutex_attr);
	if (disp_mutex != NULL)  {
		printf("Disp mutex Created...\r\n");
	} else {
		printf("Disp mutex Create Fail...\r\n");
	}

	lcd_init();
}

void disp_on(void)
{
	disp_mutex_acq();
	lcd_disp_on();
	disp_mutex_rls();
}

void disp_off(void)
{
	disp_mutex_acq();
	lcd_disp_off();
	disp_mutex_rls();
}

void disp_home(void)
{
	disp_mutex_acq();
	lcd_home();
	disp_mutex_rls();
}

void clear_display(void)
{
	disp_mutex_acq();
	lcd_clear_display();
	disp_mutex_rls();
}

void disp_locate(uint8_t row, uint8_t column)
{
	disp_mutex_acq();
	lcd_locate(row, column);
	disp_mutex_rls();
}

void disp_printchar(unsigned char ascode)
{
	disp_mutex_acq();
	lcd_printchar( ascode);
	disp_mutex_rls();
}

void disp_print_string (char *str)
{
	disp_mutex_acq();
	lcd_print_string(str);
	disp_mutex_rls();
}



