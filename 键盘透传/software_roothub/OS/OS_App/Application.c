
#include <rthw.h>
#include <rtthread.h>

#include "port.h"
#include "ch374inc.h"
#include "EX_HUB.H"   // 有关外部HUB的定义

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
    RootHubDev[n].DeviceStatus = ROOT_DEV_DISCONNECT;  // 清空
  count = 0;
  Init374Host();  // 初始化USB主机
  HostEnableRootHub();  // 启用内置的Root-HUB
  rt_kprintf("Wait Device In\n");
  while(1) 
  {
    if (Query374Interrupt()) 
      HostDetectInterrupt();  // 如果有USB主机中断则处理
      if (NewDevCount) {  // 有新的USB设备
        rt_thread_delay( 200 );  // 由于USB设备刚插入尚未稳定，故等待USB设备数百毫秒，消除插拔抖动
        NewDevCount=0;
        for ( n = 0; n < 3; n ++ ) {
          if ( RootHubDev[n].DeviceStatus == ROOT_DEV_CONNECTED ) {   // 刚插入设备尚未初始化
            s = InitDevice( n );  // 初始化/枚举指定HUB端口的USB设备
            RootHubDev[n].DeviceType = s;  // 保存设备类型
            if ( s == DEV_HUB ) {  // HUB枚举成功
              SelectHubPort( n, 0 );  // 选择操作指定的ROOT-HUB端口,设置当前USB速度以及被操作设备的USB地址
                                      // 做点什么?  给HUB各端口上电,查询各端口状态,初始化有设备连接的HUB端口,初始化设备
              s = HubPortEnum( n );  // 枚举指定ROOT-HUB端口上的外部HUB集线器的各个端口,检查各端口有无连接或移除事件
              if ( s != USB_INT_SUCCESS ) {  // 可能是HUB断开了
                      printf( "HubPortEnum error = %02X\n", (UINT16)s );
              }
              SetUsbSpeed( TRUE );  // 默认为全速
            }
          }
        }
      }
      //other work for each device
      rt_thread_delay( 100 );  // 模拟单片机做其它事
      if ( count & 0x02 ) {  // 每隔一段时间对外部HUB的端口进行一下枚举,单片机有空时做
        for ( n = 0; n < 3; n ++ ) {  // 搜索外部HUB设备
          if ( RootHubDev[n].DeviceType == DEV_HUB 
              && RootHubDev[n].DeviceStatus >= ROOT_DEV_SUCCESS ) 
          {  // 有效的外部HUB
            SelectHubPort( n, 0 );  // 选择操作指定的ROOT-HUB端口,设置当前USB速度以及被操作设备的USB地址
            s = HubPortEnum( n );  // 枚举指定ROOT-HUB端口上的外部HUB集线器的各个端口,检查各端口有无连接或移除事件
            if ( s != USB_INT_SUCCESS ) {  // 可能是HUB断开了
                    rt_kprintf( "HubPortEnum error = %02X\r\n", (UINT16)s );
            }
            SetUsbSpeed( TRUE );  // 默认为全速
          }
        }
      }
      count++;
      if ( count > 50 ) 
        count = 0;
      switch(count) {
        case 17:  // 用定时模拟主观需求,需要操作鼠标
          loc = SearchAllHubPort(DEV_MOUSE);  // 在ROOT-HUB以及外部HUB各端口上搜索指定类型的设备所在的端口号
          if ( loc != 0xFFFF ) {  // 找到了,如果有两个MOUSE如何处理?
            n = loc >> 8;
            loc &= 0xFF;
            rt_kprintf( "Query Mouse\n" );
            SelectHubPort( n, loc );  // 选择操作指定的ROOT-HUB端口,设置当前USB速度以及被操作设备的USB地址
            i = (loc?DevOnHubPort[n][loc-1].GpVar:RootHubDev[n].GpVar);  // 中断端点的地址,位7用于同步标志位
            if ( i & 0x7F ) {  // 端点有效
              s = HostTransact374( i & 0x7F, DEF_USB_PID_IN, i & 0x80 );  // CH374传输事务,获取数据
              if ( s == USB_INT_SUCCESS ) {
                i ^= 0x80;  // 同步标志翻转
                if ( loc ) DevOnHubPort[n][loc-1].GpVar = i;  // 保存同步标志位
                else RootHubDev[n].GpVar = i;
                i = Read374Byte( REG_USB_LENGTH );  // 接收到的数据长度
                if (i > 0) {
                  Read374Block( RAM_HOST_RECV, i, TempBuf );  // 取出数据并打印
                }
              }
              else if ( s != ( 0x20 | USB_INT_RET_NAK ) ) 
                rt_kprintf("Mouse error %02x\n",(UINT16)s);  // 可能是断开了
            }
            else 
              rt_kprintf("Mouse no interrupt endpoint\n");
            SetUsbSpeed(TRUE);  // 默认为全速
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