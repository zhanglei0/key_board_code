/* 2004.06.05
****************************************
**  Copyright  (C)  W.ch  1999-2004   **
**  Web:  http://www.winchiphead.com  **
****************************************
**  USB Host File Interface for CH374 **
**  TC2.0@PC, ADS_C_1.2@ARM           **
****************************************
*/
/* CH374 主机文件系统接口 */
/* 支持: FAT12/FAT16/FAT32 */

/* ARM单片机C语言的U盘文件读写示例程序 */
/* 该程序将U盘中的/C51/CH374HFT.C文件中的前600个字符显示出来,
   如果找不到原文件CH374HFT.C, 那么该程序将显示C51子目录下所有以CH374开头的文件名,
   如果找不到C51子目录, 那么该程序将显示根目录下的所有文件名,
   最后将程序ROM中的一个字符串写入写入新建的文件"NEWFILE.TXT"中
*/
/* CH374的INT#引脚采用查询方式处理, 数据复制方式为"内部复制", 本程序适用于Philips LPC2114单片机, 串口0输出监控信息,9600bps */
/* ENDIAN = "little" */

/* 本例以字节为单位读写U盘文件,读写速度较扇区模式慢,但是由于字节模式读写文件不需要文件数据缓冲区FILE_DATA_BUF,
   所以总共只需要600字节的RAM,适用于单片机硬件资源有限、数据量小并且读写速度要求不高的系统 */

/* ARMCC -c -asm -o CH374HF9.O CH374HF9.C -ISRC\ */
/* ARMLINK -o CH374HFT.AXF -scatter SRC\LPC2114.SCF -entry 0 CH374HFT.O SRC\STARTUP.O SRC\HEAP.O SRC\STACK.O CH374HF9.LIB */
/* FROMELF -bin -o CH374HFT.BIN CH374HFT.AXF */

#include "TARGET.C"						/* 目标板的专用初始化代码 */

#include <stdio.h>
#include <string.h>

/* 以下定义的详细说明请看CH374HF9.H文件 */
#define LIB_CFG_INT_EN			0		/* CH374的INT#引脚连接方式,0为"查询方式",1为"中断方式" */

//#define CH374_INT_WIRE			( IO0PIN & 0x08 )	/* P0.3, CH374的中断线INT#引脚,连接CH374的INT#引脚,用于查询中断状态 */
/* 如果未连接CH374的中断引脚,那么应该去掉上述定义,自动使用寄存器查询方式 */

//#define DISK_BASE_BUF_LEN		2048	/* 默认的磁盘数据缓冲区大小为512字节,建议选择为2048甚至4096以支持某些大扇区的U盘,为0则禁止在.H文件中定义缓冲区并由应用程序在pDISK_BASE_BUF中指定 */
/* 如果需要复用磁盘数据缓冲区以节约RAM,那么可将DISK_BASE_BUF_LEN定义为0以禁止在.H文件中定义缓冲区,而由应用程序在调用CH375Init之前将与其它程序合用的缓冲区起始地址置入pDISK_BASE_BUF变量 */

#define NO_DEFAULT_CH374_F_ENUM		1		/* 未调用CH374FileEnumer程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_F_QUERY	1		/* 未调用CH374FileQuery程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_RESET		1		/* 未调用CH374Reset程序故禁止以节约代码 */

#include "..\CH374HF9.H"

/* 本例用硬件标准SPI接口0产生CH374的4线SPI串口读写时序 */
/* 本例中的硬件连接方式如下(实际应用电路可以参照修改下述定义及子程序) */
/* LPC2114单片机的引脚    CH374芯片的引脚
         P0.3                 SCS#
         P0.4                 SCK
         P0.5                 SDO
         P0.6                 SDI     */

void CH374_PORT_INIT( )  /* 进行初始化 */
{
	IO0SET |= 0x00000008;  /* 禁止SPI片选,设置SCS#默认为高电平 */
	IO0DIR |= 0x00000008;  /* 设置SCS#为输出 */
	PINSEL0 = PINSEL0 & 0xFFFF00FF | 0x00005500;  // 设置I/O连接到SPI0
	S0PCCR = 0x08;
/* 如果是硬件SPI接口,那么可使用mode3(CPOL=1&CPHA=1)或mode0(CPOL=0&CPHA=0),CH374在时钟上升沿采样输入,下降沿输出,数据位是高位在前 */
	S0PCR = 0x38;  /* 设置SPI模式3, LSBF=0(MSB first), CPOL=1, CPHA=1, CH374也支持SPI模式0 */
}

static	UINT8	Spi374Exchange( UINT8 d )  /* 硬件SPI输出且输入8个位数据 */
{  /* 为了提高速度,可以将该子程序做成宏以减少子程序调用层次 */
	S0PDR = d;  /* 先将数据写入SPI数据寄存器,然后查询SPI状态寄存器以等待SPI字节传输完成 */
	while ( ( S0PSR & 0x80 ) == 0 );  /* 查询SPI状态寄存器以等待SPI字节传输完成 */
	return( S0PDR );  /* 先查询SPI状态寄存器以等待SPI字节传输完成,然后从SPI数据寄存器读出数据 */
}

#define	Spi374OutByte( d )		Spi374Exchange( d )		/* SPI输出8个位数据 */
#define	Spi374InByte( )			Spi374Exchange( 0xFF )	/* SPI输入8个位数据 */

static	void	Spi374Start( UINT8 addr, UINT8 cmd )  /* SPI开始 */
{
	IO0CLR = 0x00000008;  /* SPI片选有效 */
	Spi374OutByte( addr );
	Spi374OutByte( cmd );
}

#define	Spi374Stop( )	{ IO0SET |= 0x00000008; }  /* SPI结束 */
//static	void	Spi374Stop( void )  /* SPI结束 */
//{
//	IO0SET |= 0x00000008;	/* SPI片选无效 */
//}

/*#include "PARA.C"*/	/* 并口时序 */
#include "SPI.C"    /* SPI时序 */

/* 在P0.2连接一个LED用于监控演示程序的进度,低电平LED亮 */
#define LED_OUT_INIT( )		{ IO0DIR |= 0x04; }	/* P0.2 高电平为输出方向 */
#define LED_OUT_ACT( )		{ IO0CLR |= 0x04; }	/* P0.2 低电平驱动LED显示 */
#define LED_OUT_INACT( )	{ IO0SET |= 0x04; }	/* P0.2 低电平驱动LED显示 */

/* 检查操作状态,如果错误则显示错误代码并停机 */
void	mStopIfError( UINT8 iError )
{
	if ( iError == ERR_SUCCESS ) return;  /* 操作成功 */
	printf( "Error: %02X\n", (UINT16)iError );  /* 显示错误 */
/* 遇到错误后,应该分析错误码以及CH374DiskStatus状态,例如调用CH374DiskConnect查询当前U盘是否连接,如果U盘已断开那么就重新等待U盘插上再操作,
   建议出错后的处理步骤:
   1、调用一次CH374DiskReady,成功则继续操作,例如Open,Read/Write等,在CH374DiskReady中会自动调用CH374DiskConnect，不必另外调用
   2、如果CH374DiskReady不成功,那么强行将CH374DiskStatus置为DISK_CONNECT状态,然后从头开始操作(等待U盘连接CH374DiskConnect，CH374DiskReady等) */
	while ( 1 ) {
		LED_OUT_ACT( );  /* LED闪烁 */
		CH374DelaymS( 100 );
		LED_OUT_INACT( );
		CH374DelaymS( 100 );
	}
}

/* 为printf和getkey输入输出初始化串口 */
void	mInitSTDIO( )
{
	UINT32	x;
	PINSEL0 = PINSEL0 & 0xFFFFFFF0 | 0x00000005;  // 设置I/O连接到UART0
	U0LCR = 0x80;                    // DLAB位置1
	x = (Fpclk>>4)/9600;             // 9600bps
	U0DLM = x>>8;
	U0DLL = x&0xff;
	U0LCR = 0x03;                    // 8位数据位,1位停止位,无奇偶校验
	U0FCR = 0x01;
}

/* 通过串口输出监控信息 */
int		fputc( int c, FILE *f )
{
	U0THR = c;                            // 发送数据
	while( ( U0LSR & 0x20 ) == 0 );       // 等待数据发送
	return( c );
}

int	main( ) {
	UINT8	i, c;
	UINT16	TotalCount;
	UINT8	*pCodeStr;
	CH374_PORT_INIT( );
	LED_OUT_INIT( );
	LED_OUT_ACT( );  /* 开机后LED亮一下以示工作 */
	CH374DelaymS( 100 );  /* 延时100毫秒 */
	LED_OUT_INACT( );
	mInitSTDIO( );  /* 为了让计算机通过串口监控演示过程 */
	printf( "Start\n" );

#if DISK_BASE_BUF_LEN == 0
	pDISK_BASE_BUF = &my_buffer[0];  /* 不在.H文件中定义CH374的专用缓冲区,而是用缓冲区指针指向其它应用程序的缓冲区便于合用以节约RAM */
#endif

	i = CH374LibInit( );  /* 初始化CH374程序库和CH374芯片,操作成功返回0 */
	mStopIfError( i );
/* 其它电路初始化 */

	while ( 1 ) {
		printf( "Wait Udisk\n" );
		while ( 1 ) {  /* 支持USB-HUB,更复杂的处理参考EXAM1和EXAM13 */
			CH374DelaymS( 10 );  /* 没必要频繁查询 */
			if ( CH374DiskConnect( ) == ERR_SUCCESS ) break;  /* 查询方式: 检查磁盘是否连接并更新磁盘状态,返回成功说明连接 */
		}
		LED_OUT_ACT( );  /* LED亮 */
		CH374DelaymS( 200 );  /* 延时,可选操作,有的USB存储器需要几十毫秒的延时 */

/* 检查U盘是否准备好,有些U盘不需要这一步,但是某些U盘必须要执行这一步才能工作 */
		for ( i = 0; i < 5; i ++ ) {  /* 有的U盘总是返回未准备好,不过可以被忽略 */
			CH374DelaymS( 100 );
			printf( "Ready ?\n" );
			if ( CH374DiskReady( ) == ERR_SUCCESS ) break;  /* 查询磁盘是否准备好 */
		}
		if ( CH374DiskStatus <= DISK_CONNECT ) continue;  /* 支持USB-HUB */

#if DISK_BASE_BUF_LEN
		if ( DISK_BASE_BUF_LEN < CH374vSectorSize ) {  /* 检查磁盘数据缓冲区是否足够大,CH374vSectorSize是U盘的实际扇区大小 */
			printf( "Too large sector size\n" );
			continue;
		}
#endif

/* 查询磁盘物理容量 */
/*		printf( "DiskSize\n" );
		i = CH374DiskSize( );  
		mStopIfError( i );
		printf( "TotalSize = %u MB \n", (unsigned int)( mCmdParam.DiskSize.mDiskSizeSec * ( CH374vSectorSize / 512 ) >> 11 ) );  显示为以MB为单位的容量
*/

/* 读取原文件 */
		printf( "Open\n" );
		strcpy( (char *)mCmdParam.Open.mPathName, "/C51/CH374HFT.C" );  /* 文件名,该文件在C51子目录下 */
		i = CH374FileOpen( );  /* 打开文件 */
		if ( i == ERR_MISS_DIR || i == ERR_MISS_FILE ) {  /* 没有找到文件 */
/* 列出文件 */
			if ( i == ERR_MISS_DIR ) pCodeStr = (PUINT8)"/*";  /* C51子目录不存在则列出根目录下的文件 */
			else pCodeStr = (PUINT8)"/C51/CH374*";  /* CH374HFT.C文件不存在则列出\C51子目录下的以CH374开头的文件 */
			printf( "List file %s\n", pCodeStr );
			for ( c = 0; c < 254; c ++ ) {  /* 最多搜索前254个文件,如果要搜索大255个文件请参考EXAM1和EXAM0 */
				strcpy( (char *)mCmdParam.Open.mPathName, (char *)pCodeStr );  /* 搜索文件名,*为通配符,适用于所有文件或者子目录 */
/* 另外一种枚举方法是，将此处的c置为0xFF，然后将真正的枚举序号存放于CH374vFileSize中，从而可以搜索大于254个文件，从0到2147483647个 */
				i = strlen( (char *)mCmdParam.Open.mPathName );
				mCmdParam.Open.mPathName[ i ] = c;  /* 根据字符串长度将结束符替换为搜索的序号,从0到254 */
				i = CH374FileOpen( );  /* 打开文件,如果文件名中含有通配符*,则为搜索文件而不打开 */
				if ( i == ERR_MISS_FILE ) break;  /* 再也搜索不到匹配的文件,已经没有匹配的文件名 */
				if ( i == ERR_FOUND_NAME ) {  /* 搜索到与通配符相匹配的文件名,文件名及其完整路径在命令缓冲区中 */
					printf( "  match file %03d#: %s\n", (unsigned int)c, mCmdParam.Open.mPathName );  /* 显示序号和搜索到的匹配文件名或者子目录名 */
					continue;  /* 继续搜索下一个匹配的文件名,下次搜索时序号会加1 */
				}
				else {  /* 出错 */
					mStopIfError( i );
					break;
				}
			}
		}
		else {  /* 找到文件或者出错 */
			mStopIfError( i );
			TotalCount = 600;  /* 准备读取总长度 */
			printf( "从文件中读出的前%d个字符是:\n",TotalCount );
			while ( TotalCount ) {  /* 如果文件比较大,一次读不完,可以再调用CH374ByteRead继续读取,文件指针自动向后移动 */
				if ( TotalCount > MAX_BYTE_IO ) c = MAX_BYTE_IO;  /* 剩余数据较多,限制单次读写的长度不能超过 sizeof( mCmdParam.ByteRead.mByteBuffer ) */
				else c = TotalCount;  /* 最后剩余的字节数 */
				mCmdParam.ByteRead.mByteCount = c;  /* 请求读出几十字节数据 */
				i = CH374ByteRead( );  /* 以字节为单位读取数据块,单次读写的长度不能超过MAX_BYTE_IO,第二次调用时接着刚才的向后读 */
				mStopIfError( i );
				TotalCount -= mCmdParam.ByteRead.mByteCount;  /* 计数,减去当前实际已经读出的字符数 */
				for ( i=0; i!=mCmdParam.ByteRead.mByteCount; i++ ) printf( "%c", mCmdParam.ByteRead.mByteBuffer[i] );  /* 显示读出的字符 */
				if ( mCmdParam.ByteRead.mByteCount < c ) {  /* 实际读出的字符数少于要求读出的字符数,说明已经到文件的结尾 */
					printf( "\n" );
					printf( "文件已经结束\n" );
					break;
				}
			}
/*	    如果希望从指定位置开始读写,可以移动文件指针
		mCmdParam.ByteLocate.mByteOffset = 608;  跳过文件的前608个字节开始读写
		CH374ByteLocate( );
		mCmdParam.ByteRead.mByteCount = 5;  读取5个字节
		CH374ByteRead( );   直接读取文件的第608个字节到612个字节数据,前608个字节被跳过

	    如果希望将新数据添加到原文件的尾部,可以移动文件指针
		CH374FileOpen( );
		mCmdParam.ByteLocate.mByteOffset = 0xffffffff;  移到文件的尾部
		CH374ByteLocate( );
		mCmdParam.ByteWrite.mByteCount = 13;  写入13个字节的数据
		CH374ByteWrite( );   在原文件的后面添加数据,新加的13个字节接着原文件的尾部放置
		mCmdParam.ByteWrite.mByteCount = 2;  写入2个字节的数据
		CH374ByteWrite( );   继续在原文件的后面添加数据
		mCmdParam.ByteWrite.mByteCount = 0;  写入0个字节的数据,实际上该操作用于通知程序库更新文件长度
		CH374ByteWrite( );   写入0字节的数据,用于自动更新文件的长度,所以文件长度增加15,如果不这样做,那么执行CH374FileClose时也会自动更新文件长度
*/
			printf( "Close\n" );
			i = CH374FileClose( );  /* 关闭文件 */
			mStopIfError( i );
		}

#ifdef EN_DISK_WRITE  /* 子程序库支持写操作 */
/* 产生新文件 */
		printf( "Create\n" );
		strcpy( (char *)mCmdParam.Create.mPathName, "/NEWFILE.TXT" );  /* 新文件名,在根目录下,中文文件名 */
		i = CH374FileCreate( );  /* 新建文件并打开,如果文件已经存在则先删除后再新建 */
		mStopIfError( i );
		printf( "Write\n" );
		pCodeStr = (PUINT8)"Note: \xd\xa这个程序是以字节为单位进行U盘文件读写,单片机只需要有600字节的RAM\xd\xa";
		while( 1 ) {  /* 分多次写入文件数据 */
			for ( i=0; i<MAX_BYTE_IO; i++ ) {
				c = *pCodeStr;
				mCmdParam.ByteWrite.mByteBuffer[i] = c;
				if ( c == 0 ) break;  /* 源字符串结束 */
				pCodeStr++;
			}
			if ( i == 0 ) break;  /* 源字符串结束,完成写文件 */
			mCmdParam.ByteWrite.mByteCount = i;  /* 写入数据的字符数,单次读写的长度不能超过MAX_BYTE_IO,第二次调用时接着刚才的向后写 */
			i = CH374ByteWrite( );  /* 向文件写入数据 */
			mStopIfError( i );
		}
/*		printf( "Modify\n" );
		mCmdParam.Modify.mFileAttr = 0xff;   输入参数: 新的文件属性,为0FFH则不修改
		mCmdParam.Modify.mFileTime = 0xffff;   输入参数: 新的文件时间,为0FFFFH则不修改,使用新建文件产生的默认时间
		mCmdParam.Modify.mFileDate = MAKE_FILE_DATE( 2004, 5, 18 );  输入参数: 新的文件日期: 2004.05.18
		mCmdParam.Modify.mFileSize = 0xffffffff;   输入参数: 新的文件长度,以字节为单位写文件应该由程序库关闭文件时自动更新长度,所以此处不修改
		i = CH374FileModify( );   修改当前文件的信息,修改日期
		mStopIfError( i );
*/
		printf( "Close\n" );
		mCmdParam.Close.mUpdateLen = 1;  /* 自动计算文件长度,以字节为单位写文件,建议让程序库关闭文件以便自动更新文件长度 */
		i = CH374FileClose( );
		mStopIfError( i );

/* 删除某文件 */
/*		printf( "Erase\n" );
		strcpy( (char *)mCmdParam.Create.mPathName, "/OLD" );  将被删除的文件名,在根目录下
		i = CH374FileErase( );  删除文件并关闭
		if ( i != ERR_SUCCESS ) printf( "Error: %02X\n", (UINT16)i );  显示错误
*/

/* 查询磁盘信息 */
/*		printf( "Disk\n" );
		i = CH374DiskQuery( );
		mStopIfError( i );
		printf( "Fat=%d, Total=%ld, Free=%ld\n", (UINT16)mCmdParam.Query.mDiskFat, mCmdParam.Query.mTotalSector, mCmdParam.Query.mFreeSector );
*/
#endif
		printf( "Take out\n" );
		while ( 1 ) {  /* 支持USB-HUB */
			CH374DelaymS( 10 );  /* 没必要频繁查询 */
			if ( CH374DiskConnect( ) != ERR_SUCCESS ) break;  /* 查询方式: 检查磁盘是否连接并更新磁盘状态,返回成功说明连接 */
		}
		LED_OUT_INACT( );  /* LED灭 */
		CH374DelaymS( 200 );
	}
}
