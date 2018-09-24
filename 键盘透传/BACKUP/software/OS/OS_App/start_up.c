#include <rthw.h>
#include <rtthread.h>

#include "port.h"

#pragma section="HEAP"
#define STM32_SRAM_BEGIN    (__segment_begin("HEAP"))
#define STM32_SRAM_END    (__segment_end("HEAP"))
/**
 * This function will startup RT-Thread RTOS.
 */
void rtthread_startup(void)
{
    /* show version */
    rt_show_version();
    
#ifdef RT_USING_HEAP
    rt_system_heap_init(STM32_SRAM_BEGIN, STM32_SRAM_END);
#endif /* RT_USING_HEAP */

    /* init scheduler system */
    rt_system_scheduler_init();

    /* initialize timer */
    rt_system_timer_init();

    /* init timer thread */
    rt_system_timer_thread_init();

    /* init application */
    rt_application_init();

    /* init idle thread */
    rt_thread_idle_init();

    /* start scheduler */
    rt_system_scheduler_start();

    /* never reach here */
    return ;
}
