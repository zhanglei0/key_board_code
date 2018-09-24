/* CH374芯片 应用层 V1.0 */
/* USB主机,初始化和枚举当前连接的USB设备,采用双缓冲区进行BulkOnly协议的数据传输,特别针对大容量存储设备U盘等 */
/* 本程序只是简化示例程序,注重描述CH374的传输方式和便于理解,并未严格处理BulkOnly协议以及SCSI命令集,也未考虑U盘兼容性和容错性 */
//#define	DEBUG_NOW	1		// 从串口输出执行步骤

#include	<stdio.h>
#include	<string.h>

/* ********** 以下子程序请从CH374/EVT/EXAM目录复制同名文件 ********** */

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

void	HostSetLowSpeed( void );  // 设定低速USB设备运行环境

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
#ifdef DEBUG_NOW
				printf( "*In\n" );
#endif
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
			else {  // USB设备断开事件
#ifdef DEBUG_NOW
				printf( "*Out\n" );
#endif
				return( USB_INT_DISCONNECT );
			}
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
#ifdef DEBUG_NOW
			printf( "*?Int\n" );
#endif
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
#ifdef DEBUG_NOW
	printf( "*C:R\n" );
#endif
	s = WaitHostTransact374( 0, DEF_USB_PID_SETUP, FALSE, 200 );  // SETUP阶段，200mS超时
	if ( s == USB_INT_SUCCESS ) {  // SETUP成功
		tog = TRUE;  // 默认DATA1,默认无数据故状态阶段为IN
		total = *( ReqBuf + 6 );
		if ( total && DatBuf ) {  // 需要收发数据
			len = total;
			if ( *ReqBuf & 0x80 ) {  // 收
				while ( len ) {
					mDelayuS( 100 );
#ifdef DEBUG_NOW
					printf( "*C:I\n" );
#endif
					s = WaitHostTransact374( 0, DEF_USB_PID_IN, tog, 200 );  // IN数据
					if ( s != USB_INT_SUCCESS ) break;
					count = Read374Byte( REG_USB_LENGTH );
					Read374Block( RAM_HOST_RECV, count, DatBuf );
					DatBuf += count;
					if ( count <= len ) len -= count;
					else len = 0;
					if ( count & ( UsbDevEndpSize - 1 ) ) break;  // 短包
					tog = tog ? FALSE : TRUE;
				}
				tog = FALSE;  // 状态阶段为OUT
			}
			else {  // 发
				while ( len ) {
					mDelayuS( 100 );
#ifdef DEBUG_NOW
					printf( "*C:O\n" );
#endif
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
#ifdef DEBUG_NOW
			printf( "*C:S\n" );
#endif
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
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE );  // USB总线复位
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // USB总线复位,允许SOF
}

void	HostSetBusReset( void )  // USB总线复位
{
	UsbDevEndpSize = DEFAULT_ENDP0_SIZE;  /* USB设备的端点0的最大包尺寸 */
	SetHostUsbAddr( 0x00 );
	Write374Byte( REG_USB_H_CTRL, 0x00 );
//	Write374Byte( REG_USB_SETUP, M_SET_USB_BUS_RESET( Read374Byte( REG_USB_SETUP ) & ~ BIT_SETP_AUTO_SOF ) );  // USB总线复位
	Write374Byte( REG_USB_SETUP, M_SET_USB_BUS_RESET( BIT_SETP_HOST_MODE ) );  // USB总线复位
	mDelaymS( 20 );  // USB总线复位期间
//	Write374Byte( REG_USB_SETUP, M_SET_USB_BUS_FREE( Read374Byte( REG_USB_SETUP ) ) );  // USB总线空闲
	HostSetBusFree( );  // USB总线空闲
	mDelaymS( 1 );
	Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_USB_SUSPEND );  // 清中断标志
}

void	HostSetFullSpeed( void )  // 设定全速USB设备运行环境
{
	Write374Byte( REG_USB_SETUP, Read374Byte( REG_USB_SETUP ) & ~ BIT_SETP_USB_SPEED | BIT_SETP_AUTO_SOF );  // 全速且发SOF
	mDelaymS( 1 );
}

void	HostSetLowSpeed( void )  // 设定低速USB设备运行环境
{
	Write374Byte( REG_USB_SETUP, Read374Byte( REG_USB_SETUP ) | BIT_SETP_USB_SPEED | BIT_SETP_AUTO_SOF );  // 低速且发SOF
	mDelaymS( 1 );
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

/* ********** BulkOnly传输协议层 ********** */

#ifdef BIG_ENDIAN
#define USB_BO_CBW_SIG		0x55534243	/* 命令块CBW识别标志'USBC' */
#define USB_BO_CSW_SIG		0x55534253	/* 命令状态块CSW识别标志'USBS' */
#else
#define USB_BO_CBW_SIG		0x43425355	/* 命令块CBW识别标志'USBC' */
#define USB_BO_CSW_SIG		0x53425355	/* 命令状态块CSW识别标志'USBS' */
#endif

#define USB_BO_CBW_SIZE			0x1F	/* 命令块CBW的总长度 */
#define USB_BO_CSW_SIZE			0x0D	/* 命令状态块CSW的总长度 */

typedef union _BULK_ONLY_CMD {
	struct {
		UINT32	mCBW_Sig;
		UINT32	mCBW_Tag;
		UINT32	mCBW_DataLen;			/* 输入: 数据传输长度 */
		UINT8	mCBW_Flag;				/* 输入: 传输方向等标志 */
		UINT8	mCBW_LUN;
		UINT8	mCBW_CB_Len;			/* 输入: 命令块的长度,有效值是1到16 */
		UINT8	mCBW_CB_Buf[16];		/* 输入: 命令块,该缓冲区最多为16个字节 */
	} mCBW;								/* BulkOnly协议的命令块, 输入CBW结构 */
	struct {
		UINT32	mCSW_Sig;
		UINT32	mCSW_Tag;
		UINT32	mCSW_Residue;			/* 返回: 剩余数据长度 */
		UINT8	mCSW_Status;			/* 返回: 命令执行结果状态 */
	} mCSW;								/* BulkOnly协议的命令状态块, 输出CSW结构 */
} BULK_ONLY_CMD;

BULK_ONLY_CMD	idata	mBOC;			/* BulkOnly传输结构 */
UINT8			mSaveUsbPid;			/* 保存最近一次执行的事务PID */
UINT8			mSaveDevEndpTog;		/* 保存当前批量端点的同步标志 */
UINT8			mDiskBulkInEndp;		/* IN端点地址 */
UINT8			mDiskBulkOutEndp;		/* OUT端点地址 */

/* 传输事务,需预先输入mSaveUsbPid,PID令牌+目的端点地址,同步标志,返回同CH375,NAK重试,出错重试 */
UINT8	HostTransactInter( void )
{
	UINT8	s, r, retry;
	for ( retry = 3; retry != 0; retry -- ) {  /* 错误重试计数 */
		s = Wait374Interrupt( );
		if ( s == ERR_USB_UNKNOWN ) return( s );  // 中断超时,可能是硬件异常
		s = Read374Byte( REG_INTER_FLAG );  // 获取中断状态
		if ( s & BIT_IF_DEV_DETECT ) {  /* USB设备插拔事件 */
			mDelayuS( 200 );  // 等待传输完成
			Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_TRANSFER );  // 清中断标志
			if ( s & BIT_IF_DEV_ATTACH ) {  /* USB设备连接事件 */
				if ( s & BIT_IF_USB_DX_IN ) {  /* 速度匹配，不需要切换速度 */
					return( USB_INT_CONNECT );  /* 全速USB设备 */
				}
				else {  /* 速度失配，需要切换速度 */
					return( USB_INT_CONNECT_LS );  /* 低速USB设备 */
				}
			}
			else {  /* USB设备断开事件 */
				return( USB_INT_DISCONNECT );
			}
		}
		else if ( s & BIT_IF_TRANSFER ) {  /* 传输完成 */
			Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_TRANSFER );  /* 清中断标志 */
			s = Read374Byte( REG_USB_STATUS );  /* USB状态 */
			r = s & BIT_STAT_DEV_RESP;  /* USB设备应答状态 */
			if ( mSaveUsbPid == DEF_USB_PID_IN ) {  /* IN */
				if ( M_IS_HOST_IN_DATA( s ) ) {  /* DEF_USB_PID_DATA0 or DEF_USB_PID_DATA1 */
					if ( s & BIT_STAT_TOG_MATCH ) return( USB_INT_SUCCESS );  /* IN数据同步,操作成功 */
					goto mHostTransRetry;  /* 不同步则需丢弃后重试 */
				}
			}
			else {  /* OUT */
				if ( r == DEF_USB_PID_ACK ) return( USB_INT_SUCCESS );  /* OUT or SETUP,操作成功 */
			}
			if ( r == DEF_USB_PID_NAK ) {  /* NAK重试 */
				retry ++;
				goto mHostTransRetry;
			}
			else if ( ! M_IS_HOST_TIMEOUT( s ) ) return( r | 0x20 );  /* 错误或者无需重试或者意外应答,不是超时/出错 */
		}
		else {  /* 其它中断,不应该发生的情况 */
			mDelayuS( 200 );  // 等待传输完成
			Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  // 清中断标志
		}
mHostTransRetry:
		if ( retry > 1 ) {  /* 继续重试 */
			Write374Byte( REG_USB_H_CTRL, Read374Byte( REG_USB_H_CTRL ) | BIT_HOST_START );  /* 再次启动传输 */
		}
	}
	return( 0x20 );  /* 应答超时 */
}

/* 设置批量数据输入 */
void	mSetBulkIn( void )
{
	mSaveUsbPid = DEF_USB_PID_IN;  /* 输入 */
	Write374Byte( REG_USB_H_PID, M_MK_HOST_PID_ENDP( DEF_USB_PID_IN, mDiskBulkInEndp ) );  /* 指定令牌PID和目的端点号 */
	Write374Byte( REG_USB_H_CTRL, mSaveDevEndpTog | BIT_HOST_START );  /* 设置同步标志并启动传输 */
}

/* 设置批量数据输出 */
void	mSetBulkOut( void )
{
	mSaveUsbPid = DEF_USB_PID_OUT;  /* 输出 */
	Write374Byte( REG_USB_H_PID, M_MK_HOST_PID_ENDP( DEF_USB_PID_OUT, mDiskBulkOutEndp ) );  /* 指定令牌PID和目的端点号 */
	Write374Byte( REG_USB_H_CTRL, mSaveDevEndpTog | BIT_HOST_START );  /* 设置同步标志并启动传输 */
}

/* 大小端数据转换 */
UINT32	mSwapEndian( UINT32 dat )
{
	return( ( dat << 24 ) & 0xFF000000 | ( dat << 8 ) & 0x00FF0000 | ( dat >> 8 ) & 0x0000FF00 | ( dat >> 24 ) & 0x000000FF );
}

/* 执行基于BulkOnly协议的命令,该子程序比较简化,并未处理各种错误,实际应用必须处理 */
UINT8	mBulkOnlyCmd( PUINT8 DataBuf )
{
	UINT8	s, len;
	UINT32	TotalLen;
	mBOC.mCBW.mCBW_Sig = USB_BO_CBW_SIG;
	mBOC.mCBW.mCBW_Tag = 0x03740374;
	mBOC.mCBW.mCBW_LUN = 0;
	Write374Block( RAM_HOST_TRAN, USB_BO_CBW_SIZE, (PUINT8)( & mBOC.mCBW ) );  /* 向CH374主机端点的发送缓冲区写入CBW */
	Write374Byte( REG_USB_LENGTH, USB_BO_CBW_SIZE );
	mSetBulkOut( );  /* 输出 */
#ifdef DEBUG_NOW
	printf( "*B:C=%02XH\n", (UINT16)( mBOC.mCBW.mCBW_CB_Buf[0] ) );
#endif
	s = HostTransactInter( );  /* 向USB盘的OUT端点输出CBW */
	if ( s != USB_INT_SUCCESS ) return( s );  /* 发送CBW失败 */
	mSaveDevEndpTog ^= BIT_HOST_TRAN_TOG;  /* OUT端点的数据同步标志翻转 */
	if ( mBOC.mCBW.mCBW_DataLen ) {  /* 有数据需要传输,此处使用双缓冲区提高效率,但导致程序复杂 */
#ifdef BIG_ENDIAN
		TotalLen = mSwapEndian( mBOC.mCBW.mCBW_DataLen );
#else
		TotalLen = mBOC.mCBW.mCBW_DataLen;
#endif
		if ( mBOC.mCBW.mCBW_Flag & 0x80 ) {  /* 接收 */
			Write374Byte( REG_USB_SETUP, M_SET_RAM_MODE_2RX( BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF ) );  /* 全速USB主机模式,USB总线空闲,发SOF,双缓冲区 */
			mSetBulkIn( );  /* 输入 */
			while ( TotalLen ) {  /* 有数据需要传输 */
#ifdef DEBUG_NOW
				printf( "*B:I=%lX\n", TotalLen );
#endif
				s = HostTransactInter( );  /* 接收数据 */
				if ( s != USB_INT_SUCCESS ) break;  /* 操作失败 */
				mSaveDevEndpTog ^= BIT_HOST_RECV_TOG;  /* IN端点的数据同步标志翻转 */
				len = Read374Byte( REG_USB_LENGTH );
				if ( TotalLen > len ) TotalLen -= len;
				else TotalLen = 0;
				if ( len == 64 && TotalLen ) mSetBulkIn( );  /* 准备下一次输入 */
				Read374Block( mSaveDevEndpTog & BIT_HOST_RECV_TOG ? RAM_HOST_RECV : RAM_HOST_EXCH, len, DataBuf );
				DataBuf += len;
				if ( len < 64 ) break;
			}
		}
		else {  /* 发送 */
			Write374Byte( REG_USB_SETUP, M_SET_RAM_MODE_2TX( BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF ) );  /* 全速USB主机模式,USB总线空闲,发SOF,双缓冲区 */
			len = TotalLen >= 64 ? 64 : TotalLen;
			Write374Block( mSaveDevEndpTog & BIT_HOST_TRAN_TOG ? RAM_HOST_EXCH : RAM_HOST_TRAN, len, DataBuf );
			Write374Byte( REG_USB_LENGTH, len );
			mSetBulkOut( );  /* 输出 */
			while ( TotalLen ) {  /* 有数据需要传输 */
#ifdef DEBUG_NOW
				printf( "*B:O=%lX\n", TotalLen );
#endif
				TotalLen -= len;
				DataBuf += len;
				if ( len == 64 && TotalLen ) {  /* 准备下一次输出 */
					len = TotalLen >= 64 ? 64 : TotalLen;
					Write374Block( mSaveDevEndpTog & BIT_HOST_TRAN_TOG ? RAM_HOST_TRAN : RAM_HOST_EXCH, len, DataBuf );
				}
				else len = 0;
				s = HostTransactInter( );  /* 发送数据 */
				if ( s != USB_INT_SUCCESS ) break;  /* 操作失败 */
				mSaveDevEndpTog ^= BIT_HOST_TRAN_TOG;  /* OUT端点的数据同步标志翻转 */
				if ( len ) {
					Write374Byte( REG_USB_LENGTH, len );
					mSetBulkOut( );  /* 输出 */
				}
				else break;
			}
		}
		Write374Byte( REG_USB_SETUP, M_SET_RAM_MODE_OFF( BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF ) );  /* 全速USB主机模式,USB总线空闲,发SOF */
		if ( s != USB_INT_SUCCESS ) return( s );  /* 数据传输失败 */
	}
	mSetBulkIn( );  /* 输入 */
#ifdef DEBUG_NOW
	printf( "*B:S\n" );
#endif
	s = HostTransactInter( );  /* 从USB盘的IN端点输入CSW */
	if ( s != USB_INT_SUCCESS ) return( s );  /* 接收CSW失败 */
	mSaveDevEndpTog ^= BIT_HOST_RECV_TOG;  /* IN端点的数据同步标志翻转 */
	len = Read374Byte( REG_USB_LENGTH );
	Read374Block( RAM_HOST_RECV, len, (PUINT8)( & mBOC.mCSW ) );
	if ( len != USB_BO_CSW_SIZE || mBOC.mCSW.mCSW_Sig != USB_BO_CSW_SIG ) return( USB_INT_DISK_ERR );
	if ( mBOC.mCSW.mCSW_Status == 0 ) return( USB_INT_SUCCESS );
//	else if ( mBOC.mCSW.mCSW_Status >= 2 ) { RESET_RECOVERY, return ERROR }
	else return( USB_INT_DISK_ERR );  /* 磁盘操作错误 */
}

/* ********** SCSI/RBC/UFI命令层 ********** */

/* 获取磁盘特性 */
UINT8	mDiskInquiry( PUINT8 DataBuf )
{
#ifdef BIG_ENDIAN
	mBOC.mCBW.mCBW_DataLen = 0x24000000;
#else
	mBOC.mCBW.mCBW_DataLen = 0x00000024;
#endif
	mBOC.mCBW.mCBW_Flag = 0x80;
	mBOC.mCBW.mCBW_CB_Len = 6;
	mBOC.mCBW.mCBW_CB_Buf[0] = 0x12;  /* 命令码 */
	mBOC.mCBW.mCBW_CB_Buf[1] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[2] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[3] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[4] = 0x24;
	mBOC.mCBW.mCBW_CB_Buf[5] = 0x00;
	return( mBulkOnlyCmd( DataBuf ) );  /* 执行基于BulkOnly协议的命令 */
}

/* 获取磁盘容量 */
UINT8	mDiskCapacity( PUINT8 DataBuf )
{
#ifdef BIG_ENDIAN
	mBOC.mCBW.mCBW_DataLen = 0x08000000;
#else
	mBOC.mCBW.mCBW_DataLen = 0x00000008;
#endif
	mBOC.mCBW.mCBW_Flag = 0x80;
	mBOC.mCBW.mCBW_CB_Len = 10;
	mBOC.mCBW.mCBW_CB_Buf[0] = 0x25;  /* 命令码 */
	mBOC.mCBW.mCBW_CB_Buf[1] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[2] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[3] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[4] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[5] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[6] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[7] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[8] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[9] = 0x00;
	return( mBulkOnlyCmd( DataBuf ) );  /* 执行基于BulkOnly协议的命令 */
}

/* 测试磁盘是否就绪 */
UINT8	mDiskTestReady( void )
{
	mBOC.mCBW.mCBW_DataLen = 0;
	mBOC.mCBW.mCBW_Flag = 0x00;
	mBOC.mCBW.mCBW_CB_Len = 6;
	mBOC.mCBW.mCBW_CB_Buf[0] = 0x00;  /* 命令码 */
	mBOC.mCBW.mCBW_CB_Buf[1] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[2] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[3] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[4] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[5] = 0x00;
	return( mBulkOnlyCmd( NULL ) );  /* 执行基于BulkOnly协议的命令 */
}

/* 以扇区为单位从磁盘读取数据 */
UINT8	mReadSector( UINT32 StartLba, UINT8 SectCount, PUINT8 DataBuf )
{
	UINT32	len;
	len = (UINT32)SectCount << 9;
#ifdef BIG_ENDIAN
	mBOC.mCBW.mCBW_DataLen = mSwapEndian( len );
#else
	mBOC.mCBW.mCBW_DataLen = len;
#endif
	mBOC.mCBW.mCBW_Flag = 0x80;
	mBOC.mCBW.mCBW_CB_Len = 10;
	mBOC.mCBW.mCBW_CB_Buf[0] = 0x28;  /* 命令码 */
	mBOC.mCBW.mCBW_CB_Buf[1] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[2] = (UINT8)( StartLba >> 24 );
	mBOC.mCBW.mCBW_CB_Buf[3] = (UINT8)( StartLba >> 16 );
	mBOC.mCBW.mCBW_CB_Buf[4] = (UINT8)( StartLba >> 8 );
	mBOC.mCBW.mCBW_CB_Buf[5] = (UINT8)( StartLba );
	mBOC.mCBW.mCBW_CB_Buf[6] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[7] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[8] = SectCount;
	mBOC.mCBW.mCBW_CB_Buf[9] = 0x00;
	return( mBulkOnlyCmd( DataBuf ) );  /* 执行基于BulkOnly协议的命令 */
}

/* 以扇区为单位将数据写入磁盘 */
UINT8	mWriteSector( UINT32 StartLba, UINT8 SectCount, PUINT8 DataBuf )
{
	UINT32	len;
	len = (UINT32)SectCount << 9;
#ifdef BIG_ENDIAN
	mBOC.mCBW.mCBW_DataLen = mSwapEndian( len );
#else
	mBOC.mCBW.mCBW_DataLen = len;
#endif
	mBOC.mCBW.mCBW_Flag = 0x00;
	mBOC.mCBW.mCBW_CB_Len = 10;
	mBOC.mCBW.mCBW_CB_Buf[0] = 0x2A;  /* 命令码 */
	mBOC.mCBW.mCBW_CB_Buf[1] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[2] = (UINT8)( StartLba >> 24 );
	mBOC.mCBW.mCBW_CB_Buf[3] = (UINT8)( StartLba >> 16 );
	mBOC.mCBW.mCBW_CB_Buf[4] = (UINT8)( StartLba >> 8 );
	mBOC.mCBW.mCBW_CB_Buf[5] = (UINT8)( StartLba );
	mBOC.mCBW.mCBW_CB_Buf[6] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[7] = 0x00;
	mBOC.mCBW.mCBW_CB_Buf[8] = SectCount;
	mBOC.mCBW.mCBW_CB_Buf[9] = 0x00;
	return( mBulkOnlyCmd( DataBuf ) );  /* 执行基于BulkOnly协议的命令 */
}

/* ********** 主程序 ********** */

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
	UINT8 xdata	DISK_BUF[512];
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
		mDelaymS( 200 );  // 由于USB设备刚插入尚未稳定，故等待USB设备数百毫秒，消除插拔抖动
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
				printf( "Start Full-Speed Device\n" );
				HostSetFullSpeed( );  // 检测到全速USB设备
			}
			else {
				printf( "Start Low-Speed Device\n" );
				HostSetLowSpeed( );  // 检测到低速USB设备
			}
		}
		else {
			printf( "Device gone !\n" );
			continue;  // 设备已经断开,继续等待
		}
		mDelaymS( 20 );

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
		mDiskBulkInEndp = 0;
		mDiskBulkOutEndp = 0;
		for ( i = 0; i < 2; i ++ ) {  /* 分析前两个端点 */
			if ( ( (PUSB_CFG_DESCR_LONG)buf ) -> endp_descr[ i ].wMaxPacketSize == 0x40 && ( (PUSB_CFG_DESCR_LONG)buf ) -> endp_descr[ i ].bmAttributes == 2 ) {  /* 64字节长度的批量端点 */
				if ( ( (PUSB_CFG_DESCR_LONG)buf ) -> endp_descr[ i ].bEndpointAddress & 0x80 ) mDiskBulkInEndp = ( (PUSB_CFG_DESCR_LONG)buf ) -> endp_descr[ i ].bEndpointAddress & 0x0F;  /* IN端点 */
				else mDiskBulkOutEndp = ( (PUSB_CFG_DESCR_LONG)buf ) -> endp_descr[ i ].bEndpointAddress & 0x0F;  /* OUT端点 */
			}
		}
		if ( ( (PUSB_CFG_DESCR_LONG)buf ) -> itf_descr.bInterfaceClass != 0x08 || mDiskBulkInEndp == 0 || mDiskBulkOutEndp == 0 ) {  /* 不是USB存储类设备,不支持 */
			printf( "Not USB Mass Storage Device\n" );
			goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
		}

		printf( "SetUsbConfig: " );
		s = SetUsbConfig( ( (PUSB_CFG_DESCR)buf ) -> bConfigurationValue );  // 设置USB设备配置
		if ( s != USB_INT_SUCCESS ) {
			printf( "ERROR = %02X\n", (UINT16)s );
			goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
		}
		printf( "\n" );
		mSaveDevEndpTog = 0x00;  // 清同步标志

		printf( "Disk Inquiry: " );
		s = mDiskInquiry( buf );  /* 获取磁盘特性 */
		if ( s != USB_INT_SUCCESS ) {
			printf( "ERROR = %02X\n", (UINT16)s );
			goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
		}
		for ( i = 0; i < 8; i ++ ) printf( "%02X ", (UINT16)( buf[i] ) );
		printf( ", " );
		for ( i = 8; i < 36; i ++ ) printf( "%c", buf[i] );
		printf( "\n" );

		mDelaymS( 100 );
		printf( "Disk Capacity: " );
		s = mDiskCapacity( buf );  /* 获取磁盘容量 */
		if ( s != USB_INT_SUCCESS ) {
			printf( "ERROR = %02X\n", (UINT16)s );
		}
		else {
			for ( i = 0; i < 8; i ++ ) printf( "%02X ", (UINT16)( buf[i] ) );
			printf( ", %3d MB\n", (UINT16)( ( (UINT32)( buf[1] ) << 16 | (UINT16)( buf[2] ) << 8 | buf[3] ) >> 11 ) );
		}

		printf( "Disk Ready: " );
		s = mDiskTestReady( );  /* 测试磁盘是否就绪 */
		if ( s != USB_INT_SUCCESS ) {
			printf( "ERROR = %02X\n", (UINT16)s );
		}
		else printf( "\n" );

		printf( "Disk Read First Sector: " );
		s = mReadSector( 0x00000000, 1, DISK_BUF );  /* 以扇区为单位从磁盘读取数据 */
		if ( s != USB_INT_SUCCESS ) {
			printf( "ERROR = %02X\n", (UINT16)s );
			goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
		}
		for ( i = 0; i < 16; i ++ ) printf( "%02X ", (UINT16)( DISK_BUF[i] ) );
		printf( "\n" );

		DISK_BUF[0] ^= 0xFF;
		DISK_BUF[1] ^= 0xFF;
		DISK_BUF[510] ^= 0xFF;
		DISK_BUF[511] ^= 0xFF;
		printf( "Disk Write Second Sector: " );
		s = mWriteSector( 0x00000001, 1, DISK_BUF );  /* 以扇区为单位将数据写入磁盘 */
		if ( s != USB_INT_SUCCESS ) {
			printf( "ERROR = %02X\n", (UINT16)s );
			goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
		}
		for ( i = 0; i < 16; i ++ ) printf( "%02X ", (UINT16)( DISK_BUF[i] ) );
		printf( "\n" );

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
