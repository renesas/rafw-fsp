/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"

#if CFG_PMGR

#if CFG_WIFI
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <string.h>
#include "common_def.h"
#include "dpmty_patch.h"
#include "ra6w1_dpm_system.h"
#include "supp_def.h"
#include "rm_wifi.h"

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/err.h"

#include "rm_pmgr_w_instance.h"
#include "rm_pmgr_w_rtm_internal.h"
#include "sleep_mgmt_regs.h"
#include "bsp_dump_mem.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/* This is overridable for tests only... */
#ifndef RA6WX_DPM_RTM_MEM_ILLEGAL_FREE
    #define RA6WX_DPM_RTM_MEM_ILLEGAL_FREE(msg)         LWIP_ASSERT(msg, 0)
#endif

#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK

    #define RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE    16
    #if RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE > 0
        #define RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED    LWIP_MEM_ALIGN_SIZE(RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE)
    #else
        #define RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED    0
    #endif /* RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE */
    #ifndef RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER
        #define RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER   16
    #endif /* RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER*/
    #if RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER > 0
        #define RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED     LWIP_MEM_ALIGN_SIZE(RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER)
    #else
        #define RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED     0
    #endif /* RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER*/

    #define RA6WX_DPM_RTM_MEM_SANITY_OFFSET   RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED
    #define RA6WX_DPM_RTM_MEM_SANITY_OVERHEAD (RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED + RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED)
#else
    #define RA6WX_DPM_RTM_MEM_SANITY_OFFSET   0
    #define RA6WX_DPM_RTM_MEM_SANITY_OVERHEAD 0
#endif

/* lwIP replacement for your libc malloc() */

/**
 * The heap is made up as a list of structs of this type.
 * This does not have to be aligned since for getting its size,
 * we only use the macro SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM, which automatically aligns.
 */
struct ra6w1_dpm_rtm_mem {
    /** index (-> rtm[next]) of the next struct */
    ra6w1_dpm_rtm_mem_size_t next;
    /** index (-> rtm[prev]) of the previous struct */
    ra6w1_dpm_rtm_mem_size_t prev;
    /** 1: this area is used; 0: this area is unused */
    u8_t used;
#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
    /** this keeps track of the dpm allocation size for guard checks */
    ra6w1_dpm_rtm_mem_size_t dpm_size;
#endif
};

/** All allocated blocks will be RA6WX_DPM_RTM_MIN_SIZE bytes big, at least!
 * RA6WX_DPM_RTM_MIN_SIZE can be overridden to suit your needs. Smaller values save space,
 * larger values could prevent too small blocks to fragment the RAM too much. */
#ifndef RA6WX_DPM_RTM_MIN_SIZE
    #define RA6WX_DPM_RTM_MIN_SIZE             12
#endif /* RA6WX_DPM_RTM_MIN_SIZE */

static ra6w1_dpm_rtm_mem_size_t rtm_size;

/* some alignment macros: we define them here for better source code layout */
#define RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED        LWIP_MEM_ALIGN_SIZE(RA6WX_DPM_RTM_MIN_SIZE)
#define SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM        LWIP_MEM_ALIGN_SIZE(sizeof(struct ra6w1_dpm_rtm_mem))
#define RA6WX_DPM_RTM_MEM_SIZE_ALIGNED            LWIP_MEM_ALIGN_SIZE(rtm_size)

#if RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT

    static volatile u8_t ra6w1_dpm_rtm_mem_free_count;

    /* Allow RM_PMGR_W_rtm_heap_free from other (e.g. interrupt) context */
    #define RA6WX_DPM_RTM_MEM_FREE_DECL_PROTECT()  SYS_ARCH_DECL_PROTECT(lev_free)
    #define RA6WX_DPM_RTM_MEM_FREE_PROTECT()       SYS_ARCH_PROTECT(lev_free)
    #define RA6WX_DPM_RTM_MEM_FREE_UNPROTECT()     SYS_ARCH_UNPROTECT(lev_free)
    #define RA6WX_DPM_RTM_MEM_ALLOC_DECL_PROTECT() SYS_ARCH_DECL_PROTECT(lev_alloc)
    #define RA6WX_DPM_RTM_MEM_ALLOC_PROTECT()      SYS_ARCH_PROTECT(lev_alloc)
    #define RA6WX_DPM_RTM_MEM_ALLOC_UNPROTECT()    SYS_ARCH_UNPROTECT(lev_alloc)
    #define RA6WX_DPM_RTM_MEM_LFREE_VOLATILE       volatile

#else /* RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */

    /* Protect the heap only by using a mutex */
    #define RA6WX_DPM_RTM_MEM_FREE_DECL_PROTECT()
    #define RA6WX_DPM_RTM_MEM_FREE_PROTECT()    sys_mutex_lock(&ra6w1_dpm_rtm_mem_mutex)
    #define RA6WX_DPM_RTM_MEM_FREE_UNPROTECT()  sys_mutex_unlock(&ra6w1_dpm_rtm_mem_mutex)
    /* RM_PMGR_W_rtm_heap_malloc is protected using mutex AND RA6WX_DPM_RTM_MEM_ALLOC_PROTECT */
    #define RA6WX_DPM_RTM_MEM_ALLOC_DECL_PROTECT()
    #define RA6WX_DPM_RTM_MEM_ALLOC_PROTECT()
    #define RA6WX_DPM_RTM_MEM_ALLOC_UNPROTECT()
    #define RA6WX_DPM_RTM_MEM_LFREE_VOLATILE

#endif /* RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */

#if RA6WX_DPM_RTM_MEM_SANITY_CHECK
    static void ra6w1_dpm_rtm_mem_sanity(void);
    #define RA6WX_DPM_RTM_MEM_SANITY() ra6w1_dpm_rtm_mem_sanity()
#else
    #define RA6WX_DPM_RTM_MEM_SANITY()
#endif

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
/** pointer to the lowest free block, this is used for faster search */
static struct ra6w1_dpm_rtm_mem *RA6WX_DPM_RTM_MEM_LFREE_VOLATILE lfree;

/** pointer to the heap (rtm_heap): for alignment, rtm is now a pointer instead of an array */
static u8_t *rtm;
/** the last entry, always unused! */
static struct ra6w1_dpm_rtm_mem *rtm_end;

/** concurrent access protection */
#if !NO_SYS
    static sys_mutex_t ra6w1_dpm_rtm_mem_mutex;
#endif

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
int  ra6w1_dpm_rtm_mem_alloc_fail_cnt = 0;
int  ra6w1_dpm_rtm_mem_alloc_illegal_cnt = 0;
extern unsigned char dpm_dbg_cmd_flag;


/***********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/
static struct ra6w1_dpm_rtm_mem *ptr_to_mem(ra6w1_dpm_rtm_mem_size_t ptr);

static ra6w1_dpm_rtm_mem_size_t mem_to_ptr(void *mem);

static void ra6w1_dpm_rtm_mem_plug_holes(struct ra6w1_dpm_rtm_mem *mem);


/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
#if (dg_configUSE_RETENTION_MEM_INFO == 1)
void print_dpm_supp_key_info(dpm_supp_key_info_t    *dpm_supp_key_info_p);
void print_dpm_supp_conn_info(dpm_supp_conn_info_t  *dpm_supp_conn_info_p);
void print_dpm_supp_conn_ext_info(dpm_supp_conn_ext_info_t	*dpm_supp_conn_ext_info_p);
void print_dpm_supp_ip_info(dpm_supp_ip_info_t  *dpm_supp_ip_info_p);
void print_dpm_supp_net_info(dpm_supp_net_info_t    *dpm_supp_net_info_p);
void print_dpm_flag_info(dpm_flag_in_rtm_t  *dpm_flag_in_rtm_p);
void print_retmem_info(void);
#endif  // dg_configUSE_RETENTION_MEM_INFO
void dpmrtm_reset(struct dpm_param *dpmp, int sz);
void dpm_sche_init(struct dpm_param *dpmp);
void dpmrtm_init(struct dpm_param *dpmp);
#if (dg_configUSE_RETENTION_MEM_INFO == 1)
bool cmd_rtm_info(int argc, char *argv[]);
#endif    // dg_configUSE_RETENTION_MEM_INFO
int get_boot_mode(void);

/**
 * Zero the heap and initialize start, end and lowest-free
 */
void
RM_PMGR_W_rtm_heap_init(void *mem_ptr, size_t mem_len)
{
    struct ra6w1_dpm_rtm_mem *mem;

    LWIP_ASSERT("Sanity check alignment",
                (SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM & (MEM_ALIGNMENT - 1)) == 0);

    mem_len -= SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM;

    /* align the heap */
    rtm = (u8_t *)LWIP_MEM_ALIGN(mem_ptr);
    rtm_size = mem_len;
    /* initialize the start of the heap */
    mem = (struct ra6w1_dpm_rtm_mem *)(void *)rtm;
    mem->next = RA6WX_DPM_RTM_MEM_SIZE_ALIGNED;
    mem->prev = 0;
    mem->used = 0;
    /* initialize the end of the heap */
    rtm_end = ptr_to_mem(RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
    rtm_end->used = 1;
    rtm_end->next = RA6WX_DPM_RTM_MEM_SIZE_ALIGNED;
    rtm_end->prev = RA6WX_DPM_RTM_MEM_SIZE_ALIGNED;
    RA6WX_DPM_RTM_MEM_SANITY();

    /* initialize the lowest-free pointer to the start of the heap */
    lfree = (struct ra6w1_dpm_rtm_mem *)(void *)rtm;

    if (sys_mutex_new(&ra6w1_dpm_rtm_mem_mutex) != ERR_OK) {
        LWIP_ASSERT("failed to create ra6w1_dpm_rtm_mem_mutex", 0);
    }
}

void RM_PMGR_W_rtm_heap_recover(void *mem_ptr, size_t mem_len, void *lfree_ptr)
{
    LWIP_ASSERT("Sanity check alignment",
                (SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM & (MEM_ALIGNMENT - 1)) == 0);

    mem_len -= SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM;

    /* align the heap */
    rtm = (u8_t *)LWIP_MEM_ALIGN(mem_ptr);
    rtm_size = mem_len;

    /* initialize the end of the heap */
    rtm_end = ptr_to_mem(RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
    rtm_end->used = 1;
    rtm_end->next = RA6WX_DPM_RTM_MEM_SIZE_ALIGNED;
    rtm_end->prev = RA6WX_DPM_RTM_MEM_SIZE_ALIGNED;

    /* initialize the lowest-free pointer to the start of the heap */
    lfree = (struct ra6w1_dpm_rtm_mem *)lfree_ptr;

    if (sys_mutex_new(&ra6w1_dpm_rtm_mem_mutex) != ERR_OK) {
        LWIP_ASSERT("failed to create ra6w1_dpm_rtm_mem_mutex", 0);
    }
}

void * RM_PMGR_W_rtm_heap_free_ptr_get(void)
{
    return (void *)lfree;
}

void RM_PMGR_W_rtm_dpm_area_init(unsigned int wakeup_src)
{
    uint32_t save_fault_PC;
    uint8_t save_fault_CNT;

    //DPM_DEBUG_PRINT(3, "DPM [%s] \n", __FUNCTION__);
    /* check preamble retmemory and initialize */
    // We need to consider how to manage the contents of retmem @ POR Boot
    unsigned long nonreset_src = BSP_WAKEUP_SOURCE_GPIO |
                                    BSP_WAKEUP_SOURCE_WAKEUP_COUNTER |
                                    BSP_WAKEUP_SOURCE_WATCHDOG | BSP_WAKEUP_SOURCE_SENSOR;

    /*__CHK_DPM_ABNORM_STS__ - Do not initialize RTM for DPM Monitor */
    if ((wakeup_src & BSP_WAKEUP_SOURCE_POR)              || 
        (wakeup_src == BSP_WAKEUP_SOURCE_WAKEUP_COUNTER)  ||
        (wakeup_src == BSP_WAKEUP_RESET)                  ||
        (((wakeup_src & BSP_WAKEUP_RESET_WITH_RETENTION) == 0) && ((wakeup_src & nonreset_src) != 0))) {
        /* All application range of Retention mem cleared */
        save_fault_PC = BSP_GetFaultPc();
        save_fault_CNT = BSP_GetFaultCount();
        
#ifdef FOR_DEBUG
        printf(" [%s] Clear ALL APP(UMAC+SUPP) RTM (0x%x, 0x%x) \n",
                                    __func__,
                                    RETMEM_APP_SUPP_OFFSET,
									RA6WX_RTM_MAC_SIZE + RA6WX_RTM_APP_SUPP_SIZE);
#endif
        memset((void*) (RETMEM_APP_BASE), 0, RA6WX_RTM_MAC_SIZE + RA6WX_RTM_APP_SUPP_SIZE);

        BSP_SetFaultPc(save_fault_PC);
        BSP_SetFaultCount(save_fault_CNT);
    }
    else {
#ifdef FOR_DEBUG
        printf("Not clear RTM \n");
#endif
    }
}

bool RM_PMGR_W_rtm_exist(void)
{
	int bootType;

	/* Booting Scenario and Checkin */
	bootType = RM_PMGR_W_dpm_wakeup_src_get();

	/*
	*  0x01 : boot from extern wake up signal   sleep mode 1
	*  0x02 : boot form wake up counter     sleep mode 2
	*  0x04 : boot from power on reset
	*  0x08 :
	*  0x09 :
	*  0x0a : sleep mode 3 & DPM
	*/

	/* Sleep mode 1 or Sleep mode 2 or POR or REBOOT(8)
	 *	--> Normal full booting */

	switch (bootType) {
		case BSP_WAKEUP_SOURCE_POR :
		case BSP_WAKEUP_SOURCE_WATCHDOG :
		case BSP_WAKEUP_RESET :
		case BSP_WAKEUP_RESET_WITH_RETENTION :
		case BSP_WAKEUP_GPIO_WITH_RETENTION :
		case BSP_WAKEUP_GPIO_WAKEUP_COUNTER_WITH_RETENTION :
		case BSP_WAKEUP_COUNTER_WITH_RETENTION :
		case BSP_WAKEUP_WATCHDOG_WITH_RETENTION :
		case BSP_WAKEUP_WATCHDOG_GPIO_WITH_RETENTION :
		case BSP_WAKEUP_SENSOR_WITH_RETENTION :
		case BSP_WAKEUP_SENSOR_GPIO_WITH_RETENTION :
		case BSP_WAKEUP_SENSOR_WAKEUP_COUNTER_WITH_RETENTION :
		case BSP_WAKEUP_SENSOR_WATCHDOG_WITH_RETENTION :
		case BSP_WAKEUP_SENSOR_GPIO_WATCHDOG_WITH_RETENTION :
        case BSP_WAKEUP_SOURCE_WAKEUP_COUNTER :
        case BSP_WAKEUP_SOURCE_GPIO :
			return pdTRUE;	// Exist
		default:
			break;
	}

	return pdTRUE;	// Exist
}

/*
 * Shrink memory returned by RM_PMGR_W_rtm_heap_malloc().
 *
 * @param rmem pointer to memory allocated by RM_PMGR_W_rtm_heap_malloc the is to be shrinked
 * @param new_size required size after shrinking (needs to be smaller than or
 *                equal to the previous size)
 * @return for compatibility reasons: is always == rmem, at the moment
 *         or NULL if newsize is > old size, in which case rmem is NOT touched
 *         or freed!
 */
void * RM_PMGR_W_rtm_heap_trim(void *rmem, ra6w1_dpm_rtm_mem_size_t new_size)
{
    ra6w1_dpm_rtm_mem_size_t size, newsize;
    ra6w1_dpm_rtm_mem_size_t ptr, ptr2;
    struct ra6w1_dpm_rtm_mem *mem, *mem2;
    /* use the FREE_PROTECT here: it protects with sem OR SYS_ARCH_PROTECT */
    RA6WX_DPM_RTM_MEM_FREE_DECL_PROTECT();

    /* Expand the size of the allocated memory region so that we can
       adjust for alignment. */
    newsize = (ra6w1_dpm_rtm_mem_size_t)LWIP_MEM_ALIGN_SIZE(new_size);
    if (newsize < RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED) {
        /* every data block must be at least RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED long */
        newsize = RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED;
    }
#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
    newsize += RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED +
               RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED;
#endif
    if ((newsize > RA6WX_DPM_RTM_MEM_SIZE_ALIGNED) || (newsize < new_size)) {
        return NULL;
    }

    LWIP_ASSERT("RM_PMGR_W_rtm_heap_trim: legal memory",
                (u8_t *)rmem >= (u8_t *)rtm &&
                (u8_t *)rmem < (u8_t *)rtm_end);

    if ((u8_t *)rmem < (u8_t *)rtm || (u8_t *)rmem >= (u8_t *)rtm_end) {
        LWIP_DEBUGF(MEM_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("RM_PMGR_W_rtm_heap_trim: illegal memory\n"));
        /* protect mem stats from concurrent access */
        ra6w1_dpm_rtm_mem_alloc_illegal_cnt++;
        return rmem;
    }
    /* Get the corresponding struct ra6w1_dpm_rtm_mem ... */
    /* cast through void* to get rid of alignment warnings */
    mem = (struct ra6w1_dpm_rtm_mem *)(void *)((u8_t *)rmem -
            (SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM + RA6WX_DPM_RTM_MEM_SANITY_OFFSET));
#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
    ra6w1_dpm_rtm_mem_overflow_check_element(mem);
#endif
    /* ... and its offset pointer */
    ptr = mem_to_ptr(mem);

    size = (ra6w1_dpm_rtm_mem_size_t)((ra6w1_dpm_rtm_mem_size_t)(
                                          mem->next - ptr) - (SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM +
                                                  RA6WX_DPM_RTM_MEM_SANITY_OVERHEAD));
    LWIP_ASSERT("RM_PMGR_W_rtm_heap_trim can only shrink memory", newsize <= size);
    if (newsize > size) {
        /* not supported */
        return NULL;
    }
    if (newsize == size) {
        /* No change in size, simply return */
        return rmem;
    }

    /* protect the heap from concurrent access */
    RA6WX_DPM_RTM_MEM_FREE_PROTECT();

    mem2 = ptr_to_mem(mem->next);
    if (mem2->used == 0) {
        /* The next struct is unused, we can simply move it at little */
        ra6w1_dpm_rtm_mem_size_t next;
        LWIP_ASSERT("invalid next ptr", mem->next != RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
        /* remember the old next pointer */
        next = mem2->next;
        /* create new struct ra6w1_dpm_rtm_mem which is moved directly after the shrinked mem */
        ptr2 = (ra6w1_dpm_rtm_mem_size_t)(ptr + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM +
                                          newsize);
        if (lfree == mem2) {
            lfree = ptr_to_mem(ptr2);
        }
        mem2 = ptr_to_mem(ptr2);
        mem2->used = 0;
        /* restore the next pointer */
        mem2->next = next;
        /* link it back to mem */
        mem2->prev = ptr;
        /* link mem to it */
        mem->next = ptr2;
        /* last thing to restore linked list: as we have moved mem2,
         * let 'mem2->next->prev' point to mem2 again. but only if mem2->next is not
         * the end of the heap */
        if (mem2->next != RA6WX_DPM_RTM_MEM_SIZE_ALIGNED) {
            ptr_to_mem(mem2->next)->prev = ptr2;
        }
        /* no need to plug holes, we've already done that */
    } else if (newsize + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM +
               RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED <= size) {
        /* Next struct is used but there's room for another struct ra6w1_dpm_rtm_mem with
         * at least RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED of data.
         * Old size ('size') must be big enough to contain at least 'newsize' plus a struct ra6w1_dpm_rtm_mem
         * ('SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM') with some data ('RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED').
         * @todo we could leave out RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED. We would create an empty
         *       region that couldn't hold data, but when mem->next gets freed,
         *       the 2 regions would be combined, resulting in more free memory */
        ptr2 = (ra6w1_dpm_rtm_mem_size_t)(ptr + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM +
                                          newsize);
        LWIP_ASSERT("invalid next ptr", mem->next != RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
        mem2 = ptr_to_mem(ptr2);
        if (mem2 < lfree) {
            lfree = mem2;
        }
        mem2->used = 0;
        mem2->next = mem->next;
        mem2->prev = ptr;
        mem->next = ptr2;
        if (mem2->next != RA6WX_DPM_RTM_MEM_SIZE_ALIGNED) {
            ptr_to_mem(mem2->next)->prev = ptr2;
        }
        /* the original mem->next is used, so no need to plug holes! */
    }
    /* else {
       next struct ra6w1_dpm_rtm_mem is used but size between mem and mem2 is not big enough
       to create another struct ra6w1_dpm_rtm_mem
       -> don't do anyhting.
       -> the remaining space stays unused since it is too small
       } */
#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
    ra6w1_dpm_rtm_mem_overflow_init_element(mem, new_size);
#endif
    RA6WX_DPM_RTM_MEM_SANITY();
#if RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
    ra6w1_dpm_rtm_mem_free_count = 1;
#endif /* RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */
    RA6WX_DPM_RTM_MEM_FREE_UNPROTECT();
    return rmem;
}

/*
 * Allocate a block of memory with a minimum of 'size' bytes.
 *
 * @param size_in is the minimum size of the requested block in bytes.
 * @return pointer to allocated memory or NULL if no free memory was found.
 *
 * Note that the returned value will always be aligned (as defined by MEM_ALIGNMENT).
 */
void * RM_PMGR_W_rtm_heap_malloc(ra6w1_dpm_rtm_mem_size_t size_in)
{
    ra6w1_dpm_rtm_mem_size_t ptr, ptr2, size;
    struct ra6w1_dpm_rtm_mem *mem, *mem2;
#if RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
    u8_t local_mem_free_count = 0;
#endif /* RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */
    RA6WX_DPM_RTM_MEM_ALLOC_DECL_PROTECT();

    if (size_in == 0) {
        return NULL;
    }

    /* Expand the size of the allocated memory region so that we can
       adjust for alignment. */
    size = (ra6w1_dpm_rtm_mem_size_t)LWIP_MEM_ALIGN_SIZE(size_in);
    if (size < RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED) {
        /* every data block must be at least RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED long */
        size = RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED;
    }
#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
    size += RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED +
            RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED;
#endif
    if ((size > RA6WX_DPM_RTM_MEM_SIZE_ALIGNED) || (size < size_in)) {
        return NULL;
    }

    /* protect the heap from concurrent access */
    sys_mutex_lock(&ra6w1_dpm_rtm_mem_mutex);
    RA6WX_DPM_RTM_MEM_ALLOC_PROTECT();
#if RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
    /* run as long as a RM_PMGR_W_rtm_heap_free disturbed RM_PMGR_W_rtm_heap_malloc or RM_PMGR_W_rtm_heap_trim */
    do {
        local_mem_free_count = 0;
#endif /* RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */

        /* Scan through the heap searching for a free block that is big enough,
         * beginning with the lowest free block.
         */
        for (ptr = mem_to_ptr(lfree); ptr < RA6WX_DPM_RTM_MEM_SIZE_ALIGNED - size;
             ptr = ptr_to_mem(ptr)->next) {
            mem = ptr_to_mem(ptr);
#if RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
            ra6w1_dpm_rtm_mem_free_count = 0;
            RA6WX_DPM_RTM_MEM_ALLOC_UNPROTECT();

            /* allow ra6w1_dpm_rtm_mem_free_count or RM_PMGR_W_rtm_heap_trim to run */
            RA6WX_DPM_RTM_MEM_ALLOC_PROTECT();
            if (ra6w1_dpm_rtm_mem_free_count != 0) {
                /* If ra6w1_dpm_rtm_mem_free_count or RM_PMGR_W_rtm_heap_trim have run, we have to restart since they
                   could have altered our current struct ra6w1_dpm_rtm_mem. */
                local_mem_free_count = 1;
                break;
            }
#endif /* RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */

            if ((!mem->used) &&
                (mem->next - (ptr + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM)) >= size) {
                /* mem is not used and at least perfect fit is possible:
                 * mem->next - (ptr + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM) gives us the 'dpm data size' of mem */

                if (mem->next - (ptr + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM) >=
                    (size + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM +
                     RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED)) {
                    /* (in addition to the above, we test if another struct ra6w1_dpm_rtm_mem (SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM) containing
                     * at least RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED of data also fits in the 'dpm data space' of 'mem')
                     * -> split large block, create empty remainder,
                     * remainder must be large enough to contain RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED data: if
                     * mem->next - (ptr + (2*SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM)) == size,
                     * struct ra6w1_dpm_rtm_mem would fit in but no data between mem2 and mem2->next
                     * @todo we could leave out RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED. We would create an empty
                     *       region that couldn't hold data, but when mem->next gets freed,
                     *       the 2 regions would be combined, resulting in more free memory
                     */
                    ptr2 = (ra6w1_dpm_rtm_mem_size_t)(ptr + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM + size);
                    LWIP_ASSERT("invalid next ptr", ptr2 != RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
                    /* create mem2 struct */
                    mem2 = ptr_to_mem(ptr2);
                    mem2->used = 0;
                    mem2->next = mem->next;
                    mem2->prev = ptr;
                    /* and insert it between mem and mem->next */
                    mem->next = ptr2;
                    mem->used = 1;

                    if (mem2->next != RA6WX_DPM_RTM_MEM_SIZE_ALIGNED) {
                        ptr_to_mem(mem2->next)->prev = ptr2;
                    }
                } else {
                    /* (a mem2 struct does no fit into the dpm data space of mem and mem->next will always
                     * be used at this point: if not we have 2 unused structs in a row, ra6w1_dpm_rtm_mem_plug_holes should have
                     * take care of this).
                     * -> near fit or exact fit: do not split, no mem2 creation
                     * also can't move mem->next directly behind mem, since mem->next
                     * will always be used at this point!
                     */
                    mem->used = 1;
                }
#if RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
mem_malloc_adjust_lfree:
#endif /* RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */
                if (mem == lfree) {
                    struct ra6w1_dpm_rtm_mem *cur = lfree;
                    /* Find next free block after mem and update lowest free pointer */
                    while (cur->used && cur != rtm_end) {
#if RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
                        ra6w1_dpm_rtm_mem_free_count = 0;
                        RA6WX_DPM_RTM_MEM_ALLOC_UNPROTECT();
                        /* prevent high interrupt latency... */
                        RA6WX_DPM_RTM_MEM_ALLOC_PROTECT();
                        if (ra6w1_dpm_rtm_mem_free_count != 0) {
                            /* If ra6w1_dpm_rtm_mem_free_count or RM_PMGR_W_rtm_heap_trim have run, we have to restart since they
                               could have altered our current struct ra6w1_dpm_rtm_mem or lfree. */
                            goto mem_malloc_adjust_lfree;
                        }
#endif /* RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */
                        cur = ptr_to_mem(cur->next);
                    }
                    lfree = cur;
                    LWIP_ASSERT("RM_PMGR_W_rtm_heap_malloc: !lfree->used", ((lfree == rtm_end)
                                || (!lfree->used)));
                }
                RA6WX_DPM_RTM_MEM_ALLOC_UNPROTECT();
                sys_mutex_unlock(&ra6w1_dpm_rtm_mem_mutex);
                LWIP_ASSERT("RM_PMGR_W_rtm_heap_malloc: allocated memory not above rtm_end.",
                            (mem_ptr_t)mem + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM + size <= (mem_ptr_t)rtm_end);
                LWIP_ASSERT("RM_PMGR_W_rtm_heap_malloc: allocated memory properly aligned.",
                            ((mem_ptr_t)mem + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM) % MEM_ALIGNMENT == 0);
                LWIP_ASSERT("RM_PMGR_W_rtm_heap_malloc: sanity check alignment",
                            (((mem_ptr_t)mem) & (MEM_ALIGNMENT - 1)) == 0);

#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
                ra6w1_dpm_rtm_mem_overflow_init_element(mem, size_in);
#endif
                RA6WX_DPM_RTM_MEM_SANITY();
                return (u8_t *)mem + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM +
                       RA6WX_DPM_RTM_MEM_SANITY_OFFSET;
            }
        }
#if RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
        /* if we got interrupted by a ra6w1_dpm_rtm_mem_free_count, try again */
    } while (local_mem_free_count != 0);
#endif /* RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */
    ra6w1_dpm_rtm_mem_alloc_fail_cnt++;
    RA6WX_DPM_RTM_MEM_ALLOC_UNPROTECT();
    sys_mutex_unlock(&ra6w1_dpm_rtm_mem_mutex);
    //LWIP_DEBUGF(MEM_DEBUG | LWIP_DBG_LEVEL_SERIOUS, "RM_PMGR_W_rtm_heap_malloc: could not allocate %"S16_F" bytes\n", (s16_t)size);
    return NULL;
}

/*
 * Contiguously allocates enough space for count objects that are size bytes
 * of memory each and returns a pointer to the allocated memory.
 *
 * The allocated memory is filled with bytes of value zero.
 *
 * @param count number of objects to allocate
 * @param size size of the objects to allocate
 * @return pointer to allocated memory / NULL pointer if there is an error
 */
void * RM_PMGR_W_rtm_heap_calloc(ra6w1_dpm_rtm_mem_size_t count,
                         ra6w1_dpm_rtm_mem_size_t size)
{
    void *p;
    size_t alloc_size = (size_t)count * (size_t)size;

    if ((size_t)(ra6w1_dpm_rtm_mem_size_t)alloc_size != alloc_size) {
        LWIP_DEBUGF(MEM_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
                    ("RM_PMGR_W_rtm_heap_calloc: could not allocate %"SZT_F" bytes\n", alloc_size));
        return NULL;
    }

    /* allocate 'count' objects of size 'size' */
    p = RM_PMGR_W_rtm_heap_malloc((ra6w1_dpm_rtm_mem_size_t)alloc_size);
    if (p) {
        /* zero the memory */
        memset(p, 0, alloc_size);
    }
    return p;
}

/* Check if a struct ra6w1_dpm_rtm_mem is correctly linked.
 * If not, double-free is a possible reason.
 */
static int
ra6w1_dpm_rtm_mem_link_valid(struct ra6w1_dpm_rtm_mem *mem)
{
    struct ra6w1_dpm_rtm_mem *nmem, *pmem;
    ra6w1_dpm_rtm_mem_size_t rmem_idx;

    rmem_idx = mem_to_ptr(mem);
    nmem = ptr_to_mem(mem->next);
    pmem = ptr_to_mem(mem->prev);
    if (   (mem->next > RA6WX_DPM_RTM_MEM_SIZE_ALIGNED)
        || (mem->prev > RA6WX_DPM_RTM_MEM_SIZE_ALIGNED)
        || ((mem->prev != rmem_idx) && (pmem->next != rmem_idx))
        || ((nmem != rtm_end) && (nmem->prev != rmem_idx))) {
        return 0;
    }

    return 1;
}


/*
 * Put a struct ra6w1_dpm_rtm_mem back on the heap
 *
 * @param rmem is the data portion of a struct ra6w1_dpm_rtm_mem as returned by a previous
 *             call to RM_PMGR_W_rtm_heap_malloc()
 */
void
RM_PMGR_W_rtm_heap_free(void *rmem)
{
    struct ra6w1_dpm_rtm_mem *mem;
    RA6WX_DPM_RTM_MEM_FREE_DECL_PROTECT();

    if (rmem == NULL) {
        LWIP_DEBUGF(MEM_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
                    ("RM_PMGR_W_rtm_heap_free(p == NULL) was called.\n"));
        return;
    }
    if ((((mem_ptr_t)rmem) & (MEM_ALIGNMENT - 1)) != 0) {
        RA6WX_DPM_RTM_MEM_ILLEGAL_FREE("RM_PMGR_W_rtm_heap_free: sanity check alignment");
        LWIP_DEBUGF(MEM_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("RM_PMGR_W_rtm_heap_free: sanity check alignment\n"));
        /* protect mem stats from concurrent access */
        ra6w1_dpm_rtm_mem_alloc_illegal_cnt++;
        return;
    }

    /* Get the corresponding struct ra6w1_dpm_rtm_mem: */
    /* cast through void* to get rid of alignment warnings */
    mem = (struct ra6w1_dpm_rtm_mem *)(void *)((u8_t *)rmem -
            (SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM + RA6WX_DPM_RTM_MEM_SANITY_OFFSET));

    if ((u8_t *)mem < rtm
        || (u8_t *)rmem + RA6WX_DPM_RTM_MEM_MIN_SIZE_ALIGNED > (u8_t *)rtm_end) {
        RA6WX_DPM_RTM_MEM_ILLEGAL_FREE("RM_PMGR_W_rtm_heap_free: illegal memory");
        LWIP_DEBUGF(MEM_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("RM_PMGR_W_rtm_heap_free: illegal memory\n"));
        /* protect mem stats from concurrent access */
        ra6w1_dpm_rtm_mem_alloc_illegal_cnt++;
        return;
    }
#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
    ra6w1_dpm_rtm_mem_overflow_check_element(mem);
#endif
    /* protect the heap from concurrent access */
    RA6WX_DPM_RTM_MEM_FREE_PROTECT();
    /* mem has to be in a used state */
    if (!mem->used) {
        RA6WX_DPM_RTM_MEM_ILLEGAL_FREE("RM_PMGR_W_rtm_heap_free: illegal memory: double free");
        RA6WX_DPM_RTM_MEM_FREE_UNPROTECT();
        LWIP_DEBUGF(MEM_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("RM_PMGR_W_rtm_heap_free: illegal memory: double free?\n"));
        /* protect mem stats from concurrent access */
        ra6w1_dpm_rtm_mem_alloc_illegal_cnt++;
        return;
    }

    if (!ra6w1_dpm_rtm_mem_link_valid(mem)) {
        RA6WX_DPM_RTM_MEM_ILLEGAL_FREE("RM_PMGR_W_rtm_heap_free: illegal memory: non-linked: double free");
        RA6WX_DPM_RTM_MEM_FREE_UNPROTECT();
        LWIP_DEBUGF(MEM_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("RM_PMGR_W_rtm_heap_free: illegal memory: non-linked: double free?\n"));
        /* protect mem stats from concurrent access */
        ra6w1_dpm_rtm_mem_alloc_illegal_cnt++;
        return;
    }

    //PRINTF(GREEN_COLOR " [%s] mem: 0x%x used:%d next:0x%x prev:0x%x \r\n" CLEAR_COLOR,
    //                                    __func__, mem, mem->used, mem->next, mem->prev);

    /* mem is now unused. */
    mem->used = 0;

    if (mem < lfree) {
        /* the newly freed struct is now the lowest */
        lfree = mem;
    }

    /* finally, see if prev or next are free also */
    ra6w1_dpm_rtm_mem_plug_holes(mem);
    RA6WX_DPM_RTM_MEM_SANITY();
#if RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
    ra6w1_dpm_rtm_mem_free_count = 1;
#endif /* RA6WX_DPM_RTM_MEM_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */
    RA6WX_DPM_RTM_MEM_FREE_UNPROTECT();
}

void RM_PMGR_W_rtm_heap_status_print()
{
    ra6w1_dpm_rtm_mem_size_t ptr;
    struct ra6w1_dpm_rtm_mem *mem;
    RA6WX_DPM_RTM_MEM_ALLOC_DECL_PROTECT();
    ra6w1_dpm_rtm_mem_size_t used_mem = 0;
    ra6w1_dpm_rtm_mem_size_t free_mem = 0;
    ra6w1_dpm_rtm_mem_size_t node_count = 0;

    PRINTF("\r\n");

    /* Scan through the heap searching for a free block that is big enough,
     * beginning with the lowest free block.
     */
    PRINTF(CYAN_COLOR "\r\n << DPM RTM MEM STATUS >> \r\n" CLEAR_COLOR);

    /* protect the heap from concurrent access */
    sys_mutex_lock(&ra6w1_dpm_rtm_mem_mutex);
    RA6WX_DPM_RTM_MEM_ALLOC_PROTECT();

    for (ptr = mem_to_ptr(rtm); ptr < RA6WX_DPM_RTM_MEM_SIZE_ALIGNED;
         ptr = ptr_to_mem(ptr)->next) {
        mem = ptr_to_mem(ptr);
        PRINTF(" mem: %p used:%d next:0x%x prev:0x%x size:%d \r\n", mem, mem->used,
               mem->next, mem->prev, mem->next - (ptr + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM));

        node_count++;

        if (!mem->used)  {
            free_mem += (mem->next - (ptr + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM));
        } else if (mem->used) {
            used_mem += (mem->next - (ptr + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM));
        } else {
            PRINTF(RED_COLOR " [%s] Fail check \r\n" CLEAR_COLOR, __func__);
        }
    }

    RA6WX_DPM_RTM_MEM_ALLOC_UNPROTECT();
    sys_mutex_unlock(&ra6w1_dpm_rtm_mem_mutex);

    PRINTF(CYAN_COLOR " Total mem  : %d \r\n" CLEAR_COLOR, RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
    PRINTF(CYAN_COLOR " node cnt   : %d \r\n" CLEAR_COLOR, node_count);
    PRINTF(CYAN_COLOR " error_cnt  : %d \r\n" CLEAR_COLOR,
           ra6w1_dpm_rtm_mem_alloc_fail_cnt);
    PRINTF(CYAN_COLOR " illegal cnt: %d \r\n" CLEAR_COLOR,
           ra6w1_dpm_rtm_mem_alloc_illegal_cnt);
    PRINTF(CYAN_COLOR " Used_mem   : %d \r\n" CLEAR_COLOR, used_mem);
    PRINTF(CYAN_COLOR " Free mem   : %d \r\n" CLEAR_COLOR, free_mem);

    return;
}

#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
/**
 * Check if a mep element was victim of an overflow or underflow
 * (e.g. the restricted area after/before it has been altered)
 *
 * @param p the mem element to check
 * @param size allocated size of the element
 * @param descr1 description of the element source shown on error
 * @param descr2 description of the element source shown on error
 */
void
RM_PMGR_W_rtm_heap_overflow_check(void *p, size_t size, const char *descr1,
                                     const char *descr2)
{
#if RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED || RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED
    u16_t k;
    u8_t *m;

#if RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED > 0
    m = (u8_t *)p + size;
    for (k = 0; k < RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED; k++) {
        if (m[k] != 0xcd) {
            char errstr[128];
            snprintf(errstr, sizeof(errstr), "detected mem overflow in %s%s", descr1,
                     descr2);
            LWIP_ASSERT(errstr, 0);
        }
    }
#endif /* RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED > 0 */

#if RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED > 0
    m = (u8_t *)p - RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED;
    for (k = 0; k < RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED; k++) {
        if (m[k] != 0xcd) {
            char errstr[128];
            snprintf(errstr, sizeof(errstr), "detected mem underflow in %s%s", descr1,
                     descr2);
            LWIP_ASSERT(errstr, 0);
        }
    }
#endif /* RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED > 0 */
#else
    LWIP_UNUSED_ARG(p);
    LWIP_UNUSED_ARG(desc);
    LWIP_UNUSED_ARG(descr);
#endif
}

/*
 * Initialize the restricted area of a mem element.
 */
void
RM_PMGR_W_rtm_heap_init_raw_with_overflow_check(void *p, size_t size)
{
#if RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED > 0 || RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED > 0
    u8_t *m;
#if RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED > 0
    m = (u8_t *)p - RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED;
    memset(m, 0xcd, RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED);
#endif
#if RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED > 0
    m = (u8_t *)p + size;
    memset(m, 0xcd, RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED);
#endif
#else /* RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED > 0 || RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED > 0 */
    LWIP_UNUSED_ARG(p);
    LWIP_UNUSED_ARG(desc);
#endif /* RA6WX_DPM_RTM_MEM_SANITY_REGION_BEFORE_ALIGNED > 0 || RA6WX_DPM_RTM_MEM_SANITY_REGION_AFTER_ALIGNED > 0 */
}
#endif /* RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK */


#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
static void
ra6w1_dpm_rtm_mem_overflow_init_element(struct ra6w1_dpm_rtm_mem *mem,
                                        ra6w1_dpm_rtm_mem_size_t dpm_size)
{
    void *p = (u8_t *)mem + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM +
              RA6WX_DPM_RTM_MEM_SANITY_OFFSET;
    mem->dpm_size = dpm_size;
    RM_PMGR_W_rtm_heap_init_raw_with_overflow_check(p, dpm_size);
}

static void
ra6w1_dpm_rtm_mem_overflow_check_element(struct ra6w1_dpm_rtm_mem *mem)
{
    void *p = (u8_t *)mem + SIZEOF_STRUCT_RA6WX_DPM_RTM_MEM +
              RA6WX_DPM_RTM_MEM_SANITY_OFFSET;
    RM_PMGR_W_rtm_heap_overflow_check(p, mem->dpm_size, "heap", "");
}
#else /* RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK */
#define ra6w1_dpm_rtm_mem_overflow_init_element(mem, size)
#define ra6w1_dpm_rtm_mem_overflow_check_element(mem)
#endif /* RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK */

static struct ra6w1_dpm_rtm_mem *ptr_to_mem(ra6w1_dpm_rtm_mem_size_t ptr)
{
    return (struct ra6w1_dpm_rtm_mem *)(void *)&rtm[ptr];
}

static ra6w1_dpm_rtm_mem_size_t mem_to_ptr(void *mem)
{
    return (ra6w1_dpm_rtm_mem_size_t)((u8_t *)mem - rtm);
}

/*
 * "Plug holes" by combining adjacent empty struct ra6w1_dpm_rtm_mems.
 * After this function is through, there should not exist
 * one empty struct ra6w1_dpm_rtm_mem pointing to another empty struct ra6w1_dpm_rtm_mem.
 *
 * @param mem this points to a struct ra6w1_dpm_rtm_mem which just has been freed
 * @internal this function is only called by RM_PMGR_W_rtm_heap_free() and RM_PMGR_W_rtm_heap_trim()
 *
 * This assumes access to the heap is protected by the calling function
 * already.
 */
static void
ra6w1_dpm_rtm_mem_plug_holes(struct ra6w1_dpm_rtm_mem *mem)
{
    struct ra6w1_dpm_rtm_mem *nmem;
    struct ra6w1_dpm_rtm_mem *pmem;

    LWIP_ASSERT("ra6w1_dpm_rtm_mem_plug_holes: mem >= rtm", (u8_t *)mem >= rtm);
    LWIP_ASSERT("ra6w1_dpm_rtm_mem_plug_holes: mem < rtm_end",
                (u8_t *)mem < (u8_t *)rtm_end);
    LWIP_ASSERT("ra6w1_dpm_rtm_mem_plug_holes: mem->used == 0", mem->used == 0);

    /* plug hole forward */
    LWIP_ASSERT("ra6w1_dpm_rtm_mem_plug_holes: mem->next <= RA6WX_DPM_RTM_MEM_SIZE_ALIGNED",
                mem->next <= RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);

    nmem = ptr_to_mem(mem->next);
    if (mem != nmem && nmem->used == 0 && (u8_t *)nmem != (u8_t *)rtm_end) {
        /* if mem->next is unused and not end of rtm, combine mem and mem->next */
        if (lfree == nmem) {
            lfree = mem;
        }
        mem->next = nmem->next;
        if (nmem->next != RA6WX_DPM_RTM_MEM_SIZE_ALIGNED) {
            ptr_to_mem(nmem->next)->prev = mem_to_ptr(mem);
        }
    }

    /* plug hole backward */
    pmem = ptr_to_mem(mem->prev);
    if (pmem != mem && pmem->used == 0) {
        /* if mem->prev is unused, combine mem and mem->prev */
        if (lfree == mem) {
            lfree = pmem;
        }
        pmem->next = mem->next;
        if (mem->next != RA6WX_DPM_RTM_MEM_SIZE_ALIGNED) {
            ptr_to_mem(mem->next)->prev = mem_to_ptr(pmem);
        }
    }
}


#if RA6WX_DPM_RTM_MEM_SANITY_CHECK
static void
ra6w1_dpm_rtm_mem_sanity(void)
{
    struct ra6w1_dpm_rtm_mem *mem;
    u8_t last_used;

    /* begin with first element here */
    mem = (struct ra6w1_dpm_rtm_mem *)rtm;
    LWIP_ASSERT("heap element used valid", (mem->used == 0) || (mem->used == 1));
    last_used = mem->used;
    LWIP_ASSERT("heap element prev ptr valid", mem->prev == 0);
    LWIP_ASSERT("heap element next ptr valid",
                mem->next <= RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
    LWIP_ASSERT("heap element next ptr aligned",
                LWIP_MEM_ALIGN(ptr_to_mem(mem->next) == ptr_to_mem(mem->next)));

    /* check all elements before the end of the heap */
    for (mem = ptr_to_mem(mem->next); ((u8_t *)mem > rtm) && (mem < rtm_end); mem = ptr_to_mem(mem->next)) {
        LWIP_ASSERT("heap element aligned", LWIP_MEM_ALIGN(mem) == mem);
        LWIP_ASSERT("heap element prev ptr valid",
                    mem->prev <= RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
        LWIP_ASSERT("heap element next ptr valid",
                    mem->next <= RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
        LWIP_ASSERT("heap element prev ptr aligned",
                    LWIP_MEM_ALIGN(ptr_to_mem(mem->prev) == ptr_to_mem(mem->prev)));
        LWIP_ASSERT("heap element next ptr aligned",
                    LWIP_MEM_ALIGN(ptr_to_mem(mem->next) == ptr_to_mem(mem->next)));

        if (last_used == 0) {
            /* 2 unused elements in a row? */
            LWIP_ASSERT("heap element unused?", mem->used == 1);
        } else {
            LWIP_ASSERT("heap element unused member", (mem->used == 0) || (mem->used == 1));
        }

        LWIP_ASSERT("heap element link valid", ra6w1_dpm_rtm_mem_link_valid(mem));

        /* used/unused altering */
        last_used = mem->used;
    }
    LWIP_ASSERT("heap end ptr sanity",
                mem == ptr_to_mem(RA6WX_DPM_RTM_MEM_SIZE_ALIGNED));
    LWIP_ASSERT("heap element used valid", mem->used == 1);
    LWIP_ASSERT("heap element prev ptr valid",
                mem->prev == RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
    LWIP_ASSERT("heap element next ptr valid",
                mem->next == RA6WX_DPM_RTM_MEM_SIZE_ALIGNED);
}
#endif /* RA6WX_DPM_RTM_MEM_SANITY_CHECK */


#if (dg_configUSE_RETENTION_MEM_INFO == 1)
void print_dpm_supp_key_info(dpm_supp_key_info_t	*dpm_supp_key_info_p)
{
	printf("   proto                   : 0x%x\n", dpm_supp_key_info_p->proto);
	printf("   key_mgmt                : 0x%x\n", dpm_supp_key_info_p->key_mgmt);
	printf("   pairwise_cipher         : 0x%x\n", dpm_supp_key_info_p->pairwise_cipher);
	printf("   group_cipher            : 0x%x\n", dpm_supp_key_info_p->group_cipher);
	printf("   pmk_len                 : 0x%x\n", dpm_supp_key_info_p->pmk_len);
	printf("   wep_key_len             : 0x%x\n", dpm_supp_key_info_p->wep_key_len);
	printf("   wep_tx_keyidx           : 0x%x\n", dpm_supp_key_info_p->wep_tx_keyidx);
	if (dpm_supp_key_info_p->wep_key_len > 0) {
		printf("   wep_key                 : %s\n", dpm_supp_key_info_p->wep_key);
	}
	printf("   ptk(wpa_alg)            : 0x%x\n", dpm_supp_key_info_p->ptk.wpa_alg);
	printf("   ptk(key_idx)            : 0x%x\n", dpm_supp_key_info_p->ptk.key_idx);
	printf("   ptk(set_tx)             : 0x%x\n", dpm_supp_key_info_p->ptk.set_tx);
	printf("   gtk(wpa_alg)            : 0x%x\n", dpm_supp_key_info_p->gtk.wpa_alg);
	printf("   gtk(key_idx)            : 0x%x\n", dpm_supp_key_info_p->gtk.key_idx);
	printf("   gtk(set_tx)             : 0x%x\n", dpm_supp_key_info_p->gtk.set_tx);
}

void print_dpm_supp_conn_info(dpm_supp_conn_info_t	*dpm_supp_conn_info_p)
{
	printf("   mode                    : 0x%x\n", dpm_supp_conn_info_p->mode);
	printf("   disabled                : 0x%x\n", dpm_supp_conn_info_p->disabled);
	printf("   id                      : 0x%x\n", dpm_supp_conn_info_p->id);
	printf("   ssid_len                : 0x%x\n", dpm_supp_conn_info_p->ssid_len);
	printf("   scan_ssid               : 0x%x\n", dpm_supp_conn_info_p->scan_ssid);
	printf("   psk_set                 : 0x%x\n", dpm_supp_conn_info_p->psk_set);
	printf("   auth_alg                : 0x%x\n", dpm_supp_conn_info_p->auth_alg);
	//printf("   bssid                   : %s\n", dpm_supp_conn_info_p->bssid);
 #ifdef DEF_SAVE_DPM_WIFI_MODE
    printf("   wifi_mode               : 0x%x\n", dpm_supp_conn_info_p->wifi_mode);
    printf("   dpm_opt                 : 0x%x\n", dpm_supp_conn_info_p->dpm_opt);
 
 #ifdef __SUPPORT_IEEE80211W__
    printf("   pmf                     : 0x%x\n", dpm_supp_conn_info_p->pmf);
 #endif    // __SUPPORT_IEEE80211W__
 #else    // DEF_SAVE_DPM_WIFI_MODE
 #ifdef __SUPPORT_IEEE80211W__
    printf("   pmf                     : 0x%x\n", dpm_supp_conn_info_p->pmf);
 #endif    // __SUPPORT_IEEE80211W__
 #endif    // DEF_SAVE_DPM_WIFI_MODE
}

void print_dpm_supp_conn_ext_info(dpm_supp_conn_ext_info_t	*dpm_supp_conn_ext_info_p)
{
    unsigned int idx = 0;

    printf("   ieee80211w              : %d\n", dpm_supp_conn_ext_info_p->ieee80211w);
    printf("   sae_groups              : ");
    for (idx = 0; idx < DPM_MAX_SAE_GROUPS; idx++) {
        printf("%d ", dpm_supp_conn_ext_info_p->sae_groups[idx]);
    }
    printf("\n");
    printf("   sae_password            : %s\n", dpm_supp_conn_ext_info_p->sae_password);

    printf("   identity                : %d\n", dpm_supp_conn_ext_info_p->identity_len);
    for (idx = 0; idx < dpm_supp_conn_ext_info_p->identity_len; idx++) {
        printf("   0x%02x ", dpm_supp_conn_ext_info_p->identity[idx]);
    }
    printf("\n");

    printf("   password                : %d\n", dpm_supp_conn_ext_info_p->password_len);
    for (idx = 0; idx < dpm_supp_conn_ext_info_p->password_len; idx++) {
        printf("   0x%02x ", dpm_supp_conn_ext_info_p->password[idx]);
    }
    printf("\n");
}

void print_dpm_supp_ip_info(dpm_supp_ip_info_t	*dpm_supp_ip_info_p)
{
	printf("   dpm_dhcp_xid            : 0x%lx\n", dpm_supp_ip_info_p->dpm_dhcp_xid);
	printf("   dpm_ip_addr             : 0x%lx\n", dpm_supp_ip_info_p->dpm_ip_addr);
	printf("   dpm_netmask             : 0x%lx\n", dpm_supp_ip_info_p->dpm_netmask);
	printf("   dpm_gateway             : 0x%lx\n", dpm_supp_ip_info_p->dpm_gateway);
	printf("   dpm_dns_addr0           : 0x%lx\n", dpm_supp_ip_info_p->dpm_dns_addr[0]);
	printf("   dpm_dns_addr1           : 0x%lx\n", dpm_supp_ip_info_p->dpm_dns_addr[1]);
	printf("   dpm_lease               : 0x%lx\n", dpm_supp_ip_info_p->dpm_lease);
	printf("   dpm_renewal             : 0x%lx\n", dpm_supp_ip_info_p->dpm_renewal);
	printf("   dpm_timeout             : 0x%lx\n", dpm_supp_ip_info_p->dpm_timeout);
	printf("   dpm_dhcp_svr_ip         : 0x%lx\n", dpm_supp_ip_info_p->dpm_dhcp_server_ip);
}

void print_dpm_supp_net_info(dpm_supp_net_info_t	*dpm_supp_net_info_p)
{
	printf("   net_mode                : 0x%x\n", dpm_supp_net_info_p->net_mode);
	printf("   wifi_mode               : 0x%x\n", dpm_supp_net_info_p->wifi_mode);
	printf("   country                 : %s\n", dpm_supp_net_info_p->country);
}

void print_dpm_flag_info(dpm_flag_in_rtm_t	*dpm_flag_in_rtm_p)
{
	printf("   dpm_mode                : 0x%x\n", dpm_flag_in_rtm_p->dpm_mode);
	printf("   dpm_wakeup              : 0x%x\n", dpm_flag_in_rtm_p->dpm_wakeup);
	printf("   dpm_sleepd_stop         : 0x%x\n", dpm_flag_in_rtm_p->dpm_sleepd_stop);
	printf("   dpm_rtc_timeout_flag    : 0x%x\n", dpm_flag_in_rtm_p->dpm_rtc_timeout_flag);
	printf("   dpm_rtc_timeout_tid     : 0x%x\n", dpm_flag_in_rtm_p->dpm_rtc_timeout_tid);
	printf("   dpm_keepalive_time_msec : 0x%x\n", dpm_flag_in_rtm_p->dpm_keepalive_time_msec);
	printf("   dpm_supp_state          : 0x%x\n", dpm_flag_in_rtm_p->dpm_supp_state);
	printf("   dpm_timezone            : 0x%lx\n", dpm_flag_in_rtm_p->time_params.__timezone);
	printf("   systime_offset          : 0x%llx\n", dpm_flag_in_rtm_p->time_params.systime_offset);
	printf("   rtc_oldtime             : 0x%llx\n", dpm_flag_in_rtm_p->time_params.rtc_oldtime);
	printf("   dpm_sntp_use            : 0x%x\n", dpm_flag_in_rtm_p->dpm_sntp_use);
	printf("   dpm_sntp_period         : 0x%lx\n", dpm_flag_in_rtm_p->dpm_sntp_period);
	printf("   dpm_sntp_timeout        : 0x%lx\n", dpm_flag_in_rtm_p->dpm_sntp_timeout);
	printf("   dpm_dbg_level           : 0x%x\n", dpm_flag_in_rtm_p->dpm_dbg_level);
}

void print_retmem_info(void)
{
	// OLD DPM dpm_umac_info_t			*dpm_umac_info_p = RETM_MAC_BASE;
	dpm_flag_in_rtm_t	*dpm_flag_in_rtm_p = (dpm_flag_in_rtm_t *)RTM_FLAG_BASE;
	dpm_supp_net_info_t	*dpm_supp_net_info_p = (dpm_supp_net_info_t *)RTM_SUPP_NET_INFO_BASE;
	dpm_supp_ip_info_t	*dpm_supp_ip_info_p = (dpm_supp_ip_info_t *)RTM_SUPP_IP_INFO_BASE;
	dpm_supp_conn_info_t	*dpm_supp_conn_info_p = (dpm_supp_conn_info_t *)RTM_SUPP_CONN_INFO_BASE;
	dpm_supp_conn_ext_info_t *dpm_supp_conn_ext_info_p = (dpm_supp_conn_ext_info_t *)RTM_SUPP_CONN_EXT_INFO_BASE;
	dpm_supp_key_info_t	*dpm_supp_key_info_p = (dpm_supp_key_info_t *)RTM_SUPP_KEY_INFO_BASE;

	printf("\n << DPM RETENTION MEMORY CONFIGURATION >>\n");

	printf(" RETMEM_APP_BASE        : 0x%x\n", RETMEM_APP_BASE);
	// TODO: printf(" RETMEM_APP_MAC_OFFSET : 0x%x, size:0x%x \n", dpm_umac_info_p, MAC_ALLOC_SZ);
	// TODO: print_dpm_umac_info(dpm_umac_info_p);
	extern void print_dpm_mac(void);
	print_dpm_mac();

	printf(" RETMEM_APP_SUPP_OFFSET : 0x%x, size:0x%x\n", RETMEM_APP_SUPP_OFFSET, SUPP_ALLOC_SZ);

	printf("\n RTM_FLAG_BASE          : %p, size:0x%x\n", dpm_flag_in_rtm_p, FLAG_ALLOC_SZ);
	print_dpm_flag_info(dpm_flag_in_rtm_p);

	printf("\n RTM_SUPP_NET_INFO_BASE : %p, size:0x%x\n", dpm_supp_net_info_p, NET_INFO_ALLOC_SZ);
	print_dpm_supp_net_info(dpm_supp_net_info_p);

	printf("\n RTM_SUPP_IP_INFO_BASE  : %p, size:0x%x\n", dpm_supp_ip_info_p, NET_IP_ALLOC_SZ);
	print_dpm_supp_ip_info(dpm_supp_ip_info_p);

	printf("\n RTM_SUPP_CONN_INFO_BASE: %p, size:0x%x\n", dpm_supp_conn_info_p, CONN_INFO_ALLOC_SZ);
	print_dpm_supp_conn_info(dpm_supp_conn_info_p);

	printf("\n RTM_SUPP_KEY_INFO_BASE : %p, size:0x%x\n", dpm_supp_key_info_p, KEY_INFO_ALLOC_SZ);
		print_dpm_supp_key_info(dpm_supp_key_info_p);
#if defined (__SUPPORT_IPV4__)
	printf("\n RTM_ARP_BASE           : 0x%x, size:0x%x\n", RTM_ARP_BASE, ARP_ALLOC_SZ);
#endif // __SUPPORT_IPV4__
	printf(" RTM_DNS_BASE           : 0x%x, size:0x%x\n", RTM_DNS_BASE, DNS_ALLOC_SZ);
	printf(" RTM_RTC_TIMER_BASE     : 0x%x, size:0x%x\n", RTM_RTC_TIMER_BASE, RTC_TIMER_ALLOC_SZ);
	printf(" RTM_DPM_MONITOR_BASE   : 0x%x, size:0x%x\n", RTM_DPM_MONITOR_BASE, sizeof(dpm_monitor_info_t));
	printf(CYAN_COLOR " APP_ALLOC_SZ           : 0x%x\n" CLEAR_COLOR, APP_ALLOC_SZ);

	printf("\n RTM_TCP_BASE           : 0x%x, size:0x%x\n", RTM_TCP_BASE, TCP_ALLOC_SZ);
	RM_PMGR_W_socket_dpm_tcp_sess_rtm_content_print();
	for (int i=0; i<DPM_SOCK_MAX_TCP_SESS; i++) {
		RM_PMGR_W_socket_dpm_tcp_sess_pcb_print(i);
	}
	printf("\n RTM_SUPP_CONN_EXT_INFO_BASE: %p, size:0x%x\n", dpm_supp_conn_ext_info_p, RTM_SUPP_CONN_EXT_INFO_SIZE);
	print_dpm_supp_conn_ext_info(dpm_supp_conn_ext_info_p);

	printf("\n RTM_USER_POOL_BASE     : 0x%x, size:0x%x\n", RTM_USER_POOL_BASE, dg_configUSERHDR_RTM_SIZE);
	printf("\n RTM_USER_DATA_BASE     : 0x%x, size:0x%x\n", RTM_USER_DATA_BASE, RA6WX_RTM_USER_SIZE);
	dpm_user_rtm_print();

}
#endif  // dg_configUSE_RETENTION_MEM_INFO

/*
 * @fn    dpmrtm_reset
 * @brief Reset the RTM memory of DPM except for the image area.
 *              It is called when it is usually POR or RESET.
 * @param Pointer to DPM Param
 */
void dpmrtm_reset(struct dpm_param *dpmp, int sz)
{
    unsigned long env_base_addr = (unsigned long) dpm_get_env(dpmp);
    int tmp_sz = env_base_addr - ((unsigned long) dpmp);
#if 0    // TEMP_FOR_COMPILE
    UINT32 sysclk;

    _sys_clock_read(&sysclk, sizeof(UINT32));
    sysclk /= 1000000;
#endif

    memset((void *) env_base_addr, 0, sz - tmp_sz);

    dpm_set_preamble(dpmp);

    dpm_set_env_sys_clk(dpmp, DPM_SYS_CLK);
    dpm_set_env_sys_clk2(dpmp, DPM_SYS_CLK2);
    dpm_set_env_mac_clk(dpmp, DPM_MAC_CLK);
#if 0    // TEMP_FOR_COMPILE
#if 0 // In FreeRTOS, Booting CLK when PTIM goes to Fullboot must be 80M.
    dpm_set_env_sys_clk3(dpmp, DPM_SYS_CLK3);
#else
    dpm_set_env_sys_clk3(dpmp, sysclk);
#endif
#endif    // TEMP_FOR_COMPILE
    dpm_set_env_ptim_timeout_tu(dpmp, DPM_PTIM_TIMEOUT_TU);
    dpm_set_env_dtim_period(dpmp, 0);    //dtim period initial value is 0 , after Connection, it would be set as AP Dtim period
    //dpm_set_env_force_period(dpmp, 1);
	romac4rtos_req_period(1, -1);

    dpm_set_tim_version(dpmp, TIM_VER_INIT);

    dpm_set_env_rx_filter(dpmp, DPM_F_DROP_MY_DATA | DPM_F_MATCHED_MC |
            DPM_F_UC | DPM_F_DEAUTH | DPM_F_DROP_ACTION |
            DPM_F_DATA | DPM_F_DROP_NULL | DPM_F_ARP |
            DPM_F_DROP_IPv6 | DPM_F_DROP_OTHER_IP |
            DPM_F_DROP_BC_IP | DPM_F_DROP_MC_IP |
            DPM_F_MATCHED_MC_IP | DPM_F_UC_IP |
            DPM_F_DROP_UDP_SERVICE | DPM_F_MATCHED_UDP_SERVICE |
            DPM_F_DROP_TCP_SERVICE | DPM_F_MATCHED_TCP_SERVICE);

    /* TIMP Config */
    //timp_set_backup_addr(dpmp, (void *) TENTRY()->acquire_timp_backup()); // TEMP_FOR_COMPILE

    // TX Rates (Null, ARP reply, Auto ARP)
    dpm_set_env_txrate0(dpmp, 0x404); // OFDM 6M
    dpm_set_env_txrate1(dpmp, 0x400); // DSSS 1M
    dpm_set_env_txrate2(dpmp, 0x000);
    dpm_set_env_txrate3(dpmp, 0x000);

    // TX Rates (UDPH, TCPKA)
    dpm_set_env_usr_txrate0(dpmp, 0x404); // OFDM 6M
    dpm_set_env_usr_txrate1(dpmp, 0x400); // DSSS 1M
    dpm_set_env_usr_txrate2(dpmp, 0x000);
    dpm_set_env_usr_txrate3(dpmp, 0x000);

    dpm_set_env_cs_rxon_delay(dpmp, 200);
    dpm_set_env_uc_delay(dpmp, 192); // UC Delay: 192us

    // Set subnet mask
    dpm_set_env_subnet(dpmp, 0x00000000);

    // The minimum optimal value is min_optimal_tbtt.
    // dpmp->aptrk.bo.min_optimal_tbtt = 100;

    // Short Slot Unmask
    dpm_set_env_ap_cap_mask(dpmp, 0xfbff);
}

void dpm_sche_init(struct dpm_param *dpmp)
{
    memset(&dpmp->sche, 0, sizeof(struct dpm_schedule));
    memset(&dpmp->sche.prep_clk[0], 0x0, DPM_PREP_TIME_MAX);
}

void dpmrtm_init(struct dpm_param *dpmp)
{
    // Do something
    dpm_sche_init(dpmp);
}

#if (dg_configUSE_RETENTION_MEM_INFO == 1)
bool cmd_rtm_info(int argc, char *argv[])
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    print_retmem_info();
    return pdTRUE;
}
#endif    // dg_configUSE_RETENTION_MEM_INFO

///////////////////////////////////////////////////////////////////////////////
/////  For USER Retention Memory API  /////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

dpm_user_rtm_pool *user_rtm_pool = (dpm_user_rtm_pool *)RTM_USER_POOL_PTR;

// mem_malloc() / mem_free()    : libraries/3rdparty/lwip/src/core/mem.c



/*
 * @fn get_boot_mode
 * @return NORMAL BOOT is 0 and FULL BOOT is 1
 */
int get_boot_mode(void)
{
   int wakeup_source = RM_PMGR_W_dpm_wakeup_src_get();

    if (wakeup_source == BSP_WAKEUP_GPIO_WITH_RETENTION || 
        wakeup_source == BSP_WAKEUP_COUNTER_WITH_RETENTION) {
        return 1;
    }

    return 0;
}

void dpm_user_rtm_add(dpm_user_rtm *data)
{
    dpm_user_rtm *cur = NULL;

    if (user_rtm_pool->first_user_addr != NULL) {
        //goto end of linked-list
        for (cur = user_rtm_pool->first_user_addr;
            cur->next_user_addr != NULL ;
            cur = cur ->next_user_addr) {
        }

        cur->next_user_addr = data;
    } else {
        user_rtm_pool->first_user_addr = data;
    }

    return;
}

dpm_user_rtm *dpm_user_rtm_remove(char *name)
{
    dpm_user_rtm    *cur = NULL;
    dpm_user_rtm    *prev = NULL;

    if (name == NULL) {
        return NULL;
    }

    if (user_rtm_pool->first_user_addr != NULL) {
        cur = user_rtm_pool->first_user_addr;
        if (!cur) {
            return NULL;
        }

        if (dpm_strcmp(cur->name, name) == 0) {
            user_rtm_pool->first_user_addr = cur->next_user_addr;
            return cur;
        } else {
            do {
                prev= cur;
                cur= cur->next_user_addr;

                if (cur != NULL && dpm_strcmp(cur->name, name) == 0) {
                    prev->next_user_addr = cur->next_user_addr;
                    return cur;
                }
            } while (cur != NULL && cur->next_user_addr != NULL);
        }
    }

    return NULL;
}

dpm_user_rtm *dpm_user_rtm_search(char *name)
{
    dpm_user_rtm *cur = NULL;
    
    if (name == NULL) {
        return NULL;
    }

    if (user_rtm_pool->first_user_addr != NULL) {
        for (cur = user_rtm_pool->first_user_addr; cur != NULL; cur = cur->next_user_addr) {
            if (dpm_strcmp(cur->name, name) == 0) {
                return cur;
            }
        }
    }

    return NULL;
}

void dpm_user_rtm_print(void)
{
    dpm_user_rtm *cur = NULL;

    /* Show total pool information */
    RM_PMGR_W_rtm_heap_status_print();

    /* show each allocated information */
    if (user_rtm_pool->first_user_addr != NULL) {
        PRINTF(" --- Detail information --------------------------------------\n");
        for (cur = user_rtm_pool->first_user_addr; cur != NULL; cur = cur->next_user_addr) {
            PRINTF("[%s]\taddr=0x%p, alloc_size=%d, data_size:%d\n",
                    cur->name,
                    cur,
                    cur->size + sizeof(dpm_user_rtm),
                    cur->size);
            PRINTF(" -------------------------------------------------------------\n");
        }
    } else {
        PRINTF("\n - Whole available to allocate ( No allocated ) ...\n");
    }

    PRINTF("\n");
}

#endif /* CFG_WIFI */

#endif /* CFG_PMGR */
