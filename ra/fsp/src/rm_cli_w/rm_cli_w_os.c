/**
 ****************************************************************************************
 *
 * @file rm_cli_w_os.c
 *
 * @brief RTOS command functions
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */


#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "rm_cli_w_utils.h"
#include "rm_cli_w_debug_utils.h"
#include "rm_cli_w_os_task.h"
#include "sdk_defs.h"
#include "FreeRTOS.h"
#include "task.h"
#if (dg_configSYSTEMVIEW == 1)
#include "SEGGER_RTT.h"
#endif
#include "common_def.h"

extern HeapRegion_t *get_heap_regions(void);

/*globas pointer to structure holding info needed to support clr ps*/
#define MAX_TASKS_SUPPORTED_FOR_PS_CLR 32
typedef struct task_clear_tick {
    uint32_t UniqueTaskId;
    uint32_t LastClearTick;
} tsk_clear_tick;

typedef struct tsk_clear_count {
    uint32_t TotalTickStamp;
    struct task_clear_tick ClrCounterArray[MAX_TASKS_SUPPORTED_FOR_PS_CLR];
} tsk_clear_count;

static struct tsk_clear_count *taskClrCounter = NULL;

static char *stringTaskStatus[] = 
	{ "Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid" };
static char taskinfobuf[128];

/* print task information */
void show_task_list(void)
{
	TaskStatus_t	*pxTaskStatusArray;
	UBaseType_t		uxArraySize, x;
	uint32_t		ulTotalTime;
    uint64_t        ulStatsAsPercentage = 0;
	TaskStatus_t	*pTaskInfo;
	UBaseType_t		copyCount;
	int				stackSize;
	tskTCB			*p_TCB;
	char			*taskStatus, *tmpbuf;

	uxArraySize = uxTaskGetNumberOfTasks();

	printf("\n <<< Task information >>>\n");

	pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

	if (pxTaskStatusArray != NULL) {
        /* Generate the (binary) data. */
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalTime);

		/* Sort by xTaskNumber */
		pTaskInfo = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

		if (pTaskInfo != NULL) {
            UBaseType_t searchNo=0, sIdx;

            memset(pTaskInfo, 0, (uxArraySize * sizeof(TaskStatus_t)) );

            for( copyCount = 0; copyCount < uxArraySize; copyCount ++ ) {

                sIdx = uxArraySize;
                for( UBaseType_t i = 0; i < uxArraySize; i++ ) {
                    if ( ((UBaseType_t)~(0uL)) != pxTaskStatusArray[i].xTaskNumber) {
                        searchNo = pxTaskStatusArray[i].xTaskNumber;
                        sIdx = i;
                    }
                }

                if(sIdx == uxArraySize){
                    break;
                }

				for( UBaseType_t i = 0; i < uxArraySize; i++ ) {
                    if ( ((UBaseType_t)~(0uL)) == pxTaskStatusArray[i].xTaskNumber) {
                        continue;
                    }else 
                    if (searchNo > pxTaskStatusArray[i].xTaskNumber) {
						searchNo = pxTaskStatusArray[i].xTaskNumber;
                        sIdx = i;
					}
				}

                memcpy(&pTaskInfo[copyCount], &pxTaskStatusArray[sIdx], sizeof(TaskStatus_t));
                pxTaskStatusArray[sIdx].xTaskNumber = ((UBaseType_t)~(0uL));
			}

		} else {
            printf(" pTaskInfo Alloc failed\n");
			vPortFree(pxTaskStatusArray);
			return;
		}

		printf(" Task count: %lu  TotalTime: %lu Ticks\n", uxArraySize, ulTotalTime);

#if configGENERATE_RUN_TIME_STATS == 1
		printf(" ============================================================================================\n");
		printf(" No TaskName         State         Run-Tm  Run-Per Prio    Stack-B    Stack-E  S-Size Stack-H\n");
		printf(" ============================================================================================\n");
#else
		printf(" ========================================================================\n");
		printf(" No TaskName         State     Prio    Stack-B    Stack-E  S-Size Stack-H\n");
		printf(" ========================================================================\n");
#endif /* configGENERATE_RUN_TIME_STATS == 1 */

		/* Create a human readable table from the binary data. */
		for( x = 0; x < uxArraySize; x++ ) {
            if (ulTotalTime > 0UL) {
                ulStatsAsPercentage = (uint64_t)pTaskInfo[x].ulRunTimeCounter;
				ulStatsAsPercentage = (ulStatsAsPercentage*100) / ulTotalTime;
			}
			p_TCB = pTaskInfo[x].xHandle;
			stackSize = (p_TCB->pxEndOfStack - p_TCB->pxStack);

			switch (pTaskInfo[x].eCurrentState) {
				case eRunning:
					taskStatus = stringTaskStatus[eRunning];
					break;

				case eReady:
					taskStatus = stringTaskStatus[eReady];
					break;

				case eBlocked:
					taskStatus = stringTaskStatus[eBlocked];
					break;

				case eSuspended:
					taskStatus = stringTaskStatus[eSuspended];
					break;

				case eDeleted:
					taskStatus = stringTaskStatus[eDeleted];
					break;

				case eInvalid:		/* Fall through. */
				default:			/* Should not get here, but it is included
									 * to prevent static checking errors. */
					taskStatus = NULL;
					break;
			}

            taskinfobuf[0] = '\0';

            tmpbuf = &(taskinfobuf[strlen(taskinfobuf)]);
#if configGENERATE_RUN_TIME_STATS == 1
			/* TaskNo TaskName State Run-Tm */
			sprintf(tmpbuf, " %2d %-16s %-9s%11lu ",
							(int)(pTaskInfo[x].xTaskNumber),
							pTaskInfo[x].pcTaskName,
							taskStatus,
							pTaskInfo[x].ulRunTimeCounter);
#else
			/* TaskNo TaskName State */
			sprintf(tmpbuf, " %2d %-16s %-9s ",
							(int)pTaskInfo[x].xTaskNumber,
							pTaskInfo[x].pcTaskName,
							taskStatus);
#endif /* configGENERATE_RUN_TIME_STATS == 1 */

			/* Run-Per */
            tmpbuf = &(taskinfobuf[strlen(taskinfobuf)]);
            if (ulTotalTime > 0UL) {
                if (ulStatsAsPercentage > 0UL) {
    				sprintf(tmpbuf, "%7u%% ", (unsigned int)ulStatsAsPercentage);
    			} else {
    				sprintf(tmpbuf, "%7s%% ", "<1");
    			}
			}

			/* Prio Stack-B Stack-E S-Size Stack-H */
            tmpbuf = &(taskinfobuf[strlen(taskinfobuf)]);
			sprintf(tmpbuf, "%4u 0x%x 0x%x %7u %7ld",
						(unsigned int)(pTaskInfo[x].uxCurrentPriority),
						(unsigned int)(pTaskInfo[x].pxStackBase),
						(unsigned int)(p_TCB->pxEndOfStack),
						stackSize * sizeof(StackType_t),	//Change stack size unit : StackType_t -> byte
						(long int)(stackSize - pTaskInfo[x].usStackHighWaterMark) * sizeof(StackType_t));	//Change stack size unit : StackType_t -> byte

            puts(taskinfobuf);
		}

#if configGENERATE_RUN_TIME_STATS == 1
        printf(" ============================================================================================\n");
#else
		printf(" ========================================================================\n");
#endif /* configGENERATE_RUN_TIME_STATS == 1 */

		vPortFree(pxTaskStatusArray);
		vPortFree(pTaskInfo);
	} else {
		printf(" pxTaskStatusArray Alloc failed\n");
	}
}

/* rtos_clear_task - a cli service to take a counter snapshoot, the snapshoot will be used in TaskUpdateCountClear*/
static bool rtos_clear_task(int argc, const char **argv)
{
    (void) argc;
    (void) argv;

	TaskStatus_t	*pxTaskStatusArray;
	uint32_t		ulTotalTime;
	UBaseType_t		uxArraySize;

    /* on first execution allocate space */
	if (taskClrCounter == NULL){
        taskClrCounter = pvPortMalloc(sizeof(tsk_clear_count));
        if(taskClrCounter == NULL){
            printf(" tsk_clear_count Alloc failed\n");
            return true;
        }
        memset(taskClrCounter,0,sizeof(tsk_clear_count));
    }

	uxArraySize = uxTaskGetNumberOfTasks();
	pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
	if (pxTaskStatusArray != NULL) {
        /* Generate the (binary) data. */
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalTime);
        memset(taskClrCounter,0,sizeof(tsk_clear_count));
    	printf("\nclear task counters: total time %ld, total tick %d\n",ulTotalTime, (int)(taskClrCounter->TotalTickStamp));
        taskClrCounter->TotalTickStamp = ulTotalTime;

		for( UBaseType_t i = 0; i < uxArraySize; i++ ) {
			taskClrCounter->ClrCounterArray[i].UniqueTaskId = pxTaskStatusArray[i].xTaskNumber;
			taskClrCounter->ClrCounterArray[i].LastClearTick = pxTaskStatusArray[i].ulRunTimeCounter;
		}
	} else {
		printf(" pxTaskStatusArray Alloc failed\n");
	}
    return true;
}

static bool rtos_task(int argc, const char **argv)
{
    (void) argc;
    (void) argv;

    show_task_list();

    return true;
}


static bool rtos_ticks(int argc, const char **argv)
{
    (void) argc;
    (void) argv;

    printf("OS_GET_TICK_COUNT:%ld\n", xTaskGetTickCount());

    return true;
}
#if CFG_WIFI
static bool rtos_heap(int argc, const char **argv)
{
    (void) argc;
    (void) argv;

#if 0
    // HEAP_4
    size_t  free_heap_size = xPortGetFreeHeapSize();
    size_t  minimum_free_heap_size = xPortGetMinimumEverFreeHeapSize();
    size_t xTotalHeapSize = configTOTAL_HEAP_SIZE;
    size_t uxAddress = xPortGetHeapAddress();

    printf("\r\n <<< HEAP Status >> \r\n");

    printf(" Heap StartAddress       : 0x%08x\r\n", uxAddress & 0x0fffffff);
    printf(" Heap Size(in Bytes)     : %10d\r\n", configTOTAL_HEAP_SIZE);
    printf(" Minimum ever free bytes : %10d\r\n", minimum_free_heap_size);
    printf(GREEN_COLOR " Available heap space    : %10d \r\n" CLEAR_COLOR, free_heap_size);
#endif

    // HEAP_5
    HeapRegion_t	*xHeapRegions;
    size_t		size;
    size_t		totalsize = 0;
    int			i;
    #if (configTOTAL_HEAP_SIZE_DYNAMIC_ALLOC == 0)
    extern uint32_t __ddsc_IPDRAM_START;
    extern uint32_t __ddsc_RAM_END;
    extern uint32_t __ddsc_RAMCODE_END;
    extern uint32_t __ddsc_RAMCODE_START;
    size_t      unused_ram_size = 0;
    #endif

    printf("\r\n <<< HEAP Status >> \r\n");

    xHeapRegions = (HeapRegion_t *)get_heap_regions();

    for (i = 0; xHeapRegions[i].pucStartAddress != NULL; i++ ) {
        printf(" xHeapRegions[%d].pucStartAddress   : %p\r\n", i, xHeapRegions[i].pucStartAddress);
        printf(" xHeapRegions[%d].xSizeInBytes      : %d\r\n", i, xHeapRegions[i].xSizeInBytes);

        totalsize += xHeapRegions[i].xSizeInBytes;
    }

    printf("\n Total os.HEAP                     : %d\r\n", totalsize);
    size = xPortGetMinimumEverFreeHeapSize();
    printf(" Minimum ever free bytes remaining : %d\r\n", size);
    size = xPortGetFreeHeapSize();
    printf(GREEN_COLOR " Available HEAP space              : %d\r\n" CLEAR_COLOR, size);
    #if (configTOTAL_HEAP_SIZE_DYNAMIC_ALLOC == 0)
    unused_ram_size = ((size_t)(&__ddsc_IPDRAM_START) - (size_t)(&__ddsc_RAM_END)) - ((size_t)(&__ddsc_RAMCODE_END) - (size_t)(&__ddsc_RAMCODE_START));
    printf(GREEN_COLOR " Available RAM space               : %d\r\n" CLEAR_COLOR, unused_ram_size);
    #endif

#ifdef USED_HEAP_BLOCK_STATUS
    printUsedHeapBlockInfo();
#endif	// USED_HEAP_BLOCK_STATUS

    return true;
}
#endif
static bool rtos_sleep(int argc, const char **argv)
{
    (void) argc;

    portTickType xFlashRate, xLastFlashTime;

    if ( strcmp(argv[1], "tick") == 0 ) {
        if ( parse_u32(argv[2], &xFlashRate) == true ) {
            printf("pre-tick:%ld\n", xTaskGetTickCount());
            vTaskDelay(xFlashRate);
            printf("pos-tick:%ld\n", xTaskGetTickCount());
        }
    } else if ( strcmp(argv[1], "ms") == 0 ) {
        if ( parse_u32(argv[2], &xFlashRate) == true ) {
            xFlashRate = portCONVERT_MS_2_TICKS(xFlashRate);
            xLastFlashTime = xTaskGetTickCount();

            printf("pre-tick:%ld\n", xLastFlashTime);
            vTaskDelayUntil( &xLastFlashTime, xFlashRate );
            printf("pos-tick:%ld\n", xTaskGetTickCount());
        }

    } else if ( strcmp(argv[1], "sec") == 0 ) {
        if ( parse_u32(argv[2], &xFlashRate) == true ) {
            xFlashRate = portCONVERT_MS_2_TICKS(xFlashRate * 1000);
            xLastFlashTime = xTaskGetTickCount();

            printf("pre-tick:%ld\n", xLastFlashTime);
            vTaskDelayUntil( &xLastFlashTime, xFlashRate );
            printf("pos-tick:%ld\n", xTaskGetTickCount());
        }

    } else {
        printf("%s %s <tick|ms|sec> [num]\n", argv[0], argv[1]);
        return false;
    }

    return true;
}

static bool rtos_version(int argc, const char **argv)
{
    (void) argc;
    (void) argv;

    printf("FreeRTOS Official Release : %s\n"
           , tskKERNEL_VERSION_NUMBER);

#if 0	// Disable duplicate information
    printf("FreeRTOS V %d.%d.%d\n"
           , tskKERNEL_VERSION_MAJOR
           , tskKERNEL_VERSION_MINOR
           , tskKERNEL_VERSION_BUILD  );
#endif // 0

#if (dg_configSYSTEMVIEW == 1)
    printf("_SEGGER_RTT: %p\n", &(_SEGGER_RTT));
#endif

    return true;
}

static const debug_handler_t os_handlers[] = {
    { "ver",   "Show FreeRTOS kernel version", rtos_version    },
    { "task",  "Show task lists",              rtos_task       },
    { "ps",    "Show task lists",              rtos_task       },
    { "psclr", "Clear Task lists ",            rtos_clear_task },
    { "tick",  "Get current OS tick count",    rtos_ticks      },
#if CFG_WIFI
    { "heap",  "Show heap memory status",      rtos_heap       },
#endif
    { "sleep", "<tick|ms|sec> [num]",          rtos_sleep      },
    { NULL },
};

bool os_command(int argc, const char *argv[], void *user_data)
{
    (void) user_data;

    return debug_handle_message(argc, argv, os_handlers);
}
