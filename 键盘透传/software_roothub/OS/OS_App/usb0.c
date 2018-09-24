


#include <rthw.h>
#include <rtthread.h>

#include "port.h"

#define USB0_THREAD_STACK_SIZE        2048

#define LOCAL_PORT              0
/* usb0 */
static struct rt_thread usb0_handle;
ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t rt_usb0_stack[USB0_THREAD_STACK_SIZE];

static struct rt_event usb0_wakeup_evt;

rt_err_t WakeUpPort0(void)
{
  return rt_event_send(&usb0_wakeup_evt,USB0_WKUP_EVT);
}

/* threads */
static void rt_thread_usb0_entry(void *parameter)
{
  rt_event_init(&usb0_wakeup_evt,"ch374 usb0",RT_IPC_FLAG_FIFO);
  rt_uint32_t e;
  int port_status;
  rt_uint8_t status;
  
  while(1) {
begin_poll:
     if(RT_EOK == rt_event_recv(&usb0_wakeup_evt,
                   USB0_WKUP_EVT,
                   RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR,
                   RT_WAITING_FOREVER,
                   &e))
     {
       if(e == USB0_WKUP_EVT) {
         
       }
     }
  }
}



/**
 * This function will initialize barcode thread, then start it.
 *
 * @note this function must be invoked when system init.
 */
void rt_thread_usb0_init(void)
{
    /* initialize thread */
    rt_thread_init(&usb0_handle,
                   "usb0 handle",
                   rt_thread_usb0_entry,
                   RT_NULL,
                   &rt_usb0_stack[0],
                   sizeof(rt_usb0_stack),
                   USB0_PRI,
                   32);
    
    /* startup */
    rt_thread_startup(&usb0_handle);
}
