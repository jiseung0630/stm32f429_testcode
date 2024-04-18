/*
 * cmd.c
 *
 *  Created on: Apr 17, 2024
 *      Author: iot00
 */

/*
 * cli.c
 *
 *  Created on: Apr 12, 2024
 *      Author: iot00
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include "cmsis_os.h"
#include "mem.h"
#include "type.h"
#include "uart.h"
#include "app.h"
#include "cli.h"
#include "i2c_hd44780.h"
#include "softtimer.h"
#include "cmd.h"

static osThreadId_t cmd_thread_hnd;	// 쓰레드 핸들
static osMessageQueueId_t cmd_msg_id;  //메시지큐 핸들

static const osThreadAttr_t cmd_thread_attr = {
  .stack_size = 128 * 8,
  .priority = (osPriority_t) osPriorityNormal,
};

static void cmd_msg_put_0(void *arg);
static void cmd_msg_put(void *arg);


void lcd_test(void)
{
	lcd_init();
	lcd_disp_on();
	lcd_clear_display();
	lcd_home();
	lcd_printf("Hello");
	lcd_locate(2,0);
	lcd_printf("World");
}

void softtimer_callback(void)
{
	static uint8_t count = 0;
	printf("cnt = %d\r\n", count++);
	static uint8_t state = 0;
	const char disp_pg[4] = "\\|/-";

	lcd_locate(1, 16);
	lcd_printchar(disp_pg[state++]);
	state %= 4;

#if 0
	switch (state) {
		case 0 : {
			lcd_printchar('\\'); state++;
		} break;
		case 1 : {
			lcd_printchar('|'); state++;
		} break;
		case 2 : {
			lcd_printchar('/'); state++;
		} break;
		case 3 : {
			lcd_printchar('-'); state=0;
		} break;
	}
#endif
}

void cmd_thread(void *arg)
{
	(void)arg;
	osStatus sts;
	MSG_T rxMsg; //, txMsg;

	uart_regcbf(E_UART_0, cmd_msg_put_0);
	uart_regcbf(E_UART_1, cmd_msg_put);

	lcd_test();

	softtimer_init();
	softtimer_regcbf(softtimer_callback);
	softtimer_start(500);

	while (1) {
		sts = osMessageQueueGet(cmd_msg_id, &rxMsg, NULL, osWaitForever);

		if (sts == osOK) {
			switch (rxMsg.id) {
					case E_MSG_CMD_RX_0 : {
						MEM_T *pMem = (MEM_T *)rxMsg.body.vPtr;
						PKT_T *pRxPkt = (PKT_T *)pMem->buf;

						printf("E_MSG_CMD_RX_0\r\n");
						printf("cmd=%04x, len=%d ", pRxPkt->cmd, pRxPkt->len);
						for (int i=0; i<pRxPkt->len; i++) {
							printf("%02x ", pRxPkt->ctx[i]);
						}
						printf("\r\n");

						mem_free(pMem);
					} break;

				case E_MSG_CMD_RX : {
					MEM_T *pMem = (MEM_T *)rxMsg.body.vPtr;
					PKT_T *pRxPkt = (PKT_T *)pMem->buf;

					printf("E_MSG_CMD_RX\r\n");

					switch (pRxPkt->cmd) {
						case E_CMD_LED : {
							printf("LED command\r\n");
							printf("cmd=%04x, len=%d ", pRxPkt->cmd, pRxPkt->len);
							for (int i=0; i<pRxPkt->len; i++) {
								printf("%02x ", pRxPkt->ctx[i]);
							}
							printf("\r\n");

							{
								bool res = false;
								uint16_t enc_len;
								uint8_t enc_data[200];
								res = slip_encode(pMem->buf, pRxPkt->len + 3, enc_data, &enc_len);
								if (res == true) {
									for (uint16_t i=0; i<enc_len; i++) {
										printf("%02x ", enc_data[i]);
									}
									printf("\r\n");
								}
								extern UART_HandleTypeDef huart2;
								HAL_UART_Transmit(&huart2, enc_data, enc_len, 0xffff);
							}

							mem_free(pMem); //
						} break;

						case E_CMD_CLI: {
							printf("CLI command\r\n");
							pRxPkt->ctx[pRxPkt->len] = 0; //'\0';
							cli_msg_put((void *)pMem);
						} break;
					}
				} break;
			}
		}
	}
}

void cmd_init(void)
{
	cmd_msg_id = osMessageQueueNew(3, sizeof(MSG_T), NULL);

	if (cmd_msg_id != NULL) printf("CMD Message Queue Created...\r\n");
	else {
		printf("CMD Message Queue  Create File...\r\n");
		while (1);
	}

	cmd_thread_hnd = osThreadNew(cmd_thread, NULL, &cmd_thread_attr);
	if (cmd_thread_hnd != NULL) printf("CMD Thread Created...\r\n");
	else {
		printf("CMD Thread Create Fail...\r\n");
		while (1);
	}
}

// ISR
static void cmd_msg_put_0(void *arg)
{
	BUF_T *pBuf = (BUF_T *)arg;

	MEM_T *pMem = mem_alloc(100, 0);
	if (pMem != NULL) {
		memcpy(pMem->buf, pBuf, pBuf->idx);
		Q_PUT(cmd_msg_id, E_MSG_CMD_RX_0, pMem, 0);
	}
}

// ISR
static void cmd_msg_put(void *arg)
{
	BUF_T *pBuf = (BUF_T *)arg;

	MEM_T *pMem = mem_alloc(100, 0);
	if (pMem != NULL) {
		memcpy(pMem->buf, pBuf, pBuf->idx);
		Q_PUT(cmd_msg_id, E_MSG_CMD_RX, pMem, 0);
	}
}
