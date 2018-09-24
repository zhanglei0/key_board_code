/* CH374芯片 应用层 V1.0 */
/* USB主机,初始化和枚举当前连接的USB设备,适用于单HUB端口HUB0中的全速USB设备 */

#include	<stdio.h>
#include	<string.h>
#include	"..\HAL.H"			// 以MCS51为例，其它单片机需修改HAL*硬件抽象层的几个文件
#include	"..\HAL_BASE.C"	// 基本子程序及中断查询子程序

/* 硬件接口层,以下连接方式任选一种 */
#include "..\PARA_HW.C"	/* 硬件标准8位并口 */
//#include "..\PARA_SW.C"	/* 软件I/O模拟8位并口 */
//#include "..\SPI_HW.C"	/* 硬件标准4线SPI串口 */
//#include "..\SPI_SW.C"	/* 软件I/O模拟4线SPI串口 */
//#include "..\SPI3_SW.C"	/* 软件I/O模拟3线SPI串口,SDO和SDI合用一个引脚 */

// 获取设备描述符
const	UINT8C	SetupGetDevDescr[] = { 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00 };
// 获取配置描述符
const	UINT8C	SetupGetCfgDescr[] = { 0x80, 0x06, 0x00, 0x02, 0x00, 0x00, 0x04, 0x00 };
// 设置USB地址
const	UINT8C	SetupSetUsbAddr[] = { 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
// 设置USB配置
const	UINT8C	SetupSetUsbConfig[] = { 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

UINT8	UsbDevEndpSize = DEFAULT_ENDP0_SIZE;	/* USB设备的端点0的最大包尺寸 */

UINT8	FlagDeviceStatus;						/* 当前USB设备状态，通常用于中断方式的全局变量，本程序中未使用 */

// CH374传输事务，输入目的端点地址/PID令牌/同步标志，返回同CH375，NAK不重试，超时/出错重试
UINT8	HostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog );

// CH374传输事务，输入目的端点地址/PID令牌/同步标志/以mS为单位的NAK重试总时间(0xFFFF无限重试)，返回同CH375，NAK重试，超时出错重试
UINT8	WaitHostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog, UINT16 timeout );

UINT8	HostCtrlTransfer374( PUINT8 ReqBuf, PUINT8 DatBuf, PUINT8 RetLen );  // 执行控制传输,ReqBuf指向8字节请求码,DatBuf为收发缓冲区
// 如果需要接收和发送数据，那么DatBuf需指向有效缓冲区用于存放后续数据，实际成功收发的总长度保存在ReqLen指向的字节变量中

// 查询当前是否存在USB设备
//BOOL	Query374DeviceIn( void );
#define	Query374DeviceIn( )	( ( Read374Byte( REG_INTER_FLAG ) & BIT_IF_DEV_ATTACH ) ? TRUE : FALSE )

// 查询当前的USB设备是全速还是低速, 返回TRUE为全速
//BOOL	Query374DevFullSpeed( void );
#define	Query374DevFullSpeed( )	( ( Read374Byte( REG_SYS_INFO ) & BIT_INFO_USB_DP ) ? TRUE : FALSE )

void	HostDetectInterrupt( void );  // 处理USB设备插拔事件中断

void	SetHostUsbAddr( UINT8 addr );  // 设置USB主机当前操作的USB设备地址

void	HostSetBusFree( void );  // USB总线空闲

void	HostSetBusReset( void );  // USB总线复位

void	HostSetFullSpeed( void );  // 设定全速USB设备运行环境

void	HostSetLowSpeed( void );  // 设定低速USB设备运行环境,建议参考EMB_HUB中的低速例子处理

void	Init374Host( void );  // 初始化USB主机

UINT8	GetDeviceDescr( PUINT8 buf );  // 获取设备描述符

UINT8	GetConfigDescr( PUINT8 buf );  // 获取配置描述符

UINT8	SetUsbAddress( UINT8 addr );  // 设置USB设备地址

UINT8	SetUsbConfig( UINT8 cfg );  // 设置USB设备配置


// CH374传输事务，输入目的端点地址/PID令牌/同步标志，返回同CH375，NAK不重试，超时/出错重试
UINT8	HostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog )
{  // 本子程序着重于易理解,而在实际应用中,为了提供运行速度,应该对本子程序代码进行优化
	UINT8	retry;
	UINT8	s, r, u;
	for ( retry = 0; retry < 3; retry ++ ) {
		Write374Byte( REG_USB_H_PID, M_MK_HOST_PID_ENDP( pid, endp_addr ) );  // 指定令牌PID和目的端点号
//		Write374Byte( REG_USB_H_CTRL, BIT_HOST_START | ( tog ? ( BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : 0x00 ) );  // 设置同步标志并启动传输
		Write374Byte( REG_USB_H_CTRL, ( tog ? ( BIT_HOST_START | BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : BIT_HOST_START ) );  // 设置同步标志并启动传输
//		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE );  // 取消暂停
		s = Wait374Interrupt( );
		if ( s == ERR_USB_UNKNOWN ) return( s );  // 中断超时,可能是硬件异常
		s = Read374Byte( REG_INTER_FLAG );  // 获取中断状态
		if ( s & BIT_IF_DEV_DETECT ) {  // USB设备插拔事件
			mDelayuS( 200 );  // 等待传输完成
			Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_TRANSFER );  // 清中断标志
			if ( s & BIT_IF_DEV_ATTACH ) {  // USB设备连接事件
				u = Read374Byte( REG_USB_SETUP );
				if ( s & BIT_IF_USB_DX_IN ) {  // 速度匹配，不需要切换速度
					if ( u & BIT_SETP_USB_SPEED ) return( USB_INT_CONNECT_LS );  // 低速USB设备
					return( USB_INT_CONNECT );  // 全速USB设备
				}
				else {  // 速度失配，需要切换速度
					if ( u & BIT_SETP_USB_SPEED ) return( USB_INT_CONNECT );  // 全速USB设备
					return( USB_INT_CONNECT_LS );  // 低速USB设备
				}
			}
			else return( USB_INT_DISCONNECT );  // USB设备断开事件
		}
		else if ( s & BIT_IF_TRANSFER ) {  // 传输完成
			Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_TRANSFER );  // 清中断标志
			s = Read374Byte( REG_USB_STATUS );  // USB状态
			r = s & BIT_STAT_DEV_RESP;  // USB设备应答状态
			switch ( pid ) {
				case DEF_USB_PID_SETUP:
				case DEF_USB_PID_OUT:
					if ( r == DEF_USB_PID_ACK ) return( USB_INT_SUCCESS );
					else if ( r == DEF_USB_PID_STALL || r == DEF_USB_PID_NAK ) return( r | 0x20 );
					else if ( ! M_IS_HOST_TIMEOUT( s ) ) return( r | 0x20 );  // 不是超时/出错，意外应答
					break;
				case DEF_USB_PID_IN:
					if ( M_IS_HOST_IN_DATA( s ) ) {  // DEF_USB_PID_DATA0 or DEF_USB_PID_DATA1
						if ( s & BIT_STAT_TOG_MATCH ) return( USB_INT_SUCCESS );  // 不同步则需丢弃后重试
					}
					else if ( r == DEF_USB_PID_STALL || r == DEF_USB_PID_NAK ) return( r | 0x20 );
					else if ( ! M_IS_HOST_TIMEOUT( s ) ) return( r | 0x20 );  // 不是超时/出错，意外应答
					break;
				default:
					return( ERR_USB_UNKNOWN );  // 不可能的情况
					break;
			}
		}
		else {  // 其它中断,不应该发生的情况
			mDelayuS( 200 );  // 等待传输完成
			Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  /* 清中断标志 */
			if ( retry ) return( ERR_USB_UNKNOWN );  /* 不是第一次检测到则返回错误 */
		}
	}
	return( 0x20 );  // 应答超时
}

// CH374传输事务，输入目的端点地址/PID令牌/同步标志/以mS为单位的NAK重试总时间(0xFFFF无限重试)，返回同CH375，NAK重试，超时出错重试
UINT8	WaitHostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog, UINT16 timeout )
{
	UINT8	i, s;
	while ( 1 ) {
		for ( i = 0; i < 40; i ++ ) {
			s = HostTransact374( endp_addr, pid, tog );
			if ( s != ( DEF_USB_PID_NAK | 0x20 ) || timeout == 0 ) return( s );
			mDelayuS( 20 );
		}
		if ( timeout < 0xFFFF ) timeout --;
	}
}

UINT8	HostCtrlTransfer374( PUINT8 ReqBuf, PUINT8 DatBuf, PUINT8 RetLen )  // 执行控制传输,ReqBuf指向8字节请求码,DatBuf为收发缓冲区
// 如果需要接收和发送数据，那么DatBuf需指向有效缓冲区用于存放后续数据，实际成功收发的总长度保存在ReqLen指向的字节变量中
{
	UINT8	s, len, count, total;
	BOOL	tog;
	Write374Block( RAM_HOST_TRAN, 8, ReqBuf );
	Write374Byte( REG_USB_LENGTH, 8 );
	mDelayuS( 100 );
	s = WaitHostTransact374( 0, DEF_USB_PID_SETUP, FALSE, 200 );  // SETUP阶段，200mS超时
	if ( s == USB_INT_SUCCESS ) {  // SETUP成功
		tog = TRUE;  // 默认DATA1,默认无数据故状态阶段为IN
		total = *( ReqBuf + 6 );
		if ( total && DatBuf ) {  // 需要收发数据
			len = total;
			if ( *ReqBuf & 0x80 ) {  // 收
				while ( len ) {
					mDelayuS( 100 );
					s = WaitHostTransact374( 0, DEF_USB_PID_IN, tog, 200 );  // IN数据
					if ( s != USB_INT_SUCCESS ) break;
					count = Read374Byte( REG_USB_LENGTH );
					Read374Block( RAM_HOST_RECV, count, DatBuf );
					DatBuf += count;
					if ( count <= len ) len -= count;
					else len = 0;
					if ( count == 0 || ( count & ( UsbDevEndpSize - 1 ) ) ) break;  // 短包
					tog = tog ? FALSE : TRUE;
				}
				tog = FALSE;  // 状态阶段为OUT
			}
			else {  // 发
				while ( len ) {
					mDelayuS( 100 );
					count = len >= UsbDevEndpSize ? UsbDevEndpSize : len;
					Write374Block( RAM_HOST_TRAN, count, DatBuf );
					Write374Byte( REG_USB_LENGTH, count );
					s = WaitHostTransact374( 0, DEF_USB_PID_OUT, tog, 200 );  // OUT数据
					if ( s != USB_INT_SUCCESS ) break;
					DatBuf += count;
					len -= count;
					tog = tog ? FALSE : TRUE;
				}
				tog = TRUE;  // 状态阶段为IN
			}
			total -= len;  // 减去剩余长度得实际传输长度
		}
		if ( s == USB_INT_SUCCESS ) {  // 数据阶段成功
			Write374Byte( REG_USB_LENGTH, 0 );
			mDelayuS( 100 );
			s = WaitHostTransact374( 0, ( tog ? DEF_USB_PID_IN : DEF_USB_PID_OUT ), TRUE, 200 );  // STATUS阶段
			if ( tog && s == USB_INT_SUCCESS ) {  // 检查IN状态返回数据长度
				if ( Read374Byte( REG_USB_LENGTH ) ) s = USB_INT_BUF_OVER;  // 状态阶段错误
			}
		}
	}
	if ( RetLen ) *RetLen = total;  // 实际成功收发的总长度
	return( s );
}

// 查询当前是否存在USB设备
//BOOL	Query374DeviceIn( void )
//#define	Query374DeviceIn( )	( ( Read374Byte( REG_INTER_FLAG ) & BIT_IF_DEV_ATTACH ) ? TRUE : FALSE )

// 查询当前的USB设备是全速还是低速, 返回TRUE为全速
//BOOL	Query374DevFullSpeed( void )
//#define	Query374DevFullSpeed( )	( ( Read374Byte( REG_SYS_INFO ) & BIT_INFO_USB_DP ) ? TRUE : FALSE )

void	HostDetectInterrupt( void )  // 处理USB设备插拔事件中断
{
	UINT8	s, u;
	s = Read374Byte( REG_INTER_FLAG );  // 获取中断状态
	if ( s & BIT_IF_DEV_DETECT ) {  // USB设备插拔事件
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT );  // 清中断标志
		if ( s & BIT_IF_DEV_ATTACH ) {  // USB设备连接事件
			u = Read374Byte( REG_USB_SETUP );
			if ( s & BIT_IF_USB_DX_IN ) {  // 速度匹配，不需要切换速度
				if ( u & BIT_SETP_USB_SPEED ) FlagDeviceStatus = USB_INT_CONNECT_LS;  // 低速USB设备
				else FlagDeviceStatus = USB_INT_CONNECT;  // 全速USB设备
			}
			else {  // 速度失配，需要切换速度
				if ( u & BIT_SETP_USB_SPEED ) FlagDeviceStatus = USB_INT_CONNECT;  // 全速USB设备
				else FlagDeviceStatus = USB_INT_CONNECT_LS;  // 低速USB设备
			}
		}
		else FlagDeviceStatus = USB_INT_DISCONNECT;  // USB设备断开事件
	}
	else {  // 意外的中断
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_TRANSFER | BIT_IF_USB_SUSPEND | BIT_IF_WAKE_UP );  // 清中断标志
	}
}

void	SetHostUsbAddr( UINT8 addr )  // 设置USB主机当前操作的USB设备地址
{
	Write374Byte( REG_USB_ADDR, addr );
}

void	HostSetBusFree( void )  // USB总线空闲
{
//	Write374Byte( REG_USB_SETUP, M_SET_USB_BUS_FREE( Read374Byte( REG_USB_SETUP ) ) );  // USB总线空闲
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE );  // USB主机方式
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // USB主机方式,允许SOF
}

void	HostSetBusReset( void )  // USB总线复位
{
	UsbDevEndpSize = DEFAULT_ENDP0_SIZE;  /* USB设备的端点0的最大包尺寸 */
	SetHostUsbAddr( 0x00 );
	Write374Byte( REG_USB_H_CTRL, 0x00 );
//	Write374Byte( REG_USB_SETUP, M_SET_USB_BUS_RESET( Read374Byte( REG_USB_SETUP ) & ~ BIT_SETP_AUTO_SOF ) );  // USB总线复位
	Write374Byte( REG_USB_SETUP, M_SET_USB_BUS_RESET( BIT_SETP_HOST_MODE ) );  // USB总线复位
	mDelaymS( 15 );  // USB总线复位期间,10mS到20mS
//	Write374Byte( REG_USB_SETUP, M_SET_USB_BUS_FREE( Read374Byte( REG_USB_SETUP ) ) );  // USB总线空闲
	HostSetBusFree( );  // USB总线空闲
	mDelaymS( 1 );
	Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_USB_SUSPEND );  // 清中断标志
}

void	HostSetFullSpeed( void )  // 设定全速USB设备运行环境
{
	Write374Byte( REG_USB_SETUP, Read374Byte( REG_USB_SETUP ) & ~ BIT_SETP_USB_SPEED | BIT_SETP_AUTO_SOF );  // 全速且发SOF
}

void	HostSetLowSpeed( void )  // 设定低速USB设备运行环境,建议参考EMB_HUB中的低速例子处理
{
	Write374Byte( REG_USB_SETUP, Read374Byte( REG_USB_SETUP ) | BIT_SETP_USB_SPEED | BIT_SETP_AUTO_SOF );  // 低速且发SOF
}

void	Init374Host( void )  // 初始化USB主机
{
	Write374Byte( REG_USB_SETUP, 0x00 );
	SetHostUsbAddr( 0x00 );
	Write374Byte( REG_USB_H_CTRL, 0x00 );
	Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  // 清所有中断标志
//	Write374Byte( REG_INTER_EN, BIT_IE_TRANSFER );  // 允许传输完成中断,因为本程序使用查询方式检测USB设备插拔,所以无需USB设备检测中断
	Write374Byte( REG_INTER_EN, BIT_IE_TRANSFER | BIT_IE_DEV_DETECT );  // 允许传输完成中断和USB设备检测中断
	Write374Byte( REG_SYS_CTRL, BIT_CTRL_OE_POLAR );  // 对于CH374T或者UEN引脚悬空的CH374S必须置BIT_CTRL_OE_POLAR为1
	HostSetBusFree( );  // USB总线空闲
}

UINT8	GetDeviceDescr( PUINT8 buf )  // 获取设备描述符
{
	UINT8	s, len;
	s = HostCtrlTransfer374( SetupGetDevDescr, buf, &len );  // 执行控制传输
	if ( s == USB_INT_SUCCESS ) {
		UsbDevEndpSize = ( (PUSB_DEV_DESCR)buf ) -> bMaxPacketSize0;  // 端点0最大包长度,这是简化处理,正常应该先获取前8字节后立即更新UsbDevEndpSize再继续
		if ( len < ( (PUSB_SETUP_REQ)SetupGetDevDescr ) -> wLengthL ) s = USB_INT_BUF_OVER;  // 描述符长度错误
	}
	return( s );
}

UINT8	GetConfigDescr( PUINT8 buf )  // 获取配置描述符
{
	UINT8	s, len;
	UINT8	BufLogDescr[ sizeof( SetupGetCfgDescr ) ] ;
	s = HostCtrlTransfer374( SetupGetCfgDescr, buf, &len );  // 执行控制传输
	if ( s == USB_INT_SUCCESS ) {
		if ( len < ( (PUSB_SETUP_REQ)SetupGetCfgDescr ) -> wLengthL ) s = USB_INT_BUF_OVER;  // 返回长度错误
		else {
			memcpy ( BufLogDescr, SetupGetCfgDescr, sizeof( SetupGetCfgDescr ) );
			( (PUSB_SETUP_REQ)BufLogDescr ) -> wLengthL = ( (PUSB_CFG_DESCR)buf ) -> wTotalLengthL;  // 完整配置描述符的总长度
			s = HostCtrlTransfer374( BufLogDescr, buf, &len );  // 执行控制传输
			if ( s == USB_INT_SUCCESS ) {
				if ( len < ( (PUSB_CFG_DESCR)buf ) -> wTotalLengthL ) s = USB_INT_BUF_OVER;  // 描述符长度错误
			}
		}
	}
	return( s );
}

UINT8	SetUsbAddress( UINT8 addr )  // 设置USB设备地址
{
	UINT8	s;
	UINT8	BufSetAddr[ sizeof( SetupSetUsbAddr ) ] ;
	memcpy ( BufSetAddr, SetupSetUsbAddr, sizeof( SetupSetUsbAddr ) );
	( (PUSB_SETUP_REQ)BufSetAddr ) -> wValueL = addr;  // USB设备地址
	s = HostCtrlTransfer374( BufSetAddr, NULL, NULL );  // 执行控制传输
	if ( s == USB_INT_SUCCESS ) {
		SetHostUsbAddr( addr );  // 设置USB主机当前操作的USB设备地址
	}
	mDelaymS( 3 );  // 等待USB设备完成操作
	return( s );
}

UINT8	SetUsbConfig( UINT8 cfg )  // 设置USB设备配置
{
	UINT8	BufSetCfg[ sizeof( SetupSetUsbConfig ) ] ;
	memcpy ( BufSetCfg, SetupSetUsbConfig, sizeof( SetupSetUsbConfig ) );
	( (PUSB_SETUP_REQ)BufSetCfg ) -> wValueL = cfg;  // USB设备配置
	return( HostCtrlTransfer374( BufSetCfg, NULL, NULL ) );  // 执行控制传输
}

/* 为printf和getkey输入输出初始化串口 */
void	mInitSTDIO( )
{
	SCON = 0x50;
	PCON = 0x80;
	TL2 = RCAP2L = 0 - 13; /* 24MHz晶振, 57600bps */
	TH2 = RCAP2H = 0xFF;
	T2CON = 0x34;  /* 定时器2用于串口的波特率发生器 */
	TI = 1;
}

int	main( void )  // USB host
{
	UINT8	i, s;
	UINT8 idata	buf[64];
//	P1&=0xF8; // 如果在U盘文件读写模块上试用本程序必须加上本行
	mDelaymS( 50 );  // 等待CH374复位完成
	CH374_PORT_INIT( );  /* CH374接口初始化 */

	mInitSTDIO( );  /* 为了让计算机通过串口监控演示过程 */
	printf( "Start CH374 Host\n" );

	Init374Host( );  // 初始化USB主机
	while ( 1 ) {
		HostSetBusFree( );  // 设定USB主机空闲
		printf( "Wait Device In\n" );
		while ( 1 ) {
			if ( Query374Interrupt( ) ) HostDetectInterrupt( );  // 如果有USB主机中断则处理
			if ( Query374DeviceIn( ) ) break;  // 有USB设备
		}
		mDelaymS( 250 );  // 由于USB设备刚插入尚未稳定，故等待USB设备数百毫秒，消除插拔抖动
		if ( Query374Interrupt( ) ) HostDetectInterrupt( );  // 如果有USB主机中断则处理

		printf( "Reset Device\n" );
		HostSetBusReset( );  // USB总线复位
		for ( i = 0; i < 100; i ++ ) {  // 等待USB设备复位后重新连接
			if ( Query374DeviceIn( ) ) break;  // 有USB设备
			mDelaymS( 1 );
		}
		if ( Query374Interrupt( ) ) HostDetectInterrupt( );  // 如果有USB主机中断则处理
		if ( Query374DeviceIn( ) ) {  // 有USB设备
			if ( Query374DevFullSpeed( ) ) {
				HostSetFullSpeed( );  // 检测到全速USB设备
				printf( "Start Full-Speed Device\n" );
			}
			else {  // 建议参考EMB_HUB中的低速例子处理
				HostSetLowSpeed( );  // 检测到低速USB设备,建议参考EMB_HUB中的低速例子处理
				printf( "Start Low-Speed Device\n" );
			}
		}
		else {
			printf( "Device gone !\n" );
			continue;  // 设备已经断开,继续等待
		}
		mDelaymS( 50 );

		printf( "GetDeviceDescr: " );
		s = GetDeviceDescr( buf );  // 获取设备描述符
		if ( s != USB_INT_SUCCESS ) {
			printf( "ERROR = %02X\n", (UINT16)s );
			goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
		}
		for ( i = 0; i < ( (PUSB_SETUP_REQ)SetupGetDevDescr ) -> wLengthL; i ++ ) printf( "%02X ", (UINT16)( buf[i] ) );
		printf( "\n" );

		printf( "SetUsbAddress: " );
		s = SetUsbAddress( 0x02 );  // 设置USB设备地址
		if ( s != USB_INT_SUCCESS ) {
			printf( "ERROR = %02X\n", (UINT16)s );
			goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
		}
		printf( "\n" );

		printf( "GetConfigDescr: " );
		s = GetConfigDescr( buf );  // 获取配置描述符
		if ( s != USB_INT_SUCCESS ) {
			printf( "ERROR = %02X\n", (UINT16)s );
			goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
		}
		for ( i = 0; i < ( (PUSB_CFG_DESCR)buf ) -> wTotalLengthL; i ++ ) printf( "%02X ", (UINT16)( buf[i] ) );
		printf( "\n" );
/* 分析配置描述符，获取端点数据/各端点地址/各端点大小等，更新变量endp_addr和endp_size等 */

		printf( "SetUsbConfig: " );
		s = SetUsbConfig( ( (PUSB_CFG_DESCR)buf ) -> bConfigurationValue );  // 设置USB设备配置
		if ( s != USB_INT_SUCCESS ) {
			printf( "ERROR = %02X\n", (UINT16)s );
			goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
		}
		printf( "\n" );

		printf( "USB device ready now\n" );
/* do something, read/write ...
		len = out_endp_size;
		Write374Block( RAM_HOST_TRAN, len, buf );
		Write374Byte( REG_USB_LENGTH, len );
		s = WaitHostTransact374( out_endp_addr, DEF_USB_PID_OUT, FALSE, 1000 );
		s = WaitHostTransact374( in_endp_addr, DEF_USB_PID_IN, FALSE, 1000 );
		len = Read374Byte( REG_USB_LENGTH );
		Read374Block( RAM_HOST_RECV, len, buf );
		len = out_endp_size;
		Write374Block( RAM_HOST_TRAN, len, buf );
		Write374Byte( REG_USB_LENGTH, len );
		s = WaitHostTransact374( out_endp_addr, DEF_USB_PID_OUT, TRUE, 1000 );
		s = WaitHostTransact374( in_endp_addr, DEF_USB_PID_IN, TRUE, 1000 );
		len = Read374Byte( REG_USB_LENGTH );
		Read374Block( RAM_HOST_RECV, len, buf );
*/
		printf( "do something, read / write ......\n" );

WaitDeviceOut:  // 等待USB设备拔出
		printf( "Wait Device Out\n" );
		while ( 1 ) {
			if ( Query374Interrupt( ) ) HostDetectInterrupt( );  // 如果有USB主机中断则处理
			if ( Query374DeviceIn( ) == FALSE ) break;  // 没有USB设备
		}
		mDelaymS( 100 );  // 等待设备完全断开，消除插拔抖动
		if ( Query374DeviceIn( ) ) goto WaitDeviceOut;  // 没有完全断开
//		HostSetBusFree( );  // 设定USB主机空闲，主要目的是关闭SOF
	}
}
