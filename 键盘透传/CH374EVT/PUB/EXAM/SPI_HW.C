/* CH374芯片 硬件标准SPI串行连接的硬件抽象层 V1.0 */
/* 提供I/O接口子程序,提供寄存器级读写子程序 */

#include	"HAL.H"

/* 本例中的硬件连接方式如下(实际应用电路可以参照修改下述定义及子程序) */
/* 单片机的引脚    CH374芯片的引脚
      P1.4                 SCS#
      P1.5                 SDI
      P1.6                 SDO
      P1.7                 SCK      */
sbit	P14					=	P1^4;
#define	CH374_SPI_SCS			P14		/* 假定CH374的SCS引脚 */

sfr		SPDR = 0x86;	/* SPI数据寄存器 */
sfr		SPSR = 0xAA;	/* SPI状态寄存器 */
sfr		SPCR = 0xD5;    /* SPI控制寄存器 */
#define	SPI_IF_TRANS	0x80	/* SPI字节传输完成标志,在SPSR的位7 */

void CH374_PORT_INIT( )  /* 由于使用通用I/O模拟并口读写时序,所以进行初始化 */
{
/* 如果是硬件SPI接口,那么可使用mode3(CPOL=1&CPHA=1)或mode0(CPOL=0&CPHA=0),CH374在时钟上升沿采样输入,下降沿输出,数据位是高位在前 */
	CH374_SPI_SCS = 1;  /* 禁止SPI片选 */
/* 对于双向I/O引脚模拟SPI接口,那么必须在此设置SPI_SCS,SPI_SCK,SPI_SDI为输出方向,SPI_SDO为输入方向 */
	SPCR = 0x5C;  /* 设置SPI模式3, DORD=0(MSB first), CPOL=1, CPHA=1, CH374也支持SPI模式0 */
}

UINT8	Spi374Exchange( UINT8 d )  /* 硬件SPI输出且输入8个位数据 */
{  /* 为了提高速度,可以将该子程序做成宏以减少子程序调用层次 */
	SPDR = d;  /* 先将数据写入SPI数据寄存器,然后查询SPI状态寄存器以等待SPI字节传输完成 */
	while ( ( SPSR & SPI_IF_TRANS ) == 0 );  /* 查询SPI状态寄存器以等待SPI字节传输完成 */
	SPSR &= ~ SPI_IF_TRANS;  /* 清除SPI字节传输完成标志,有的单片机会自动清除 */
	return( SPDR );  /* 先查询SPI状态寄存器以等待SPI字节传输完成,然后从SPI数据寄存器读出数据 */
}

#define	Spi374OutByte( d )		Spi374Exchange( d )		/* SPI输出8个位数据 */
#define	Spi374InByte( )			Spi374Exchange( 0xFF )	/* SPI输入8个位数据 */

void	Spi374Start( UINT8 addr, UINT8 cmd )  /* SPI开始 */
{
//	CH374_SPI_SCS = 1;
/* 对于双向I/O引脚模拟SPI接口,那么必须确保已经设置SPI_SCS,SPI_SCK,SPI_SDI为输出方向,SPI_SDO为输入方向 */
	CH374_SPI_SCS = 0;  /* SPI片选有效 */
	Spi374OutByte( addr );
	Spi374OutByte( cmd );
}

#define	Spi374Stop( )	{ CH374_SPI_SCS = 1; }  /* SPI结束 */
//void	Spi374Stop( void )  /* SPI结束 */
//{
//	CH374_SPI_SCS = 1;	/* SPI片选无效 */
//}

UINT8	Read374Byte( UINT8 mAddr )  /* 从指定寄存器读取数据 */
{
	UINT8	d;
	Spi374Start( mAddr, CMD_SPI_374READ );
	d = Spi374InByte( );
	Spi374Stop( );
	return( d );
}

void	Write374Byte( UINT8 mAddr, UINT8 mData )  /* 向指定寄存器写入数据 */
{
	Spi374Start( mAddr, CMD_SPI_374WRITE );
	Spi374OutByte( mData );
	Spi374Stop( );
}

void	Read374Block( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* 从指定起始地址读出数据块 */
{
	Spi374Start( mAddr, CMD_SPI_374READ );
	while ( mLen -- ) *mBuf++ = Spi374InByte( );
	Spi374Stop( );
}

void	Write374Block( UINT8 mAddr, UINT8 mLen, PUINT8 mBuf )  /* 向指定起始地址写入数据块 */
{
	Spi374Start( mAddr, CMD_SPI_374WRITE );
	while ( mLen -- ) Spi374OutByte( *mBuf++ );
	Spi374Stop( );
}
