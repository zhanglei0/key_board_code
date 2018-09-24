/* 2004.06.05
****************************************
**  Copyright  (C)  W.ch  1999-2004   **
**  Web:  http://www.winchiphead.com  **
****************************************
**  USB Host File Interface for CH374 **
**  TC2.0@PC, KC7.0@MCS51             **
****************************************
*/
/* CH374 主机文件系统接口 */
/* 支持: FAT12/FAT16/FAT32 */

/* MCS-51单片机C语言的U盘文件读写示例程序, 适用于89C52或者更大程序空间的单片机,也适用于ATMEL/PHILIPS/SST等具有1KB内部RAM的单片机 */
/* 本程序用于演示将ADC模数采集的数据保存到U盘文件MY_ADC.TXT中 */
/* CH374的INT#引脚采用查询方式处理, 数据复制方式为"单DPTR复制",兼容性最好但是速度最慢,
   本例以字节为单位读写U盘文件,读写速度较扇区模式慢,
   但是由于字节模式读写文件不需要文件数据缓冲区FILE_DATA_BUF,
   所以总共只需要600字节的RAM,适用于单片机硬件资源有限、数据量小并且读写速度要求不高的系统 */


/* C51   CH374HFT.C */
/* LX51  CH374HFT.OBJ , CH374HF6.LIB, C51DPTR1.LIB */
/* OHX51 CH374HFT */

#include <reg52.h>
#include <stdio.h>
#include <string.h>

#define	MAX_BYTE_IO				35		/* 以字节为单位单次读写文件时的最大长度,默认值是29,值大则占用内存多,值小则超过该长度必须分多次读写 */

/* 以下定义的详细说明请看CH374HF6.H文件 */
#define LIB_CFG_INT_EN			0		/* CH374的INT#引脚连接方式,0为"查询方式",1为"中断方式" */

#define CH374_IDX_PORT_ADDR		0xBDF1	/* CH374索引端口的I/O地址 */
#define CH374_DAT_PORT_ADDR		0xBCF0	/* CH374数据端口的I/O地址 */
/* 只使用单片机内置的1KB外部RAM: 0000H-01FFH 为磁盘读写缓冲区, 以字节为单位读写文件不需要文件数据读写缓冲区FILE_DATA_BUF */
#define	DISK_BASE_BUF_ADDR		0x0000	/* 外部RAM的磁盘数据缓冲区的起始地址,从该单元开始的缓冲区长度为SECTOR_SIZE */

#define CH374_INT_WIRE			INT0	/* P3.2, INT0, CH374的中断线INT#引脚,连接CH374的INT#引脚,用于查询中断状态 */
/* 如果未连接CH374的中断引脚,那么应该去掉上述定义,自动使用寄存器查询方式 */

//#define DISK_BASE_BUF_LEN		2048	/* 默认的磁盘数据缓冲区大小为512字节,建议选择为2048甚至4096以支持某些大扇区的U盘,为0则禁止在.H文件中定义缓冲区并由应用程序在pDISK_BASE_BUF中指定 */
/* 如果需要复用磁盘数据缓冲区以节约RAM,那么可将DISK_BASE_BUF_LEN定义为0以禁止在.H文件中定义缓冲区,而由应用程序在调用CH375Init之前将与其它程序合用的缓冲区起始地址置入pDISK_BASE_BUF变量 */

#define NO_DEFAULT_CH374_F_ENUM		1		/* 未调用CH374FileEnumer程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_F_QUERY	1		/* 未调用CH374FileQuery程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_RESET		1		/* 未调用CH374Reset程序故禁止以节约代码 */

#include "..\CH374HF6.H"				/* 如果不需要支持FAT32,那么请选用CH374HF6.H */

/* 在P1.4连接一个LED用于监控演示程序的进度,低电平LED亮,当U盘插入后亮 */
sbit P1_4  = P1^4;
#define LED_OUT_INIT( )		{ P1_4 = 1; }	/* P1.4 高电平 */
#define LED_OUT_ACT( )		{ P1_4 = 0; }	/* P1.4 低电平驱动LED显示 */
#define LED_OUT_INACT( )	{ P1_4 = 1; }	/* P1.4 低电平驱动LED显示 */
sbit P1_5  = P1^5;
/* 在P1.5连接一个LED用于监控演示程序的进度,低电平LED亮,当对U盘操作时亮 */
#define LED_RUN_ACT( )		{ P1_5 = 0; }	/* P1.5 低电平驱动LED显示 */
#define LED_RUN_INACT( )	{ P1_5 = 1; }	/* P1.5 低电平驱动LED显示 */
sbit P1_6  = P1^6;
/* 在P1.6连接一个LED用于监控演示程序的进度,低电平LED亮,当对U盘写操作时亮 */
#define LED_WR_ACT( )		{ P1_6 = 0; }	/* P1.6 低电平驱动LED显示 */
#define LED_WR_INACT( )		{ P1_6 = 1; }	/* P1.6 低电平驱动LED显示 */

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
	SCON = 0x50;
	PCON = 0x80;
	TMOD = 0x20;
	TH1 = 0xf3;  /* 24MHz晶振, 9600bps */
	TR1 = 1;
	TI = 1;
}

main( ) {
	UINT8	i, month, hour;
	UINT16	date, adc, len;
	LED_OUT_INIT( );
	LED_OUT_ACT( );  /* 开机后LED亮一下以示工作 */
	CH374DelaymS( 100 );  /* 延时100毫秒 */
	LED_OUT_INACT( );
	mInitSTDIO( );  /* 为了让计算机通过串口监控演示过程 */
	printf( "Start\n" );

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
/* 查询磁盘物理容量 */
/*		printf( "DiskSize\n" );
		i = CH374DiskSize( );  
		mStopIfError( i );
		printf( "TotalSize = %u MB \n", (unsigned int)( mCmdParam.DiskSize.mDiskSizeSec * ( CH374vSectorSize / 512 ) >> 11 ) );  显示为以MB为单位的容量
*/
		LED_RUN_ACT( );  /* 开始操作U盘 */

/* 如果MY_ADC.TXT文件已经存在则添加数据到尾部,如果不存在则新建文件 */
		printf( "Open\n" );
		strcpy( mCmdParam.Open.mPathName, "/MY_ADC.TXT" );  /* 文件名,该文件在根目录下 */
		i = CH374FileOpen( );  /* 打开文件 */
		if ( i == ERR_SUCCESS ) {  /* 文件存在并且已经被打开,移动文件指针到尾部以便添加数据 */
			printf( "File size = %ld\n", CH374vFileSize );  /* 在成功打开文件后,全局变量CH374vFileSize中是文件当前长度 */
			printf( "Locate tail\n" );
			mCmdParam.ByteLocate.mByteOffset = 0xffffffff;  /* 移到文件的尾部 */
			i = CH374ByteLocate( );
			mStopIfError( i );
		}
		else if ( i == ERR_MISS_FILE ) {  /* 没有找到文件,必须新建文件 */
			LED_WR_ACT( );  /* 写操作 */
			printf( "Create\n" );
/*			strcpy( mCmdParam.Create.mPathName, "/MY_ADC.TXT" );*/  /* 文件名,该文件在根目录下,刚才已经提供给CH374FileOpen */
			i = CH374FileCreate( );  /* 新建文件并打开,如果文件已经存在则先删除后再新建 */
			mStopIfError( i );
		}
		else mStopIfError( i );  /* 打开文件时出错 */
		LED_WR_ACT( );  /* 写操作 */
		printf( "Write begin\n" );
		i = sprintf( mCmdParam.ByteWrite.mByteBuffer, "此前文件长度= %ld 字节\xd\xa", CH374vFileSize );
		mCmdParam.ByteWrite.mByteCount = i;  /* 指定本次写入的字节数,单次读写的长度不能超过MAX_BYTE_IO */
		i = CH374ByteWrite( );  /* 以字节为单位向文件写入数据,单次读写的长度不能超过MAX_BYTE_IO */
		mStopIfError( i );
/* 默认情况下,如果字节数mCmdParam.ByteWrite.mByteCount不为0那么CH374ByteWrite只负责写入数据而不修改文件长度,
   如果长时间不写入数据则应该更新文件长度,防止突然断电后前面写入的数据与文件长度不相符,
   如果需要写完数据后立即修改/更新文件长度,那么可以置字节数mCmdParam.ByteWrite.mByteCount为0后调用CH374ByteWrite强行更新文件长度,
   如果确定不会突然断电或者后面很快有数据不断写入则不必更新文件长度,可以提高速度并减少U盘损耗(U盘内部的内存寿命有限,不宜频繁改写) */
		printf( "Write ADC data\n" );
		for ( hour = 8; hour != 20; hour ++  ) {  /* 用循环方式添加12行数据 */
			TR0=1;  /* 用定时器0的计数值代替ADC数据 */
			month = 5;  /* 假定是5月 */
			date = TL1 & 0x1F;  /* 因为测试板上没有实时时钟芯片,所以用定时器1的计数代替进行演示 */
/*			adc = get_adc_data( ); */
			adc = ( (UINT16)TH0 << 8 ) | TL0;  /* 因为测试板上没有ADC,所以用定时器0的计数代替ADC数据演示 */
			len = sprintf( mCmdParam.ByteWrite.mByteBuffer, "%02d.%02d.%02d ADC=%u\xd\xa", (UINT16)month, date, (UINT16)hour, adc );  /* 将二制制数据格式为一行字符串 */
			mCmdParam.ByteWrite.mByteCount = (unsigned char)len;  /* 指定本次写入的字节数,不能超过MAX_BYTE_IO,否则另用缓冲区分多次写入 */
			i = CH374ByteWrite( );  /* 以字节为单位向文件写入数据,单次读写的长度不能超过MAX_BYTE_IO */
			mStopIfError( i );
		}
/*		mCmdParam.ByteWrite.mByteCount = 0;  如果指定写入0字节,则用于刷新文件的长度
		CH374ByteWrite( );  以字节为单位向文件写入数据,因为是0字节写入,所以只用于更新文件的长度,当阶段性写入数据后,可以用这种办法更新文件长度 */
		printf( "Write end\n" );
		strcpy( mCmdParam.ByteWrite.mByteBuffer, "今天的ADC数据到此结束\xd\xa" );
		i = strlen( mCmdParam.ByteWrite.mByteBuffer );
		mCmdParam.ByteWrite.mByteCount = i;  /* 指定本次写入的字节数,单次读写的长度不能超过MAX_BYTE_IO */
		i = CH374ByteWrite( );  /* 以字节为单位向文件写入数据,单次读写的长度不能超过MAX_BYTE_IO */
		mStopIfError( i );
/*		printf( "Modify\n" );  如果实际产品中有实时时钟,可以根据需要将文件的日期和时间修改为实际值
		mCmdParam.Modify.mFileAttr = 0xff;   输入参数: 新的文件属性,为0FFH则不修改
		mCmdParam.Modify.mFileTime = MAKE_FILE_TIME( 16, 32, 09 );   输入参数: 新的文件时间: 16:32:09
		mCmdParam.Modify.mFileDate = MAKE_FILE_DATE( 2004, 5, 18 );  输入参数: 新的文件日期: 2004.05.18
		mCmdParam.Modify.mFileSize = 0xffffffff;   输入参数: 新的文件长度,以字节为单位写文件应该由程序库关闭文件时自动更新长度,所以此处不修改
		i = CH374FileModify( );   修改当前文件的信息,修改日期
		mStopIfError( i );
*/
		printf( "Close\n" );
		mCmdParam.Close.mUpdateLen = 1;  /* 自动计算文件长度,以字节为单位写文件,建议让程序库关闭文件以便自动更新文件长度 */
		i = CH374FileClose( );  /* 关闭文件 */
		mStopIfError( i );
		LED_WR_INACT( );
		LED_RUN_INACT( );
		printf( "Take out\n" );
		while ( 1 ) {  /* 支持USB-HUB */
			CH374DelaymS( 10 );  /* 没必要频繁查询 */
			if ( CH374DiskConnect( ) != ERR_SUCCESS ) break;  /* 查询方式: 检查磁盘是否连接并更新磁盘状态,返回成功说明连接 */
		}
		LED_OUT_INACT( );  /* LED灭 */
		CH374DelaymS( 200 );
	}
}
