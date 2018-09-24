#ifndef PORT_H
#define PORT_H

#include "string.h"
#include "stdint.h"
#include "stdio.h"
#include "stm32f10x.h"
#include "ch374inc.h"

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

void control_led(uint8_t state);
int rt_application_init(void);


int Ch374_CheckPortAttach(void);
int Query374DevPortSpeed(void);
uint8_t GetFunctionSel(void);
void InitBeep(void);
void InitUart(void);
void Beep(uint32_t tick);
uint8_t ScanKeyBoard(void);
void InitKey(void);
void InitDetectPin(void);
void control_led(uint8_t state);
void HandleUARTEvt(void);

void HostEnableRootHub( void );

//USB设备相关信息表，CH374U最多支持3个设备
#define		ROOT_DEV_DISCONNECT		0
#define		ROOT_DEV_CONNECTED		1
#define		ROOT_DEV_FAILED			2
#define		ROOT_DEV_SUCCESS		3

#define	DEV_ERROR		0x00
#define	DEV_KEYBOARD	        0x31
#define	DEV_MOUSE		0x32
#define	DEV_PRINT		0x70
#define	DEV_DISK		0x80
#define	DEV_HUB			0x90
#define	DEV_UNKNOWN		0xFF

//////Hub Class Feature Selectors
#define	C_HUB_LOCAL_POWER	0
#define C_HUB_OVER_CURRENT	1
#define PORT_CONNECTION		0
#define PORT_ENABLE             1
#define PORT_SUSPEND		2
#define PORT_OVER_CURRENT	3
#define PORT_RESET              4
#define PORT_POWER              8
#define	PORT_LOW_SPEED		9
#define C_PORT_CONNECTION	16
#define C_PORT_ENABLE		17
#define C_PORT_SUSPEND		18
#define C_PORT_OVER_CURRENT	19
#define C_PORT_RESET		20

#define CLEAR_HUB_FEATURE	0x20
#define CLEAR_PORT_FEATURE	0x23
#define GET_BUS_STATE		0xa3
#define GET_HUB_DESCRIPTOR	0xa0
#define GET_HUB_STATUS		0xa0
#define GET_PORT_STATUS		0xa3
#define SET_HUB_DESCRIPTOR	0x20
#define SET_HUB_FEATURE		0x20
#define SET_PORT_FEATURE	0x23


typedef struct {
  UINT8	DeviceStatus;			// 设备状态,0-无设备,1-有设备但尚未初始化,2-有设备但初始化枚举失败,3-有设备且初始化枚举成功
  UINT8	DeviceAddress;			// 设备被分配的USB地址
  UINT8	DeviceSpeed;			// 0为低速,非0为全速
  UINT8	DeviceType;				// 设备类型
  UINT8	GpVar;					// 通用变量
} _RootHubDev;

typedef struct {
  UINT8	DeviceStatus;			// 设备状态,0-无设备,1-有设备但尚未初始化,2-有设备但初始化枚举失败,3-有设备且初始化枚举成功
  UINT8	DeviceAddress;			// 设备被分配的USB地址
  UINT8	DeviceSpeed;			// 0为低速,非0为全速
  UINT8	DeviceType;				// 设备类型
  UINT8	GpVar;					// 通用变量
} _DevOnHubPort;  // 假定:不超过三个外部HUB,每个外部HUB不超过4个端口(多了不管)


#define CTRL_KEY                        0x0A

#define KEY_1                           0x0C
#define KEY_2                           0x0D
#define KEY_3                           0x0E
#define KEY_4                           0x0F

#define CTRL_ALT_KEY_1                  0x11
#define CTRL_ALT_KEY_2                  0x12
#define CTRL_ALT_KEY_3                  0x13
#define CTRL_ALT_KEY_4                  0x14

#define CTRL_KEY_1                      0x20
#define CTRL_KEY_2                      0x21
#define CTRL_KEY_3                      0x22
#define CTRL_KEY_4                      0x23

#define DUMMY_KEY                       0x70

extern _RootHubDev RootHubDev[3];


UINT8 AnalyzeHidIntEndp( void );
UINT8 InitDevice( UINT8 index );
void SetUsbSpeed( BOOL FullSpeed );
UINT16	SearchAllHubPort( UINT8 type );
void SelectHubPort( UINT8 HubIndex, UINT8 PortIndex );
UINT8	HubPortEnum( UINT8 index );
UINT8	SearchRootHubPort( UINT8 type );
UINT8	GetHubDescriptor( void );
UINT8	GetPortStatus( UINT8 port );
UINT8	SetPortFeature( UINT8 port, UINT8 select );
UINT8	ClearPortFeature( UINT8 port, UINT8 select );
void	mDelaymS( UINT8 ms );
void	AnalyzeRootHub( void );
uint8_t HandleKeyBoardKeyVal(uint8_t *input);
void ProcessKeyBoardVal(uint8_t val,uint8_t bootmode);
#endif