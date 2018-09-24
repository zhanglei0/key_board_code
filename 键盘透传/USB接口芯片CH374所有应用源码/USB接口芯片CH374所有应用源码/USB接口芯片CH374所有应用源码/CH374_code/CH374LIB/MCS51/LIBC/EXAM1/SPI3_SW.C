/* 本例用I/O引脚模拟产生CH374的3线SPI串口读写时序 */
/* 本例中的硬件连接方式如下(实际应用电路可以参照修改下述定义及子程序) */
/* 单片机的引脚    CH374芯片的引脚
      P1.4                 SCS#
      P1.5                 SDI , SDO      建议在调试期间将SDO串几百欧电阻后再与SDI并联,正式产品可以无需串电阻
      P1.7                 SCK      */
sbit	P14					=	P1^4;
sbit	P15					=	P1^5;
sbit	P17					=	P1^7;
#define	CH374_SPI_SCS			P14		/* 假定CH374的SCS引脚 */
#define	CH374_SPI_SDI			P15		/* 假定CH374的SDI引脚 */
#define	CH374_SPI_SDO			P15		/* 假定CH374的SDO引脚 */
#define	CH374_SPI_SCK			P17		/* 假定CH374的SCK引脚 */
#define	CH374_SPI_SDI_DIR_OUT( )		/* 设置CH374的SDI引脚方向为输出 */
#define	CH374_SPI_SDO_DIR_IN( )		{ CH374_SPI_SDO = 1; }	/* 设置CH374的SDO引脚方向为输入 */

void CH374_PORT_INIT( )  /* 由于使用通用I/O模拟并口读写时序,所以进行初始化 */
{
	CH374_SPI_SCS = 1;  /* 禁止SPI片选 */
	CH374_SPI_SCK = 1;  /* 默认为高电平,SPI模式3,也可以用SPI模式0,但模拟程序可能需稍做修改 */
/* 对于双向I/O引脚模拟SPI接口,那么必须在此设置SPI_SCS,SPI_SCK为输出方向 */
}

static	void	Spi374OutByte( UINT8 d )  /* SPI输出8个位数据 */
{
	UINT8	i;
	for ( i = 0; i < 8; i ++ ) {
		CH374_SPI_SCK = 0;
		if ( d & 0x80 ) CH374_SPI_SDI = 1;
		else CH374_SPI_SDI = 0;
		d <<= 1;  /* 数据位是高位在前 */
		CH374_SPI_SCK = 1;  /* CH374在时钟上升沿采样输入 */
	}
}

static	UINT8	Spi374InByte( void )  /* SPI输入8个位数据 */
{
	UINT8	i, d;
	d = 0;
/* 如果SPI的SDO和SDI合用一个I/O引脚，那么必须在此设置该I/O方向为输入 */
	CH374_SPI_SDO_DIR_IN( );  /* 设置CH374的SDO引脚方向为输入 */
	for ( i = 0; i < 8; i ++ ) {
		CH374_SPI_SCK = 0;  /* CH374在时钟下降沿输出 */
		d <<= 1;  /* 数据位是高位在前 */
		if ( CH374_SPI_SDO ) d ++;
		CH374_SPI_SCK = 1;
	}
	return( d );
}

static	void	Spi374Start( UINT8 addr, UINT8 cmd )  /* SPI开始 */
{
//	CH374_SPI_SCS = 1;
/* 对于双向I/O引脚模拟SPI接口,那么必须确保已经设置SPI_SCS,SPI_SCK,SPI_SDI为输出方向,SPI_SDO为输入方向 */
	CH374_SPI_SDI_DIR_OUT( );  /* 设置CH374的SDI引脚方向为输出 */
	CH374_SPI_SCS = 0;  /* SPI片选有效 */
	Spi374OutByte( addr );
	Spi374OutByte( cmd );
}

#define	Spi374Stop( )	{ CH374_SPI_SCS = 1; }  /* SPI结束 */
//static	void	Spi374Stop( void )  /* SPI结束 */
//{
//	CH374_SPI_SCS = 1;	/* SPI片选无效 */
//}
