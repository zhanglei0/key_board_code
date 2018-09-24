


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
             HostDetectInterrupt();  // �����USB�����ж�����
           }
           HostSetBusReset();
           for ( i = 0; i < 50; i++ )
           {
              // �ȴ�USB�豸��λ����������
              if ( Ch374_CheckPortAttach(LOCAL_PORT))
                break;  // ��USB�豸
              rt_thread_delay(10);
            }
            if(Query374Interrupt())
              HostDetectInterrupt();
            if (Ch374_CheckPortAttach(LOCAL_PORT))
            {
              // ��USB�豸
              if ( Query374DevPortSpeed()) 
              {
                HostSetFullSpeed();  // ��⵽ȫ��USB�豸
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
            status = GetDeviceDescr(DeviceDescBuf);  // ��ȡ�豸������
            if ( status != USB_INT_SUCCESS ) {
               goto WaitDeviceOut;  // ��ֹ����,�ȴ�USB�豸�γ�
            }
            device_desc = (PUSB_DEV_DESCR)DeviceDescBuf;
            status = SetUsbAddress( 0x02 );  // ����USB�豸��ַ
            if ( status != USB_INT_SUCCESS ) {
               goto WaitDeviceOut;  // ��ֹ����,�ȴ�USB�豸�γ�
            }
            status = GetConfigDescr(ConfigDescBuf);  // ��ȡ����������
            if ( status != USB_INT_SUCCESS ) {
                goto WaitDeviceOut;  // ��ֹ����,�ȴ�USB�豸�γ�
            }
            status = SetUsbConfig(((PUSB_CFG_DESCR)ConfigDescBuf)->bConfigurationValue);
            if ( status != USB_INT_SUCCESS ) {
              goto WaitDeviceOut;  // ��ֹ����,�ȴ�USB�豸�γ�
            }
            status = Set_Idle( ); //����IDLE����������ǰ���HID���Э��������
            if(status != USB_INT_SUCCESS) {
              if((status & 0x0f) == USB_INT_RET_STALL)
                goto next_operate1; //����STALL���ܱ���֧��
            }
         next_operate1:
            status = Get_Hid_Des(HidDescBuf);  // ��ȡ����������������
            if(status != USB_INT_SUCCESS)
              goto WaitDeviceOut;  // ��ֹ����,�ȴ�USB�豸�γ�
            while(1) {
              //handle USB transfer
            }
         WaitDeviceOut:
           while (1) {
              if ( Query374Interrupt()) 
                HostDetectInterrupt();  // �����USB�����ж�����
              if ( Ch374_CheckPortAttach(0) == FALSE )
                break;  // û��USB�豸
            }
           rt_thread_delay(100);  // �ȴ��豸��ȫ�Ͽ���������ζ���
           if (Ch374_CheckPortAttach(LOCAL_PORT))
             goto WaitDeviceOut;  // û����ȫ�Ͽ�
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
