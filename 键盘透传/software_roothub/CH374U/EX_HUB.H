#ifndef EX_HUB_H
#define EX_HUB_H

#include "ch374inc.h"


#define CLEAR_HUB_FEATURE	0x20
#define CLEAR_PORT_FEATURE	0x23
#define GET_BUS_STATE		0xa3
#define GET_HUB_DESCRIPTOR	0xa0
#define GET_HUB_STATUS		0xa0
#define GET_PORT_STATUS		0xa3
#define SET_HUB_DESCRIPTOR	0x20
#define SET_HUB_FEATURE		0x20
#define SET_PORT_FEATURE	0x23

//////Hub Class Feature Selectors
#define	C_HUB_LOCAL_POWER	0
#define C_HUB_OVER_CURRENT	1
#define PORT_CONNECTION		0
#define PORT_ENABLE			1
#define PORT_SUSPEND		2
#define PORT_OVER_CURRENT	3
#define PORT_RESET			4
#define PORT_POWER			8
#define	PORT_LOW_SPEED		9
#define C_PORT_CONNECTION	16
#define C_PORT_ENABLE		17
#define C_PORT_SUSPEND		18
#define C_PORT_OVER_CURRENT	19
#define C_PORT_RESET		20

////////Hub Class Request Codes
#define GET_STATUS			0
#define CLEAR_FEATURE		1
#define GET_STATE			2
#define SET_FEATURE			3
#define GET_DESCRIPTOR		6
#define SET_DESCRIPTOR		7

typedef struct 
{
	unsigned char bDescLength;
	unsigned char bDescriptorType;
	unsigned char bNbrPorts;
	unsigned char wHubCharacteristics[2];
	unsigned char bPwrOn2PwrGood;
	unsigned char bHubContrCurrent;
	unsigned char DeviceRemovable;
	
} HUBDescr,*PHUBDescr;

#define		ROOT_DEV_DISCONNECT		0
#define		ROOT_DEV_CONNECTED		1
#define		ROOT_DEV_FAILED			2
#define		ROOT_DEV_SUCCESS		3


#define	DEV_ERROR		0x00
#define	DEV_KEYBOARD	0x31
#define	DEV_MOUSE		0x32
#define	DEV_PRINT		0x70
#define	DEV_DISK		0x80
#define	DEV_HUB			0x90
#define	DEV_UNKNOWN		0xFF


struct _RootHubDev{
  UINT8	DeviceStatus;			// 设备状态,0-无设备,1-有设备但尚未初始化,2-有设备但初始化枚举失败,3-有设备且初始化枚举成功
  UINT8	DeviceAddress;			// 设备被分配的USB地址
  UINT8	DeviceSpeed;			// 0为低速,非0为全速
  UINT8	DeviceType;				// 设备类型
  UINT8	GpVar;					// 通用变量
} ;


struct _DevOnHubPort{
  UINT8	DeviceStatus;			// 设备状态,0-无设备,1-有设备但尚未初始化,2-有设备但初始化枚举失败,3-有设备且初始化枚举成功
  UINT8	DeviceAddress;			// 设备被分配的USB地址
  UINT8	DeviceSpeed;			// 0为低速,非0为全速
  UINT8	DeviceType;				// 设备类型
  UINT8	GpVar;					// 通用变量
} ;  // 假定:不超过三个外部HUB,每个外部HUB不超过4个端口(多了不管)

#endif

/////////// 动态集线器信息分配表
