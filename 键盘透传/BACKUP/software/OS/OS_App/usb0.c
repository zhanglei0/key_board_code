


#include <rthw.h>
#include <rtthread.h>

#include "port.h"

#define USB0_THREAD_STACK_SIZE        2048

#define LOCAL_PORT              0

static UINT8 DeviceDescBuf[DEVICE_DESC_LEN];
static UINT8 ConfigDescBuf[CONFIG_DESC_LEN];
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
  uint32_t e;
  int port_status;
  uint8_t status;
  PUSB_DEV_DESCR device_desc;
  while(1) {
begin_poll:
     if(RT_EOK == rt_event_recv(&usb0_wakeup_evt,
                   USB0_WKUP_EVT,
                   RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR,
                   RT_WAITING_FOREVER,
                   &e))
     {
       if(e == USB0_WKUP_EVT) {
         port_status = Ch374_CheckPortAttach(LOCAL_PORT);
         if(TRUE == port_status)
         {
           rt_thread_delay(250);
           if (Query374Interrupt()) {
             HostDetectInterrupt();  // 如果有USB主机中断则处理
           }
           HostSetBusReset();
           for ( i = 0; i < 50; i++ )
           {
              // 等待USB设备复位后重新连接
              if ( Ch374_CheckPortAttach(LOCAL_PORT))
                break;  // 有USB设备
              rt_thread_delay(10);
            }
            if(Query374Interrupt())
              HostDetectInterrupt();
            if (Ch374_CheckPortAttach(LOCAL_PORT))
            {
              // 有USB设备
              if ( Query374DevPortSpeed()) 
              {
                HostSetFullSpeed();  // 检测到全速USB设备
              }
              else 
              {
                HostSetLowSpeed();
              }
            }
            else 
              goto begin_poll;
            rt_thread_delay(50);
            /* USB setup process */
            status = GetDeviceDescr(DeviceDescBuf);  // 获取设备描述符
            if ( status != USB_INT_SUCCESS ) {
               goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
            }
            device_desc = (PUSB_DEV_DESCR)DeviceDescBuf;
            status = SetUsbAddress( 0x02 );  // 设置USB设备地址
            if ( status != USB_INT_SUCCESS ) {
               goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
            }
            status = GetConfigDescr(ConfigDescBuf);  // 获取配置描述符
            if ( status != USB_INT_SUCCESS ) {
                goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
            }
            status = SetUsbConfig(((PUSB_CFG_DESCR)ConfigDescBuf)->bConfigurationValue);
            if ( status != USB_INT_SUCCESS ) {
              goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
            }
            status = Set_Idle( ); //设置IDLE，这个步骤是按照HID类的协议来做的
            if(status != USB_INT_SUCCESS) {
              if((status & 0x0f) == USB_INT_RET_STALL)
                goto next_operate1; //返回STALL可能本身不支持
            }
         next_operate1:
            status = Get_Hid_Des(HidDescBuf);  // 获取报告描述符描述符
            if(status != USB_INT_SUCCESS)
              goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
            while(1) {
              //handle USB transfer
            }
         WaitDeviceOut:
           while (1) {
              if ( Query374Interrupt()) 
                HostDetectInterrupt();  // 如果有USB主机中断则处理
              if ( Ch374_CheckPortAttach(0) == FALSE )
                break;  // 没有USB设备
            }
           rt_thread_delay(100);  // 等待设备完全断开，消除插拔抖动
           if (Ch374_CheckPortAttach(LOCAL_PORT))
             goto WaitDeviceOut;  // 没有完全断开
         }
       }
     }
  }
}



/**
 * This function will initialize barcode thread, then start it.
 *
 * @note this function must be invoked when system init.
 */
void rt_thread_bar_init(void)
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
