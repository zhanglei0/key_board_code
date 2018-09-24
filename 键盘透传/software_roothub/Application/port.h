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

void	CH374_PORT_INIT( );  /* CH374�ӿڳ�ʼ�� */

uint8_t	Read374Byte( uint8_t mAddr );	// ��ָ���Ĵ�����ȡ����

void	Write374Byte( uint8_t mAddr, uint8_t mData );	// ��ָ���Ĵ���д������

void	Read374Block( uint8_t mAddr, uint8_t mLen, uint8_t* mBuf );  /* ��ָ����ʼ��ַ�������ݿ� */

void	Write374Block( uint8_t mAddr, uint8_t mLen, uint8_t* mBuf );  /* ��ָ����ʼ��ַд�����ݿ� */

// CH374������������Ŀ�Ķ˵��ַ/PID����/ͬ����־������ͬCH375��NAK�����ԣ���ʱ/��������
UINT8	HostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog );

// CH374������������Ŀ�Ķ˵��ַ/PID����/ͬ����־/��mSΪ��λ��NAK������ʱ��(0xFFFF��������)������ͬCH375��NAK���ԣ���ʱ��������
UINT8	WaitHostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog, UINT16 timeout );

UINT8	HostCtrlTransfer374( PUINT8 ReqBuf, PUINT8 DatBuf, PUINT8 RetLen );  // ִ�п��ƴ���,ReqBufָ��8�ֽ�������,DatBufΪ�շ�������
// �����Ҫ���պͷ������ݣ���ôDatBuf��ָ����Ч���������ڴ�ź������ݣ�ʵ�ʳɹ��շ����ܳ��ȱ�����ReqLenָ����ֽڱ�����

// ��ѯ��ǰ�Ƿ����USB�豸
//BOOL	Query374DeviceIn( void );
#define	Query374DeviceIn( )	( ( Read374Byte( REG_HUB_SETUP ) & BIT_HUB0_ATTACH ) ? TRUE : FALSE )

// ��ѯ��ǰ��USB�豸��ȫ�ٻ��ǵ���, ����TRUEΪȫ��
//BOOL	Query374DevFullSpeed( void );
#define	Query374DevFullSpeed( )	( ( Read374Byte( REG_SYS_INFO ) & BIT_INFO_USB_DP ) ? TRUE : FALSE )

void	HostDetectInterrupt( void );  // ����USB�豸����¼��ж�

void	SetHostUsbAddr( UINT8 addr );  // ����USB������ǰ������USB�豸��ַ

void	HostSetBusFree( void );  // USB���߿���

void	HostSetBusReset( void );  // USB���߸�λ

void	HostSetFullSpeed( void );  // �趨ȫ��USB�豸���л���

void	HostSetLowSpeed( void );  // �趨����USB�豸���л���

void	Init374Host( void );  // ��ʼ��USB����
BOOL	Query374Interrupt( void );


UINT8	GetDeviceDescr( PUINT8 buf );  // ��ȡ�豸������
UINT8	SetUsbAddress( UINT8 addr );  // ����USB�豸��ַ
UINT8	SetUsbConfig( UINT8 cfg );  // ����USB�豸����
UINT8	GetConfigDescr( PUINT8 buf );  // ��ȡ����������

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