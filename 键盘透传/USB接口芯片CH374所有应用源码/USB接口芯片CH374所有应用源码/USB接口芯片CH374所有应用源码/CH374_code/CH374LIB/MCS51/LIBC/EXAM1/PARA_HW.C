/* 本例用硬件标准并口产生CH374的并口读写时序 */
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
//static	void Write374Index( UINT8 mIndex )  /* 向CH374写索引地址 */
//{
//	CH374_IDX_PORT = mIndex;
//}

#define	Write374Data( d )	{ CH374_DAT_PORT = d; }	/* 向数据端口写入数据,索引地址自动加1 */
//static	void Write374Data( UINT8 mData )  /* 向CH374写数据 */
//{
//	CH374_DAT_PORT = mData;
//}

#define	Read374Data( )		( CH374_DAT_PORT )		/* 从数据端口读出数据,索引地址自动加1 */
//static	UINT8 Read374Data( void )  /* 从CH374读数据 */
//{
//	return( CH374_DAT_PORT );
//}
