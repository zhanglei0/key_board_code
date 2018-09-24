/* 本例用I/O引脚模拟产生CH374的并口读写时序 */
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

static	void Write374Index( UINT8 mIndex )  /* 向CH374写索引地址 */
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

static	void Write374Data( UINT8 mData )  /* 向CH374写数据,索引地址自动加1 */
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

static	UINT8 Read374Data( void )  /* 从CH374读数据,索引地址自动加1 */
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
