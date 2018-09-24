/* 以下为USB设备方式的应用程序 */
/*
; 单片机内置USB调试固件程序  V1.0
; 用于连接CH374的单片机进行简单的调试功能
; 可以用include直接包含到应用系统的主程序中,或者添加到工程项目中
;
; Website:  http://winchiphead.com
; Email:    tech@winchiphead.com
; @2004.08
;****************************************************************************
*/

/* MCS-51单片机C语言, KC7.0 */
/* 用于其它类型单片机或者硬件资源不同时, 该程序应该根据需要进行局部修改 */

#define		CH374HF_NO_CODE		1
#include "CH374.H"

#ifdef __C51__
#pragma NOAREGS
#endif

#ifndef MAX_DATA_SIZE
#define MAX_DATA_SIZE		20			/* 单次命令处理的最大数据长度,有效值是1到56 */
#endif

typedef	struct	_USB_DOWN_PACKET {		/* 下传的数据包结构,用于命令/写数据 */
	UINT8	mCommand;					/* 命令码,见下面的定义 */
	UINT8	mCommandNot;				/* 命令码的反码,用于校验下传数据包 */
	union {
		UINT8	mByte[4];				/* 通用参数 */
		UINT16	mWord[2];				/* 通用参数,低字节在前,Little-Endian */
		UINT32	mDword;					/* 通用参数,低字节在前,Little-Endian */
		void	*mAddress;				/* 读写操作的起始地址,低字节在前,Little-Endian */
	} u;
	UINT8	mLength;					/* 下面的缓冲区的长度,读写操作的字节数 */
	UINT8	mBuffer[ MAX_DATA_SIZE ];	/* 数据缓冲区 */
}	USB_DOWN_PKT;

typedef	struct	_USB_UP_PACKET {		/* 上传的数据包结构,用于状态/读数据 */
	UINT8	mStatus;					/* 状态码,见下面的定义 */
	UINT8	mCommandNot;				/* 命令码的反码,用于校验上传数据包 */
	UINT8	mReserved[4];
	UINT8	mLength;					/* 下面的缓冲区的长度,读操作的字节数 */
	UINT8	mBuffer[ MAX_DATA_SIZE ];	/* 数据缓冲区 */
}	USB_UP_PKT;

typedef union	_USB_DATA_PACKET {		/* USB上传或者下传数据缓冲区 */
	USB_DOWN_PKT	down;
	USB_UP_PKT		up;
}	USB_DATA_PKT;

/* 命令码定义,按位说明
   位7为命令类型:  0=实现特定功能, 1=存储器和SFR读写
   对于"实现特定功能"命令类型:
       位6-位0为定义的具体命令码, 命令码为00H-7FH, 其中: 00H-3FH为通用标准命令, 40H-7FH为与应用系统有关的特定命令
       目前版本定义了以下通用标准命令:
           00H: 获取调试固件程序的版本,并取消未完成的上传数据块
           10H: 获取当前应用系统的版本和说明字符串
   对于"存储器和SFR读写"命令类型:
       位6为数据传输方向:      0=读操作/上传, 1=写操作/下传
       位5-位4为数据读写宽度:  00=以字节为单位/8位, 01=以字为单位/16位, 10=以双字为单位/32位, 11=以位为单位/1位
       位1-位0为存储器空间:    00=存取SFR, 01=存取内部RAM, 10=存取外部RAM, 11=存取程序ROM
       例如: 命令码80H为读SFR, 命令码83H为读程序ROM, 命令码C1H为写内部RAM, 命令码C2H为写外部RAM
   状态码定义: 00H为操作成功, 080H为命令不支持, 0FFH为未定义的错误 */

#define USB_CMD_GET_FW_INFO		0x00
#define USB_CMD_GET_APP_INFO	0x10

#define USB_CMD_MEM_ACCESS		0x80
#define USB_CMD_MEM_DIR_WR		0x40
#define USB_CMD_MEM_WIDTH		0x0C
#define USB_CMD_MEM_W_BYTE		0x00
#define USB_CMD_MEM_W_WORD		0x04
#define USB_CMD_MEM_W_DWORD		0x08
#define USB_CMD_MEM_W_BIT		0x0C
#define USB_CMD_MEM_SPACE		0x03
#define USB_CMD_MEM_S_SFR		0x00
#define USB_CMD_MEM_S_IRAM		0x01
#define USB_CMD_MEM_S_XRAM		0x02
#define USB_CMD_MEM_S_ROM		0x03

#define ERR_SUCCESS				0x00
#define ERR_PARAMETER			0x10
#define ERR_UNSUPPORT			0x80
#define ERR_UNDEFINED			0xFF

#define THIS_FIRMWARE_VER		0x10
#define THIS_APP_SYS_VER		0x09
#define THIS_APP_SYS_STR		"CH374+MCS51"


/*UINT8V	FreeUSBmS;*/
#define	FreeUSBmS				CH374DiskStatus					/* 节约占用的内存,因为USB主从不会同时运行,所以USB主机的变量可以用于USB设备 */

/* USB结构和常量 */

typedef struct _USB_SETUP_REQ {
	UINT8	bType;
	UINT8	bReq;
	UINT8	wValueL;
	UINT8	wValueH;
	UINT8	wIndexL;
	UINT8	wIndexH;
	UINT8	wLengthL;
	UINT8	wLengthH;
} USB_SETUP_REQ, *PUSB_SETUP_REQ;

// 设备描述符
const	UINT8	code	MyDevDescr[] = { 0x12, 0x01, 0x10, 0x01,
								0xFF, 0x80, 0x37, 0x08,
								0x48, 0x43, 0x37, 0x55,  // 厂商ID和产品ID
								0x00, 0x01, 0x01, 0x02,
								0x00, 0x01 };
// 配置描述符
const	UINT8	code	MyCfgDescr[] = { 0x09, 0x02, 0x27, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
								 0x09, 0x04, 0x00, 0x00, 0x03, 0xFF, 0x80, 0x37, 0x00,
								 0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00,
								 0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
								 0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x00 };
// 语言描述符
const	UINT8	code	MyLangDescr[] = { 0x04, 0x03, 0x09, 0x04 };
// 厂家信息
const	UINT8	code	MyManuInfo[] = { 0x0E, 0x03, 'w', 0, 'c', 0, 'h', 0, '.', 0, 'c', 0, 'n', 0 };
// 产品信息
const	UINT8	code	MyProdInfo[] = { 0x0C, 0x03, 'C', 0, 'H', 0, '3', 0, '7', 0, '4', 0 };

UINT8	UsbConfig = 0;	// USB配置标志

/* 与CH374有关的基本I/O操作,以上是标准总线并口,参考EVT/EXAM目录下有模拟并口/SPI接口的接口子程序 */
//UINT8XV	CH374_IDX_PORT	_at_ 0xBDF1;	/* 假定CH374索引端口的I/O地址 */
//UINT8XV	CH374_DAT_PORT	_at_ 0xBCF0;	/* 假定CH374数据端口的I/O地址 */

#define	Write374Index( a )	{ CH374_IDX_PORT = a; }	/* 向索引端口写入索引地址 */
//void Write374Index( UINT8 mIndex )  /* 向CH374写索引地址 */
//{
//	CH374_IDX_PORT = mIndex;
//}

#define	Write374Data( d )	{ CH374_DAT_PORT = d; }	/* 向数据端口写入数据,索引地址自动加1 */
//void Write374Data( UINT8 mData )  /* 向CH374写数据 */
//{
//	CH374_DAT_PORT = mData;
//}

#define	Read374Data( )		( CH374_DAT_PORT )		/* 从数据端口读出数据,索引地址自动加1 */
//UINT8 Read374Data( void )  /* 从CH374读数据 */
//{
//	return( CH374_DAT_PORT );
//}

#define	Read374Data0( )		( CH374_IDX_PORT )		/* 从索引端口读出数据，索引地址不变，适用于[读出->修改->写回]操作 */
//UINT8 Read374Data0( void )  /* 从CH374读数据 */
//{
//	return( CH374_IDX_PORT );
//}

UINT8	Read374Byte( UINT8 mAddr )  /* 从指定寄存器读取数据 */
{
	Write374Index( mAddr );
	return( Read374Data( ) );
}

void	Write374Byte( UINT8 mAddr, UINT8 mData )  /* 向指定寄存器写入数据 */
{
	Write374Index( mAddr );
	Write374Data( mData );
}

void	Read374Block( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* 从指定起始地址读出数据块 */
{
	Write374Index( mAddr );
	while ( mLen -- ) *mBuf++ = Read374Data( );
}

void	Write374Block( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* 向指定起始地址写入数据块 */
{
	Write374Index( mAddr );
	while ( mLen -- ) Write374Data( *mBuf++ );
}

/* CH374初始化子程序 */
void	CH374DeviceInit( void ) {
	Write374Byte( REG_USB_SETUP, 0x00 );
	Write374Byte( REG_USB_ADDR, 0x00 );
	Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_NAK( 0 ) );
	Write374Byte( REG_USB_ENDP1, M_SET_EP1_TRAN_NAK( 0 ) );
	Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_NAK( 0 ) );
	Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  // 清所有中断标志
	Write374Byte( REG_INTER_EN, BIT_IE_TRANSFER | BIT_IE_BUS_RESET | BIT_IE_USB_SUSPEND );  // 允许传输完成中断和USB总线复位中断以及USB总线挂起中断,芯片唤醒完成中断
	Write374Byte( REG_SYS_CTRL, BIT_CTRL_OE_POLAR );  // 对于CH374T或者UEN引脚悬空的CH374S必须置BIT_CTRL_OE_POLAR为1
	Write374Byte( REG_USB_SETUP, BIT_SETP_TRANS_EN | BIT_SETP_PULLUP_EN );  // 启动USB设备
/* 下面启用USB中断,CH374的INT#引脚可以连接到单片机的中断引脚,中断为低电平有效或者下降沿有效,
   如果不使用中断,那么也可以用查询方式,由单片机程序查询CH374的INT#引脚为低电平 */
	IT0 = 0;  /* 置外部信号为低电平触发 */
	IE0 = 0;  /* 清中断标志 */
	EX0 = 1;  /* 允许CH374中断,假定CH374的INT#引脚连接到单片机的INT0 */
}

/* CH374中断服务程序,假定CH374的INT#引脚连接到单片机的INT0,使用寄存器组1 */
void	mCH374Interrupt( void ) interrupt 0 using 1 {
/*	UINT8			s;*/
/*	UINT8			l;*/
/*	UINT8			dat;*/
#define	s				CH374vDevEndpTog				/* 节约占用的内存,因为USB主从不会同时运行,所以USB主机的变量可以用于USB设备 */
#define	l				CH374vDiskFat					/* 节约占用的内存,因为USB主从不会同时运行,所以USB主机的变量可以用于USB设备 */
#define	dat				CH374vSecPerClus				/* 节约占用的内存,因为USB主从不会同时运行,所以USB主机的变量可以用于USB设备 */
	PUINT8C			str;
/*	USB_DATA_PKT	udp;*/
#define	pudp		( (USB_DATA_PKT *)&mCmdParam )	/* 节约结构变量占用的内存,因为USB主从不会同时运行,所以USB主机的变量可以用于USB设备 */
	static	UINT8X	SetupReq, SetupLen;
	static	PUINT8	pDescr;

	s = Read374Byte( REG_INTER_FLAG );  // 获取中断状态
	FreeUSBmS = 0;  /* 清除USB空闲计时 */
	if ( s & BIT_IF_BUS_RESET ) {  // USB总线复位
		Write374Byte( REG_USB_ADDR, 0x00 );  // 清USB设备地址
		Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_NAK( 0 ) );
		Write374Byte( REG_USB_ENDP1, M_SET_EP1_TRAN_NAK( 0 ) );
		Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_NAK( 0 ) );
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_BUS_RESET );  // 清中断标志
	}
	else if ( s & BIT_IF_TRANSFER ) {  // USB传输完成
		s = Read374Byte( REG_USB_STATUS );
		switch( s & BIT_STAT_PID_ENDP ) {  // USB设备中断状态
			case USB_INT_EP2_OUT: {  // 批量端点下传成功 
				if ( s & BIT_STAT_TOG_MATCH ) {  // 仅同步包
					l = Read374Byte( REG_USB_LENGTH );
					Read374Block( RAM_ENDP2_RECV, l, (PUINT8)&pudp->down );
					if ( l == 0 ) {  /* 长度为0,没有数据,在某些应用中也可以将长度0定义为一种特殊命令 */
						Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_NAK( Read374Byte( REG_USB_ENDP2 ) ) );  /* 同步触发位不变,设置USB端点2的IN正忙,返回NAK */
					}
					else if ( pudp->down.mCommand == (UINT8)( ~ pudp->down.mCommandNot ) ) {  /* 命令包反码校验通过,否则放弃该下传包 */
						if ( pudp->down.mCommand & USB_CMD_MEM_ACCESS ) {  /* 命令类型:存储器和SFR读写 */
							if ( ( pudp->down.mCommand & USB_CMD_MEM_WIDTH ) != USB_CMD_MEM_W_BYTE ) {  /* 本程序目前对MCS51只支持以字节为单位进行读写 */
								pudp->up.mLength = 0;
								pudp->up.mStatus = ERR_UNSUPPORT;  /* 命令不支持 */
							}
							else {  /* 以字节为单位进行读写 */
								for ( l = 0; l != pudp->down.mLength; l ++ ) {  /* 读写操作计数 */
									dat = pudp->down.mBuffer[ l ];  /* 准备写入的数据 */
									switch( pudp->down.mCommand & USB_CMD_MEM_SPACE ) {  /* 存储器空间 */
										case USB_CMD_MEM_S_SFR:
											switch ( pudp->down.u.mByte[0] ) {  /* 分析SFR地址 */
												case 0x80:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) P0 = dat;
													else dat = P0;
													break;
												case 0x87:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) PCON = dat;
													else dat = PCON;
													break;
												case 0x88:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) TCON = dat;
													else dat = TCON;
													break;
												case 0x89:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) TMOD = dat;
													else dat = TMOD;
													break;
												case 0x90:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) P1 = dat;
													else dat = P1;
													break;
												case 0x98:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) SCON = dat;
													else dat = SCON;
													break;
												case 0x99:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) SBUF = dat;
													else dat = SBUF;
													break;
												case 0xA0:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) P2 = dat;
													else dat = P2;
													break;
												case 0xA8:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) IE = dat;
													else dat = IE;
													break;
												case 0xB0:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) P3 = dat;
													else dat = P3;
													break;
												case 0xB8:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) IP = dat;
													else dat = IP;
													break;
												case 0xC8:
													if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) T2CON = dat;
													else dat = T2CON;
													break;
												default:
													dat = 0;
													break;
											}
											break;
										case USB_CMD_MEM_S_IRAM:
											if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) *(PUINT8)pudp->down.u.mByte[0] = dat;
											else dat = *(PUINT8)pudp->down.u.mByte[0];
											break;
										case USB_CMD_MEM_S_XRAM:
											if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) *(PUINT8X)( pudp->down.u.mByte[0] | (UINT16)pudp->down.u.mByte[1] << 8 )= dat;
											else dat = *(PUINT8X)( pudp->down.u.mByte[0] | (UINT16)pudp->down.u.mByte[1] << 8 );
											break;
										case USB_CMD_MEM_S_ROM:
											if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) pudp->up.mStatus = ERR_UNSUPPORT;  /* 命令不支持 */
											else dat = *(PUINT8C)( pudp->down.u.mByte[0] | (UINT16)pudp->down.u.mByte[1] << 8 );
											break;
									}
									if ( ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) == 0 ) pudp->up.mBuffer[ l ] = dat;  /* 返回读出的数据 */
									pudp->down.u.mByte[0] ++;
									if ( pudp->down.u.mByte[0] == 0 ) pudp->down.u.mByte[1] ++;
								}
								if ( pudp->down.mCommand & USB_CMD_MEM_DIR_WR ) pudp->up.mLength = 0;  /* 写操作不返回数据 */
								pudp->up.mStatus = ERR_SUCCESS;
							}
						}
						else switch ( pudp->down.mCommand ) {  /* 命令类型:实现特定功能,分析命令码 */
							case USB_CMD_GET_FW_INFO:  /* 获取调试固件程序的版本,并取消未完成的上传数据块 */
								pudp->up.mBuffer[0] = THIS_FIRMWARE_VER;
								pudp->up.mLength = 1;
								pudp->up.mStatus = ERR_SUCCESS;
								Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_NAK( Read374Byte( REG_USB_ENDP2 ) ) );  /* 同步触发位不变,设置USB端点2的IN正忙,返回NAK */
								break;
							case USB_CMD_GET_APP_INFO:  /* 获取当前应用系统的版本和说明字符串 */
								pudp->up.mBuffer[0] = THIS_APP_SYS_VER;
								l = 0;
								str = THIS_APP_SYS_STR;
								while ( pudp->up.mBuffer[ l ] = *str ) { l ++; str ++; }  /* 说明字符串 */
								pudp->up.mLength = 1 + sizeof( THIS_APP_SYS_STR );
								pudp->up.mStatus = ERR_SUCCESS;
								break;
/* 							case MY_CMD_CH451: */
							default:  /* 命令不支持 */
								pudp->up.mLength = 0;
								pudp->up.mStatus = ERR_UNSUPPORT;
								break;
						}
						l = pudp->up.mLength + (UINT8)( & ( (USB_UP_PKT *)0 ) -> mBuffer );
						Write374Byte( REG_USB_LENGTH, l );
						Write374Block( RAM_ENDP2_TRAN, l, (PUINT8)&pudp->up );  // 向USB端点2的发送缓冲区写入数据块
						Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_ACK( Read374Byte( REG_USB_ENDP2 ) ) ^ BIT_EP2_RECV_TOG );
//						Write374Index( REG_USB_ENDP2 );  // 对于并口连接可以用本行及下面一行代替上一行的程序,减少写一次index的时间,提高效率
//						Write374Data( M_SET_EP2_TRAN_ACK( Read374Data0( ) ) ^ BIT_EP2_RECV_TOG );
					}
				}
				break;
			}
			case USB_INT_EP2_IN: {  // 批量端点上传成功,未处理
				Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_NAK( Read374Byte( REG_USB_ENDP2 ) ) ^ BIT_EP2_TRAN_TOG );
//				Write374Index( REG_USB_ENDP2 );  // 对于并口连接可以用本行及下面一行代替上一行的程序,减少写一次index的时间,提高效率
//				Write374Data( M_SET_EP2_TRAN_NAK( Read374Data0( ) ) ^ BIT_EP2_TRAN_TOG );
				break;
			}
			case USB_INT_EP1_IN: {  // 中断端点上传成功,未处理
				Write374Byte( REG_USB_ENDP1, M_SET_EP1_TRAN_NAK( Read374Byte( REG_USB_ENDP1 ) ) ^ BIT_EP1_TRAN_TOG );
				break;
			}
			case USB_INT_EP0_SETUP: {  // 控制传输
				USB_SETUP_REQ	SetupReqBuf;
				l = Read374Byte( REG_USB_LENGTH );
				if ( l == sizeof( USB_SETUP_REQ ) ) {
					Read374Block( RAM_ENDP0_RECV, l, (PUINT8)&SetupReqBuf );
					SetupLen = SetupReqBuf.wLengthL;
					if ( SetupReqBuf.wLengthH || SetupLen > 0x7F ) SetupLen = 0x7F;  // 限制总长度
					l = 0;  // 默认为成功并且上传0长度
					if ( ( SetupReqBuf.bType & DEF_USB_REQ_TYPE ) != DEF_USB_REQ_STAND ) {  /* 只支持标准请求 */
						l = 0xFF;  // 操作失败
					}
					else {  // 标准请求
						SetupReq = SetupReqBuf.bReq;  // 请求码
						switch( SetupReq ) {
							case DEF_USB_GET_DESCR:
								switch( SetupReqBuf.wValueH ) {
									case 1:
										pDescr = (PUINT8)( &MyDevDescr[0] );
										l = sizeof( MyDevDescr );
										break;
									case 2:
										pDescr = (PUINT8)( &MyCfgDescr[0] );
										l = sizeof( MyCfgDescr );
										break;
									case 3:
										switch( SetupReqBuf.wValueL ) {
											case 1:
												pDescr = (PUINT8)( &MyManuInfo[0] );
												l = sizeof( MyManuInfo );
												break;
											case 2:
												pDescr = (PUINT8)( &MyProdInfo[0] );
												l = sizeof( MyProdInfo );
												break;
											case 0:
												pDescr = (PUINT8)( &MyLangDescr[0] );
												l = sizeof( MyLangDescr );
												break;
											default:
												l = 0xFF;  // 操作失败
												break;
										}
										break;
									default:
										l = 0xFF;  // 操作失败
										break;
								}
								if ( SetupLen > l ) SetupLen = l;  // 限制总长度
								l = SetupLen >= RAM_ENDP0_SIZE ? RAM_ENDP0_SIZE : SetupLen;  // 本次传输长度
								Write374Block( RAM_ENDP0_TRAN, l, pDescr );  /* 加载上传数据 */
								SetupLen -= l;
								pDescr += l;
								break;
							case DEF_USB_SET_ADDRESS:
								SetupLen = SetupReqBuf.wValueL;  // 暂存USB设备地址
								break;
							case DEF_USB_GET_CONFIG:
								Write374Byte( RAM_ENDP0_TRAN, UsbConfig );
								if ( SetupLen >= 1 ) l = 1;
								break;
							case DEF_USB_SET_CONFIG:
								UsbConfig = SetupReqBuf.wValueL;
								break;
							case DEF_USB_CLR_FEATURE:
								if ( ( SetupReqBuf.bType & 0x1F ) == 0x02 ) {  // 不是端点不支持
									switch( SetupReqBuf.wIndexL ) {
										case 0x82:
											Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_NAK( Read374Byte( REG_USB_ENDP2 ) ) );
											break;
										case 0x02:
											Write374Byte( REG_USB_ENDP2, M_SET_EP2_RECV_ACK( Read374Byte( REG_USB_ENDP2 ) ) );
											break;
										case 0x81:
											Write374Byte( REG_USB_ENDP1, M_SET_EP1_TRAN_NAK( Read374Byte( REG_USB_ENDP1 ) ) );
											break;
										case 0x01:
											Write374Byte( REG_USB_ENDP1, M_SET_EP1_RECV_ACK( Read374Byte( REG_USB_ENDP1 ) ) );
											break;
										default:
											l = 0xFF;  // 操作失败
											break;
									}
								}
								else l = 0xFF;  // 操作失败
								break;
							case DEF_USB_GET_INTERF:
								Write374Byte( RAM_ENDP0_TRAN, 0 );
								if ( SetupLen >= 1 ) l = 1;
								break;
							case DEF_USB_GET_STATUS:
								Write374Byte( RAM_ENDP0_TRAN, 0 );
								Write374Byte( RAM_ENDP0_TRAN + 1, 0 );
								if ( SetupLen >= 2 ) l = 2;
								else l = SetupLen;
								break;
							default:
								l = 0xFF;  // 操作失败
								break;
						}
					}
				}
				else l = 0xFF;  // 操作失败
				if ( l == 0xFF ) {  // 操作失败
					Write374Byte( REG_USB_ENDP0, M_SET_EP0_RECV_STA( M_SET_EP0_TRAN_STA( 0 ) ) );  // STALL
				}
				else if ( l <= RAM_ENDP0_SIZE ) {  // 上传数据
					Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_ACK( M_SET_EP0_RECV_ACK( Read374Byte( REG_USB_ENDP0 ) ), l ) | BIT_EP0_TRAN_TOG );  // DATA1
				}
				else {  // 下传数据或其它
					Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_NAK( M_SET_EP0_RECV_ACK( Read374Byte( REG_USB_ENDP0 ) ) ) | BIT_EP0_RECV_TOG );  // DATA1
				}
				break;
			}
			case USB_INT_EP0_IN: {
				switch( SetupReq ) {
					case DEF_USB_GET_DESCR:
						l = SetupLen >= RAM_ENDP0_SIZE ? RAM_ENDP0_SIZE : SetupLen;  // 本次传输长度
						Write374Block( RAM_ENDP0_TRAN, l, pDescr );  /* 加载上传数据 */
						SetupLen -= l;
						pDescr += l;
						Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_ACK( Read374Byte( REG_USB_ENDP0 ), l ) ^ BIT_EP0_TRAN_TOG );
						break;
					case DEF_USB_SET_ADDRESS:
						Write374Byte( REG_USB_ADDR, SetupLen );
					default:
						Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_NAK( 0 ) );  // 结束
						break;
				}
				break;
			}
			case USB_INT_EP0_OUT: {
				switch( SetupReq ) {
//					case download:
//						get_data;
//						break;
					case DEF_USB_GET_DESCR:
					default:
						Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_NAK( 0 ) );  // 结束
						break;
				}
				break;
			}
			default: {
				break;
			}
		}
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_TRANSFER );  // 清中断标志
	}
	else if ( s & BIT_IF_USB_SUSPEND ) {  // USB总线挂起
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_USB_SUSPEND );  // 清中断标志
		Write374Byte( REG_SYS_CTRL, Read374Byte( REG_SYS_CTRL ) | BIT_CTRL_OSCIL_OFF );  // 时钟振荡器停止振荡,进入睡眠状态
	}
	else if ( s & BIT_IF_WAKE_UP ) {  // 芯片唤醒完成
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_WAKE_UP );  // 清中断标志
	}
	else {  // 意外的中断,不可能发生的情况,除了硬件损坏
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  // 清中断标志
	}
/*	IE0 = 0;  清中断标志,与单片机硬件有关,对应于INT0中断 */
}

/* 关闭CH374的所有USB通讯 */
void	CH374OffUSB( void ) {
	EX0 = 0;  /* 关闭USB中断,本程序中USB主机模式下使用查询方式 */
	Write374Byte( REG_USB_SETUP, 0x00 );  /* 关闭USB操作 */
//	CH374Reset( );  /* 复位也可以 */
	CH374DelaymS( 10 );  /* 为USB主从切换进行时间缓冲,这是必要的延时操作,用于让计算机认为USB设备已经撤离 */
/* 如果CH374仍然连接着计算机,而程序使CH374切换到USB主机模式,那么会导致与计算机之间双USB主机冲突 */
}

void device( ) {
	CH374DeviceInit( );  /* 初始化USB设备模式 */
	FreeUSBmS = 0;  /* 清除USB空闲计时 */
	while( 1 ) {
		if ( IsKeyPress( ) ) {  /* 有键按下 */
			if ( FreeUSBmS >= 250 ) {  /* USB空闲超过250毫秒 */
				printf( "Exit USB device mode\n" );
				CH374OffUSB( );  /* 关闭USB设备 */
				return;
			}
		}
		if ( FreeUSBmS < 250 ) FreeUSBmS ++;  /* USB空闲计时,避免在USB通讯过程中由用户按键导致USB主从切换 */
		CH374DelaymS( 1 );
/* USB设备模式全部在中断服务中处理,主程序可以做其它事情,当然也可以在主程序中使用查询方式处理USB设备的通讯 */
	}
}
