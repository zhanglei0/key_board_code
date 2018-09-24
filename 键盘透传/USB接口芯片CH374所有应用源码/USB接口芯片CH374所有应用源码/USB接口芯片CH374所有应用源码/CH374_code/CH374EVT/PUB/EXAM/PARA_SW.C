/* CH374芯片 软件模拟8位并口连接的硬件抽象层 V1.0 */
/* 提供I/O接口子程序,提供寄存器级读写子程序 */

#include	"HAL.H"

/* 本例中的硬件连接方式如下(实际应用电路可以参照修改下述定义及子程序) */
/* 单片机的引脚    CH374芯片的引脚
      P2.0                 A0
      P2.6                 CS#    如果模拟出的并口上只有CH374,那么CS#可以直接接低电平,强制片选
      P3.6                 WR#
      P3.7                 RD#
      P0(8位端口)         D7-D0       */
sbit	CH374_A0	=	P2^0;
sbit	CH374_CS	=	P2^6;
sbit	CH374_WR	=	P3^6;
sbit	CH374_RD	=	P3^7;
#define	CH374_DATA_DAT_OUT( d )	{ P0 = d; }		/* 向并口输出数据 */
#define	CH374_DATA_DAT_IN( )	( P0 )			/* 从并口输入数据 */
#define	CH374_DATA_DIR_OUT( )					/* 设置并口方向为输出 */
#define	CH374_DATA_DIR_IN( )	{ P0 = 0xFF; }	/* 设置并口方向为输入 */

void CH374_PORT_INIT( )  /* 由于使用通用I/O模拟并口读写时序,所以进行初始化 */
{
	CH374_CS = 1;
	CH374_WR = 1;
	CH374_RD = 1;
	CH374_A0 = 0;
	CH374_DATA_DIR_IN( );  /* 设置并口输入 */
}

void Write374Index( UINT8 mIndex )  /* 向CH374写索引地址 */
{
	CH374_DATA_DAT_OUT( mIndex );  /* 向CH374的并口输出数据 */
	CH374_DATA_DIR_OUT( );  /* 设置并口方向为输出 */
	CH374_A0 = 1;
	CH374_CS = 0;
	CH374_WR = 0;  /* 输出有效写控制信号, 写CH374芯片的索引端口 */
//	CH374_CS = 0;  /* 该操作无意义,仅作延时,CH374要求读写脉冲宽度大于70nS */
	CH374_WR = 1;  /* 输出无效的控制信号, 完成操作CH374芯片 */
	CH374_CS = 1;
	CH374_A0 = 0;
	CH374_DATA_DIR_IN( );  /* 禁止数据输出 */
}

void Write374Data( UINT8 mData )  /* 向CH374写数据,索引地址自动加1 */
{
	CH374_DATA_DAT_OUT( mData );  /* 向CH374的并口输出数据 */
	CH374_DATA_DIR_OUT( );  /* 设置并口方向为输出 */
	CH374_A0 = 0;
	CH374_CS = 0;
	CH374_WR = 0;  /* 输出有效写控制信号, 写CH374芯片的数据端口 */
//	CH374_CS = 0;  /* 该操作无意义,仅作延时,CH374要求读写脉冲宽度大于70nS */
	CH374_WR = 1;  /* 输出无效的控制信号, 完成操作CH374芯片 */
	CH374_CS = 1;
	CH374_DATA_DIR_IN( );  /* 禁止数据输出 */
}

UINT8 Read374Data( void )  /* 从CH374读数据,索引地址自动加1 */
{
	UINT8	mData;
	CH374_DATA_DIR_IN( );  /* 设置并口方向为输入 */
	CH374_A0 = 0;
	CH374_CS = 0;
	CH374_RD = 0;  /* 输出有效写控制信号, 读CH374芯片的数据端口 */
	CH374_CS = 0;  /* 该操作无意义,仅作延时,CH374要求读写脉冲宽度大于70nS,强烈建议此处执行一条空指令延时以确保并口有足够时间输入数据 */
	mData = CH374_DATA_DAT_IN( );  /* 从CH374的并口输入数据 */
	CH374_RD = 1;  /* 输出无效的控制信号, 完成操作CH374芯片 */
	CH374_CS = 1;
	return( mData );
}

UINT8 Read374Data0( void )  /* 从索引端口读出数据，索引地址不变，适用于[读出->修改->写回]操作 */
{
	UINT8	mData;
	CH374_DATA_DIR_IN( );  /* 设置并口方向为输入 */
	CH374_A0 = 1;
	CH374_CS = 0;
	CH374_RD = 0;  /* 输出有效写控制信号, 读CH374芯片的索引端口 */
	CH374_CS = 0;  /* 该操作无意义,仅作延时,CH374要求读写脉冲宽度大于70nS,强烈建议此处执行一条空指令延时以确保并口有足够时间输入数据 */
	mData = CH374_DATA_DAT_IN( );  /* 从CH374的并口输入数据 */
	CH374_RD = 1;  /* 输出无效的控制信号, 完成操作CH374芯片 */
	CH374_CS = 1;
	CH374_A0 = 0;
	return( mData );
}

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

void	Modify374Byte( UINT8 mAddr, UINT8 mAndData, UINT8 mOrData )  /* 修改指定寄存器的数据,先与再或,比Write374Byte再Read374Byte效率高 */
{
	Write374Index( mAddr );
	Write374Data( Read374Data0( ) & mAndData | mOrData );
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
