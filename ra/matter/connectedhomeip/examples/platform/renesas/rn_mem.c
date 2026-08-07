/*
 * Copyright (c) 2021 Project CHIP Authors
 * Copyright (c) 2023 Modified by Renesas Electronics Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * FreeRTOS Kernel V10.4.3
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*
 * A sample implementation of pvPortMalloc() and vPortFree() that combines
 * (coalescences) adjacent memory blocks as they are freed, and in so doing
 * limits memory fragmentation.
 *
 * See heap_1.c, heap_2.c and heap_3.c for alternative implementations, and the
 * memory management pages of https://www.FreeRTOS.org for more information.
 */
#include <stdlib.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "rn_mem.h"
#include "task.h"
//matterwork[[::for Renesas
#include "common_def.h"
#include <stdio.h>

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#if (configSUPPORT_DYNAMIC_ALLOCATION == 0)
    #error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
#endif

/*-----------------------------------------------------------*/
#if 0 //[rrq61000 matter work]
// void * pvPortCalloc(size_t num, size_t size)
{
    size_t total = num * size;
    // check for multiplication overflow
    if ((size != 0) && ((total / size) != num))
    {
        return NULL;
    }

    void * ptr = pvPortMalloc(total);
    if (ptr != NULL)
    {
        memset(ptr, 0, total);
    }

    return ptr;
}
#endif

void * pvPortRealloc(void * pv, size_t size)
{
//matterwork[[::for Renesas
    if (size == 0) {
        vPortFree(pv);
        return NULL;
    }

    void *p;
    p = pvPortMalloc(size);
    if (p) {
        /* zero the memory */
        if (pv != NULL) {
            memcpy(p, pv, size);
            vPortFree(pv);
        }
    }
    return p;
//]]matterwork
}

/*-----------------------------------------------------------*/

void xPortResetHeapMinimumEverFreeHeapSize(void)
{
//matterwork[[::not used on Renesas
//]]matterwork
}

/*
 * Wrapper functions to override default memory allocation functions
 */
void * __wrap_malloc(size_t size)
{
    return pvPortMalloc(size);
}

void __wrap_free(void * ptr)
{
    vPortFree(ptr);
}

void * __wrap_realloc(void * ptr, size_t new_size)
{
    return pvPortRealloc(ptr, new_size);
}

void * __wrap_calloc(size_t num, size_t size)
{
    return pvPortCalloc(num, size);
}

void * __wrap__calloc_r(void * REENT, size_t num, size_t size)
{
    return __wrap_calloc(num, size);
}

void * __wrap__malloc_r(void * REENT, size_t size)
{
    return __wrap_malloc(size);
}

void __wrap__free_r(void * REENT, void * ptr)
{
    __wrap_free(ptr);
}

void * __wrap__realloc_r(void * REENT, void * ptr, size_t new_size)
{
    return __wrap_realloc(ptr, new_size);
}
