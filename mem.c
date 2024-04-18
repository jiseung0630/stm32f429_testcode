/*
 * mem.c
 *
 *  Created on: Apr 16, 2024
 *      Author: iot00
 */

#include <stdbool.h>
#include <stdio.h>
#include "cmsis_os.h"
#include "mem.h"

#define D_MEM_COUNT_MAX		10

static osMemoryPoolId_t mem_id = NULL;

void mem_init(void)
{
	mem_id = osMemoryPoolNew(D_MEM_COUNT_MAX, sizeof(MEM_T), NULL);
	if (mem_id != NULL) printf("Memory Pool Created...\\n");
	else {
		printf("Memory Pool Create Fail...\r\n");
		while (1);
	}
}

// Timeout is always 0, when mem_alloc is called in ISR.
MEM_T *mem_alloc(uint16_t size, uint32_t timeout)
{
	MEM_T *pMem = NULL;

	pMem = osMemoryPoolAlloc(mem_id, timeout);
	if (pMem == NULL) return NULL;
	pMem->id = mem_id;
	return pMem;
}

bool mem_free(MEM_T *pMem)
{
	 osMemoryPoolId_t id;
	 if (pMem == NULL) return false;
	 if (pMem->id == NULL) return false;
	 id = pMem->id;
	 pMem->id = NULL;
	 osMemoryPoolFree(id, pMem);
	 return true;
 }
