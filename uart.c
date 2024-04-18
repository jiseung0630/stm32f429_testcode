/*
 * uart.c
 *
 *  Created on: Apr 11, 2024
 *      Author: iot00
 */
#include <stdbool.h>
#include <stdio.h>
#include "uart.h"

extern UART_HandleTypeDef huart2;		// idx = 0
extern UART_HandleTypeDef huart3;		// idx = 1

#define D_BUF_OBJ_MAX	3
static uint8_t rxdata[D_BUF_OBJ_MAX];
static BUF_T gBufObjs[D_BUF_OBJ_MAX];

//static void (*uart_cbf[D_BUF_OBJ_MAX])(void *);
//typedef void (*UART_CBF)(void *);
static UART_CBF uart_cbf[D_BUF_OBJ_MAX];

void uart_init(void)
{
	for (int i=0; i<D_BUF_OBJ_MAX; i++) {
		gBufObjs[i].idx = 0;
		gBufObjs[i].flag = false;
	}

	// 인터럽트 방식 수신 시작 : 1바이트
	HAL_UART_Receive_IT(&huart2, (uint8_t *)&rxdata[E_UART_0], 1); // uart 2
	HAL_UART_Receive_IT(&huart3, (uint8_t *)&rxdata[E_UART_1], 1);	// uart 3
}

bool uart_regcbf(uint8_t idx, UART_CBF cbf)
{
	if (idx > D_BUF_OBJ_MAX) return false;
	uart_cbf[idx] = cbf;
	return true;
}

//void uart_thread(void *arg)
//{
//	for (int i=0; i<D_BUF_OBJ_MAX; i++) {
//		if (gBufObjs[i].flag == true) {
//			if (uart_cbf != NULL) uart_cbf((void *)&gBufObjs[i]);
//			gBufObjs[i].idx = 0;
//			gBufObjs[i].flag = false;
//		}
//	}
//}

#define FEND			0xC0
#define FESC			0xDB
#define TFEND			0xDC
#define TFESC			0xDD

static void slip_decode(uint8_t *pstate, uint8_t rxd, BUF_T *p, UART_CBF uart_cbf)
{
	switch (*pstate) {
		case 0: {
			if (rxd == FEND) {
				(*pstate)++;
				p->idx = 0;
			}
		} break;

		case 1: {
			if (rxd == FEND) {
				if (p->idx == 0) {
					(*pstate) = 0;
				} else {
					p->flag = true;
					if (uart_cbf != NULL) uart_cbf((void *)p);
					p->flag = false;
					(*pstate) = 0;
				}
			} else if (rxd == FESC) {
				(*pstate)++;
			} else {
				p->buf[p->idx++] = rxd;
			}
		} break;

		case 2: {
			if (rxd == TFEND)			{
				p->buf[p->idx++] = FEND;
				(*pstate)--;
			} else if (rxd == TFESC)	{
				p->buf[p->idx++] = FESC;
				(*pstate)--;
			} else {
				(*pstate) = 0;
			}
		} break;
	}
}

bool slip_encode(const uint8_t *pRaw, uint16_t rawLen, uint8_t *pEncode, uint16_t *pEncodeLen)
{
	if (pRaw == NULL || pEncode == NULL || pEncodeLen == NULL) return false;

	uint16_t idx = 0;

	pEncode[idx++] = FEND;
	for (uint16_t i=0; i<rawLen; i++) {
		if (pRaw[i] == FEND) {
			pEncode[idx++] = FESC;
			pEncode[idx++] = TFEND;
		} else if (pRaw[i] == FESC) {
			pEncode[idx++] = FESC;
			pEncode[idx++] = TFESC;
		} else {
			pEncode[idx++] = pRaw[i];
		}
	}
	pEncode[idx++] = FEND;

	pEncodeLen[0] = idx;  // *pEncodeLen = idx;

	return true;
}

// 인터럽트 서비스 루틴 (ISR)
static uint8_t state[2] = {0, };

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	volatile uint8_t rxd;

	if (huart == &huart2) {	// idx = 0
		rxd = rxdata[E_UART_0];
		HAL_UART_Receive_IT(huart, (uint8_t *)&rxdata[E_UART_0], 1);

		BUF_T *p = (BUF_T *)&gBufObjs[E_UART_0];
		if (p->flag == false) {
			slip_decode(&state[E_UART_0], rxd, p, uart_cbf[E_UART_0]);
		}
	}

	if (huart == &huart3) { // idx = 1
		rxd = rxdata[E_UART_1];
		HAL_UART_Receive_IT(huart, (uint8_t *)&rxdata[E_UART_1], 1);

		BUF_T *p = (BUF_T *)&gBufObjs[E_UART_1];
		if (p->flag == false) {
			slip_decode(&state[E_UART_1], rxd, p, uart_cbf[E_UART_1]);
#if 0
//			switch (state) {
//				case 0: {
//					if (rxd == FEND) { state++;  p->idx = 0; }
//				} break;
//
//				case 1: {
//					if (rxd == FEND) {
//						if (p->idx == 0) { 	state = 0;
//						} else {
//							p->flag = true;
//							if (uart_cbf[1] != NULL) uart_cbf[1]((void *)p); //&gBufObjs[E_UART_1]);
//							p->flag = false;
//							state = 0;
//						}
//					} else if (rxd == FESC) { state++;
//					} else { p->buf[p->idx++] = rxd; }
//				} break;
//
//				case 2: {
//					if (rxd == TFEND)			{ p->buf[p->idx++] = FEND; 	state--;
//					} else if (rxd == TFESC)	{ p->buf[p->idx++] = FESC;	state--;
//					} else { state = 0; }
//				} break;
//			}
#endif
#if 0
			p->buf[p->idx] = rxd;
			//p->idx++;
			//p->idx %= D_BUF_MAX;
			if (p->idx < D_BUF_MAX) p->idx++;

			if (rxd == '\r' || rxd == '\n') {
				p->buf[p->idx] = 0; //'\0';
				p->flag = true;
				if (uart_cbf[1] != NULL) uart_cbf[1]((void *)&gBufObjs[E_UART_1]);
				p->idx = 0;
				p->flag = false;
			}
#endif
		}
	}
}
