#ifndef PORT_H
#define PORT_H

#include "string.h"
#include "stdint.h"
#include "stdio.h"
#include "stm32f10x.h"
#include "ch374inc.h"
#include "EX_HUB.H"

#include "usb_desc.h"


#define Spi374Start()     GPIO_ResetBits(GPIOA,GPIO_Pin_4)
#define Spi374Stop()      GPIO_SetBits(GPIOA,GPIO_Pin_4)
#define CH374_INT_WIRE    GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_3)

void InitSystick(void);
void InitSPI(void);
void Delay(uint32_t ms);
void SpiTransceiver(uint8_t *buff,uint16_t len);

void	CH374_PORT_INIT( );  /* CH374接口初始化 */

uint8_t	Read374Byte( uint8_t mAddr );	// 从指定寄存器读取数据

void	Write374Byte( uint8_t mAddr, uint8_t mData );	// 向指定寄存器写入数据

void	Read374Block( uint8_t mAddr, uint8_t mLen, uint8_t* mBuf );  /* 从指定起始地址读出数据块 */

void	Write374Block( uint8_t mAddr, uint8_t mLen, uint8_t* mBuf );  /* 向指定起始地址写入数据块 */

// CH374传输事务，输入目的端点地址/PID令牌/同步标志，返回同CH375，NAK不重试，超时/出错重试
UINT8	HostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog );

// CH374传输事务，输入目的端点地址/PID令牌/同步标志/以mS为单位的NAK重试总时间(0xFFFF无限重试)，返回同CH375，NAK重试，超时出错重试
UINT8	WaitHostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog, UINT16 timeout );

UINT8	HostCtrlTransfer374( PUINT8 ReqBuf, PUINT8 DatBuf, PUINT8 RetLen );  // 执行控制传输,ReqBuf指向8字节请求码,DatBuf为收发缓冲区
// 如果需要接收和发送数据，那么DatBuf需指向有效缓冲区用于存放后续数据，实际成功收发的总长度保存在ReqLen指向的字节变量中

// 查询当前是否存在USB设备
//BOOL	Query374DeviceIn( void );
#define	Query374DeviceIn( )	( ( Read374Byte( REG_HUB_SETUP ) & BIT_HUB0_ATTACH ) ? TRUE : FALSE )

// 查询当前的USB设备是全速还是低速, 返回TRUE为全速
//BOOL	Query374DevFullSpeed( void );
#define	Query374DevFullSpeed( )	( ( Read374Byte( REG_SYS_INFO ) & BIT_INFO_USB_DP ) ? TRUE : FALSE )

void	HostDetectInterrupt( void );  // 处理USB设备插拔事件中断

void	SetHostUsbAddr( UINT8 addr );  // 设置USB主机当前操作的USB设备地址

void	HostSetBusFree( void );  // USB总线空闲

void	HostSetBusReset( void );  // USB总线复位

void	HostSetFullSpeed( void );  // 设定全速USB设备运行环境

void	HostSetLowSpeed( void );  // 设定低速USB设备运行环境

void	Init374Host( void );  // 初始化USB主机
BOOL	Query374Interrupt( void );


UINT8	GetDeviceDescr( PUINT8 buf );  // 获取设备描述符
UINT8	SetUsbAddress( UINT8 addr );  // 设置USB设备地址
UINT8	SetUsbConfig( UINT8 cfg );  // 设置USB设备配置
UINT8	GetConfigDescr( PUINT8 buf );  // 获取配置描述符

UINT8  Set_Idle(void);
UINT8  Get_Hid_Des(unsigned char *p);
unsigned char Interrupt_Data_Trans(UINT8 endp_in_addr,PUINT8 p,PUINT8 counter);
UINT8  Set_Report(unsigned char *p);


uint32_t GetTick(void);

#define DEVICE_DESC_LEN                 256
#define CONFIG_DESC_LEN                 256
#define POINT_DESC_LEN                  256
#define HID_DESC_LEN                    256


#define USB0_WKUP_EVT                   0x01
#define USB1_WKUP_EVT                   0x02
#define USB2_WKUP_EVT                   0x04


#define USB0_PRI                        10
#define USB1_PRI                        11
#define USB2_PRI                        12
#define INIT_PRI                        9

void control_led(uint8_t state);
int rt_application_init(void);
void rt_thread_init_init(void);
void rtthread_startup(void);
void  SysTick_Configuration(void);
#endif