/* CH374芯片 硬件标准8位并口连接的硬件抽象层 V1.0 */
/* 提供I/O接口子程序,提供寄存器级读写子程序 */

#include	"HAL.H"

/* 本例中的硬件连接方式如下(实际应用电路可以参照修改下述定义及子程序) */
/* 单片机的引脚    CH374芯片的引脚
      P2.0                 A0
      P2.6                 CS#    如果并口上只有CH374,那么CS#可以直接接低电平,强制片选 */

UINT8XV	CH374_IDX_PORT	_at_ 0xBDF1;	/* 假定CH374索引端口的I/O地址 */
UINT8XV	CH374_DAT_PORT	_at_ 0xBCF0;	/* 假定CH374数据端口的I/O地址 */

void CH374_PORT_INIT( )  /* 由于使用标准并口读写时序,所以无需初始化 */
{
}

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
