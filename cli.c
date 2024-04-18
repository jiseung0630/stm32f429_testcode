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
#include "type.h"
#include "uart.h"
#include "app.h"
#include "tim.h"
#include "led.h"
#include "cli.h"

typedef struct {
	char *cmd;	// 명령어
	uint8_t no;	// 인자 최소 갯수
	int (*cbf)(int, char **);  // int argc, char *argv[]
	char *remark; // 설명
} CMD_LIST_T;

static osThreadId_t cli_thread_hnd;	// 쓰레드 핸들
//static osEventFlagsId_t cli_evt_id;		// 이벤트 플래그 핸들
static osMessageQueueId_t cli_msg_id;  //메시지큐 핸들

static const osThreadAttr_t cli_thread_attr = {
  .stack_size = 128 * 8,
  .priority = (osPriority_t) osPriorityNormal,
};

static int cli_led(int argc, char **argv);
static int cli_echo(int argc, char *argv[]);
static int cli_help(int argc, char *argv[]);
static int cli_mode(int argc, char *argv[]);
static int cli_dump(int argc, char *argv[]);
static int cli_duty(int argc, char *argv[]);
static int cli_btn_uart(int argc, char *argv[]);

const CMD_LIST_T gCmdListObjs[] = {
	{ "btn",		2,		cli_btn_uart,		"button(uart)\r\n btn ['a'~'Z']"	},
	{ "duty",		2,		cli_duty,			"led 1 pwm duty\r\n duty [duty:0~999]"	},
	{ "dump",	3,		cli_dump,		"memory dump\r\n dump [address:hex] [length:max:10 lines]"	},
	{ "mode",	2,		cli_mode,		"change application mode\r\n mode [0/1]"	},
	{ "led",		3,		cli_led,			"led [1/2/3] [on/off]"	},
	{ "echo",		2,		cli_echo,			"echo [echo data]"	},
	{ "help", 		1, 		cli_help, 			"help" 					},
	{ NULL,		0,		NULL,				NULL						}
};

//extern UART_HandleTypeDef huart2;
static int cli_btn_uart(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	//HAL_UART_Transmit(&huart2, (uint8_t *)&argv[1][0], 1, 0xffff);
	printf("cli_btn:%c\r\n", argv[1][0]);

	return 0;
}

static int cli_duty(int argc, char *argv[])
{
	uint16_t duty;

	if (argc < 2) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	duty = (uint16_t)strtol(argv[1], NULL, 10);
	printf("tim_duty_set(%d)\r\n", duty);
	//tim_duty_set(duty);

	return 0;
}

static int cli_dump(int argc, char *argv[])
{
	uint32_t address, length, temp;

	if (argc < 3) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	if (strncmp(argv[1], "0x", 2) == 0) address = (uint32_t)strtol(&argv[1][2], NULL, 16);
	else address = (uint32_t)strtol(&argv[1][0], NULL, 16);

	length = (uint32_t)strtol(argv[2], NULL, 10);
	if (length > 10) length = 10;

	printf("address  %08lX, length = %ld\r\n", address, length);

	for (int i=0; i<length; i++) {
		printf("\r\n%08lX : ", (uint32_t)address);

		temp=address;
		for (int j=0; j<16; j++) {
			printf("%02X ", *(uint8_t *)temp);
			temp++;
		}

		temp=address;
		for (int j=0; j<16; j++) {
			char c = *(char *)temp;
			c = isalnum(c) ? c : (char)' ';
			printf("%c", c);
			temp++;
		}

		address = temp;
	}
	printf("\r\n");

	return 0;
}

static int cli_mode(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	long mode = strtol(argv[1], NULL, 10);
	//app_mode((int)mode);

	return 0;
}

static int cli_led(int argc, char *argv[])
{
	if (argc < 3) {
		printf("Err : Arg No\r\n");
		return -1;
	}

	long no = strtol(argv[1], NULL, 10);
	int onoff = strcmp(argv[2], "off");

	if (onoff != 0) onoff = 1;
	bool sts = onoff ? true : false;

	printf("led %ld, %d\r\n", no, onoff);
	 //led_onoff((uint8_t)no, sts);

	return 0;
}

static int cli_echo(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Err : Arg No\r\n");
		return -1;
	}
	printf("%s\r\n", argv[1]);

	return 0;
}

static int cli_help(int argc, char *argv[])
{
	for (int i=0; gCmdListObjs[i].cmd != NULL; i++) {
		printf("%s\r\n", gCmdListObjs[i].remark);
	}

	return 0;
}

static void cli_parser(uint8_t *arg);
//static void cli_event_set(void *arg);
void cli_msg_put(void *arg);

//static BUF_T gBufObj[1];

void cli_thread(void *arg)
{
	(void)arg;
	osStatus sts;
	MSG_T rxMsg, txMsg;

	osDelay(1000);

	txMsg.id = E_MSG_CLI_INIT;
	osMessageQueuePut(cli_msg_id, &txMsg, 0, osWaitForever);

	while (1) {
		sts = osMessageQueueGet(cli_msg_id, &rxMsg, NULL, osWaitForever);

		if (sts == osOK) {
			switch (rxMsg.id) {
				case E_MSG_CLI_INIT: {
					printf("CLI Thread Started...\r\n");
//					printf("Sizeof(MSG_T)=%d\r\n", sizeof(MSG_T));
//					MEM_T *p = NULL;
//
//					p = mem_alloc(10, 0);
//					if (p != NULL) {
//						sprintf((char *)p->buf, "%s", "12345678");
//						printf((char *)p->buf);
//						mem_free(p);
//					}
//
//					uart_regcbf(E_UART_1, cli_msg_put);
				} break;

				case E_MSG_CLI: {
					MEM_T *pMem = (MEM_T *)rxMsg.body.vPtr;
					PKT_T *pRxPkt = (PKT_T *)pMem->buf;

					cli_parser(pRxPkt->ctx);

					mem_free(pMem);
				} break;
			}
		}
	}
}

void cli_init(void)
{
	//cli_evt_id = osEventFlagsNew(NULL);
	cli_msg_id = osMessageQueueNew(3, sizeof(MSG_T), NULL);

	if (cli_msg_id != NULL) printf("CLI Message Queue Created...\r\n");
	else {
		printf("CLI Message Queue  Create File...\r\n");
		while (1);
	}

	cli_thread_hnd = osThreadNew(cli_thread, NULL, &cli_thread_attr);
	if (cli_thread_hnd != NULL) printf("CLI Thread Created...\r\n");
	else {
		printf("CLI Thread Create Fail...\r\n");
		while (1);
	}
}

void cli_msg_put(void *arg)
{
	Q_PUT(cli_msg_id, E_MSG_CLI, arg, osWaitForever);
}


#define D_DELIMITER		" ,\r\n"

static void cli_parser(uint8_t *arg)
{
	int argc = 0;
	char *argv[10];
	char *ptr;

	char *buf = (char *)arg;

	//printf("rx:%s\r\n", (char *)arg);
	// token 분리
	ptr = strtok(buf, D_DELIMITER);
	if (ptr == NULL) return;

	while (ptr != NULL) {
		argv[argc] = ptr;
		argc++;
		ptr = strtok(NULL, D_DELIMITER);
	}

//	for (int i=0; i<argc; i++) {
//		printf("%d:%s\r\n", i, argv[i]);
//	}

	for (int i=0; gCmdListObjs[i].cmd != NULL; i++) {
		if (strcmp(gCmdListObjs[i].cmd, argv[0]) == 0) {
			gCmdListObjs[i].cbf(argc, argv);
			return;
		}
	}

	printf("Unsupported Command\r\n");
}
