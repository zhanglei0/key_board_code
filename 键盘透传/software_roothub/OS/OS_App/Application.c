
#include <rthw.h>
#include <rtthread.h>

#include "port.h"
#include "ch374inc.h"
#include "EX_HUB.H"   // �й��ⲿHUB�Ķ���

__IO uint8_t PrevXferComplete = 0;

extern UINT8	NewDevCount;
extern UINT8	CtrlBuf[8];
extern UINT8	TempBuf[64];
extern struct _RootHubDev    RootHubDev[3];
extern struct _DevOnHubPort  DevOnHubPort[3][4];
int rt_application_init(void)
{
  SysTick_Configuration();
  return 0;
}


#define INIT_THREAD_STACK_SIZE        2048

/* Init CH374 */
static struct rt_thread init_ch374_handle;
ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t rt_init_stack[INIT_THREAD_STACK_SIZE];

/* threads */
static void rt_thread_init_entry(void *parameter)
{
  rt_uint8_t i,s, n;
  rt_uint8_t count;
  rt_uint16_t loc;
  NewDevCount = 0;
  UINT8 status;
  InitSystick();
  InitSPI();
  rt_thread_delay(50);
  for ( n = 0; n < 3; n ++ )
    RootHubDev[n].DeviceStatus = ROOT_DEV_DISCONNECT;  // ���
  count = 0;
  Init374Host();  // ��ʼ��USB����
  HostEnableRootHub();  // �������õ�Root-HUB
  rt_kprintf("Wait Device In\n");
  while(1) 
  {
    if (Query374Interrupt()) 
      HostDetectInterrupt();  // �����USB�����ж�����
      if (NewDevCount) {  // ���µ�USB�豸
        rt_thread_delay( 200 );  // ����USB�豸�ղ�����δ�ȶ����ʵȴ�USB�豸���ٺ��룬������ζ���
        NewDevCount=0;
        for ( n = 0; n < 3; n ++ ) {
          if ( RootHubDev[n].DeviceStatus == ROOT_DEV_CONNECTED ) {   // �ղ����豸��δ��ʼ��
            s = InitDevice( n );  // ��ʼ��/ö��ָ��HUB�˿ڵ�USB�豸
            RootHubDev[n].DeviceType = s;  // �����豸����
            if ( s == DEV_HUB ) {  // HUBö�ٳɹ�
              SelectHubPort( n, 0 );  // ѡ�����ָ����ROOT-HUB�˿�,���õ�ǰUSB�ٶ��Լ��������豸��USB��ַ
                                      // ����ʲô?  ��HUB���˿��ϵ�,��ѯ���˿�״̬,��ʼ�����豸���ӵ�HUB�˿�,��ʼ���豸
              s = HubPortEnum( n );  // ö��ָ��ROOT-HUB�˿��ϵ��ⲿHUB�������ĸ����˿�,�����˿��������ӻ��Ƴ��¼�
              if ( s != USB_INT_SUCCESS ) {  // ������HUB�Ͽ���
                      printf( "HubPortEnum error = %02X\n", (UINT16)s );
              }
              SetUsbSpeed( TRUE );  // Ĭ��Ϊȫ��
            }
          }
        }
      }
      //other work for each device
      rt_thread_delay( 100 );  // ģ�ⵥƬ����������
      if ( count & 0x02 ) {  // ÿ��һ��ʱ����ⲿHUB�Ķ˿ڽ���һ��ö��,��Ƭ���п�ʱ��
        for ( n = 0; n < 3; n ++ ) {  // �����ⲿHUB�豸
          if ( RootHubDev[n].DeviceType == DEV_HUB 
              && RootHubDev[n].DeviceStatus >= ROOT_DEV_SUCCESS ) 
          {  // ��Ч���ⲿHUB
            SelectHubPort( n, 0 );  // ѡ�����ָ����ROOT-HUB�˿�,���õ�ǰUSB�ٶ��Լ��������豸��USB��ַ
            s = HubPortEnum( n );  // ö��ָ��ROOT-HUB�˿��ϵ��ⲿHUB�������ĸ����˿�,�����˿��������ӻ��Ƴ��¼�
            if ( s != USB_INT_SUCCESS ) {  // ������HUB�Ͽ���
                    rt_kprintf( "HubPortEnum error = %02X\r\n", (UINT16)s );
            }
            SetUsbSpeed( TRUE );  // Ĭ��Ϊȫ��
          }
        }
      }
      count++;
      if ( count > 50 ) 
        count = 0;
      switch(count) {
        case 17:  // �ö�ʱģ����������,��Ҫ�������
          loc = SearchAllHubPort(DEV_MOUSE);  // ��ROOT-HUB�Լ��ⲿHUB���˿�������ָ�����͵��豸���ڵĶ˿ں�
          if ( loc != 0xFFFF ) {  // �ҵ���,���������MOUSE��δ���?
            n = loc >> 8;
            loc &= 0xFF;
            rt_kprintf( "Query Mouse\n" );
            SelectHubPort( n, loc );  // ѡ�����ָ����ROOT-HUB�˿�,���õ�ǰUSB�ٶ��Լ��������豸��USB��ַ
            i = (loc?DevOnHubPort[n][loc-1].GpVar:RootHubDev[n].GpVar);  // �ж϶˵�ĵ�ַ,λ7����ͬ����־λ
            if ( i & 0x7F ) {  // �˵���Ч
              s = HostTransact374( i & 0x7F, DEF_USB_PID_IN, i & 0x80 );  // CH374��������,��ȡ����
              if ( s == USB_INT_SUCCESS ) {
                i ^= 0x80;  // ͬ����־��ת
                if ( loc ) DevOnHubPort[n][loc-1].GpVar = i;  // ����ͬ����־λ
                else RootHubDev[n].GpVar = i;
                i = Read374Byte( REG_USB_LENGTH );  // ���յ������ݳ���
                if (i > 0) {
                  Read374Block( RAM_HOST_RECV, i, TempBuf );  // ȡ�����ݲ���ӡ
                }
              }
              else if ( s != ( 0x20 | USB_INT_RET_NAK ) ) 
                rt_kprintf("Mouse error %02x\n",(UINT16)s);  // �����ǶϿ���
            }
            else 
              rt_kprintf("Mouse no interrupt endpoint\n");
            SetUsbSpeed(TRUE);  // Ĭ��Ϊȫ��
          }
          break;
      }

  }
}



/**
 * This function will initialize barcode thread, then start it.
 *
 * @note this function must be invoked when system init.
 */
void rt_thread_init_init(void)
{
    /* initialize thread */
    rt_thread_init(&init_ch374_handle,
                   "init ch374",
                   rt_thread_init_entry,
                   RT_NULL,
                   &rt_init_stack[0],
                   sizeof(rt_init_stack),
                   INIT_PRI,
                   32);
    
    /* startup */
    rt_thread_startup(&init_ch374_handle);
}

int main(void)
{
  rtthread_startup();
  return 0;
}