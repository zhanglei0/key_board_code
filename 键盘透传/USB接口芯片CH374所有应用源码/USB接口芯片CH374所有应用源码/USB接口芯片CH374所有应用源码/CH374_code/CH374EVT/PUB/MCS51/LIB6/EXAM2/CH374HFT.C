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

/* MCS-51单片机C语言的U盘文件读写示例程序, 适用于ATMEL/PHILIPS/SST等具有双DPTR的单片机 */
/* 该程序将U盘中的/C51/CH374HFT.C文件中的前600个字符显示出来,
   如果找不到原文件CH374HFT.C, 那么该程序将显示C51子目录下所有以CH374开头的文件名,
   如果找不到C51子目录, 那么该程序将显示根目录下的所有文件名,
   最后将程序ROM中的一个字符串写入写入新建的文件"NEWFILE.TXT"中
   本例以字节模式读写文件不需要文件数据缓冲区FILE_DATA_BUF,总共只需要600字节的RAM
*/
/* CH374的INT#引脚采用中断方式处理, 数据复制方式为"双DPTR复制", 所以速度较快, 适用于ATMEL/PHILIPS/SST等具有双DPTR的单片机 */


/* C51   CH374HFT.C */
/* LX51  CH374HFT.OBJ , CH374HF6.LIB, C51DPTR2.LIB */
/* OHX51 CH374HFT */

#include <reg52.h>
#include <stdio.h>
#include <string.h>

/* 以下定义的详细说明请看CH374HF6.H文件 */
#define LIB_CFG_INT_EN			1		/* CH374的INT#引脚连接方式,0为"查询方式",1为"中断方式" */
#define NO_DEFAULT_CH374_INT	1		/* 禁止CH374HF6.H中的默认中断处理程序,使用下面自行编写的中断程序 */

#define CH374_IDX_PORT_ADDR		0xBDF1	/* CH374索引端口的I/O地址 */
#define CH374_DAT_PORT_ADDR		0xBCF0	/* CH374数据端口的I/O地址 */
/* 62256提供的32KB的RAM分为两部分: 0000H-01FFH为磁盘读写缓冲区, 0200H-7FFFH为文件数据缓冲区 */
#define	DISK_BASE_BUF_ADDR		0x0000	/* 外部RAM的磁盘数据缓冲区的起始地址,从该单元开始的缓冲区长度为SECTOR_SIZE */

#define CH374_INT_NO			0		/* CH374中断号, CH374的中断线INT#引脚连接单片机的INT0引脚 */
#define CH374_INT_FLAG			IE0		/* IE0,CH374中断标志 */
#define CH374_INT_EN			EX0		/* EX0,CH374中断允许 */

#define NO_DEFAULT_CH374_F_ENUM		1		/* 未调用CH374FileEnumer程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_F_QUERY	1		/* 未调用CH374FileQuery程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_RESET		1		/* 未调用CH374Reset程序故禁止以节约代码 */

#include "..\CH374HF6.H"

UINT8	process( void );

#ifdef NO_DEFAULT_CH374_INT			/* 在应用程序中定义NO_DEFAULT_CH374_INT可以禁止默认的中断处理程序,然后用自行编写的程序代替它 */
void xQueryInterrupt( void )			/* 查询中断状态,等待硬件中断 */
{
#ifdef CH374_INT_WIRE					/* 连接了CH374的中断引脚 */
	while ( CH374_INT_WIRE );  /* 如果CH374的中断引脚输出高电平则等待 */
#else									/* 未连接CH374的中断引脚 */
	while ( ( CH374_READ_REGISTER( REG_INTER_FLAG ) & ( BIT_IF_DEV_DETECT | BIT_IF_TRANSFER ) ) == 0 );  /* 查询中断标志寄存器 */
#endif
}
/* 由于进出中断等操作浪费了单片机时间,实际效果上中断方式比查询方式慢 */
/* 理论上讲可以在中断程序中处理所有U盘事务,例如检测到U盘插入中断就执行CH374DiskReady以及FileOpen等开始读写,但由于处理时间太长,对主程序影响较大而不宜在中断程序中读写U盘 */
void	CH374Interrupt( void ) interrupt CH374_INT_NO	/* CH374中断服务程序,使用寄存器组1,由CH374的INT#的低电平或者下降沿触发单片机中断 */
{
	UINT8	s;
	s = CH374_READ_REGISTER( REG_INTER_FLAG );  /* 中断标志寄存器 */
	if ( s & BIT_IF_DEV_DETECT ) {  /* USB设备插拔事件 */
		CH374DiskStatus = DISK_DISCONNECT;  /* 假定为USB设备断开事件 */
#ifdef EN_HUB_DISK
		CH374vHubPortCount = 0;  /* 默认为无HUB */
#endif
		CH374_WRITE_REGISTER( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_TRANSFER );  /* 清中断标志 */
		CH374_WRITE_REGISTER( REG_USB_SETUP, BIT_SETP_HOST_MODE );  /* USB主机模式,USB总线空闲 */
		if ( s & BIT_IF_DEV_ATTACH ) {  /* USB设备连接事件 */
			CH374DiskStatus = DISK_CONNECT;  /* USB设备已经连接或者断开后重新连接 */
			process( );  /* 向U盘写入文件 */
		}
	}
	else {  /* 其它中断,不应该发生的情况,除非人为修改CH374的中断使能寄存器 */
		CH374_WRITE_REGISTER( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  /* 清中断标志 */
	}
}
#endif

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
//	printf( "Error: %02X\n", (UINT16)iError );  /* 显示错误 */
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

UINT8	process( void ) {  /* 向U盘写入文件 */
	UINT8	i, c;
	UINT8	code *pCodeStr;
	LED_OUT_ACT( );  /* LED亮 */
	CH374DelaymS( 100 );  /* 延时,可选操作,有的USB存储器需要几十毫秒的延时 */
	for ( i = 0; i < 10; i ++ ) {  /* 最长等待时间,10*50mS */
		CH374DelaymS( 50 );
//		printf( "Ready ?\n" );
		if ( CH374DiskReady( ) == ERR_SUCCESS ) break;  /* 查询磁盘是否准备好 */
		if ( CH374DiskStatus < DISK_CONNECT ) {  /* 检测到断开,重新检测并计时 */
//			printf( "Device gone\n" );
			return( ERR_DISK_DISCON );  /* 重新等待 */
		}
		if ( CH374DiskStatus >= DISK_MOUNTED && i > 5 ) break;  /* 有的U盘总是返回未准备好,不过可以忽略,只要其建立连接MOUNTED且尝试5*50mS */
	}
	if ( CH374DiskStatus < DISK_MOUNTED ) {  /* 未知USB设备,例如USB键盘、打印机等 */
//		printf( "Unknown device\n" );
		return( USB_INT_DISK_ERR );
	}
/* 查询磁盘物理容量 */
/*	printf( "DiskSize\n" );
	i = CH374DiskSize( );  
	mStopIfError( i );
	printf( "TotalSize = %u MB \n", (unsigned int)( mCmdParam.DiskSize.mDiskSizeSec >> 11 ) );  显示为以MB为单位的容量
*/
	LED_RUN_ACT( );  /* 开始操作U盘 */
/* 产生新文件 */
	LED_WR_ACT( );  /* 写操作 */
//	printf( "Create\n" );
	strcpy( mCmdParam.Create.mPathName, "/NEWFILE.TXT" );  /* 新文件名,在根目录下,中文文件名 */
	i = CH374FileCreate( );  /* 新建文件并打开,如果文件已经存在则先删除后再新建 */
	mStopIfError( i );
//	printf( "Write\n" );
	pCodeStr = "Note: \xd\xa这个程序是以字节为单位进行U盘文件读写,单片机只需要有600字节的RAM,本程序工作于中断方式,当有U盘插入后就自动写入数据\xd\xa";
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
/*	printf( "Modify\n" );
	mCmdParam.Modify.mFileAttr = 0xff;   输入参数: 新的文件属性,为0FFH则不修改
	mCmdParam.Modify.mFileTime = 0xffff;   输入参数: 新的文件时间,为0FFFFH则不修改,使用新建文件产生的默认时间
	mCmdParam.Modify.mFileDate = MAKE_FILE_DATE( 2004, 5, 18 );  输入参数: 新的文件日期: 2004.05.18
	mCmdParam.Modify.mFileSize = 0xffffffff;   输入参数: 新的文件长度,以字节为单位写文件应该由程序库关闭文件时自动更新长度,所以此处不修改
	i = CH374FileModify( );   修改当前文件的信息,修改日期
	mStopIfError( i );
*/
//	printf( "Close\n" );
	mCmdParam.Close.mUpdateLen = 1;  /* 自动计算文件长度,以字节为单位写文件,建议让程序库关闭文件以便自动更新文件长度 */
	i = CH374FileClose( );
	mStopIfError( i );

	LED_WR_INACT( );

	LED_RUN_INACT( );
//	printf( "Take out\n" );
	return( ERR_SUCCESS );
}

main( ) {
	UINT8	i;
	LED_OUT_INIT( );
	LED_OUT_ACT( );  /* 开机后LED亮一下以示工作 */
	CH374DelaymS( 100 );  /* 延时100毫秒 */
	LED_OUT_INACT( );
	mInitSTDIO( );  /* 为了让计算机通过串口监控演示过程 */
	printf( "Start\n" );

	i = CH374LibInit( );  /* 初始化CH374程序库和CH374芯片,操作成功返回0 */
	mStopIfError( i );
	CH374_INT_EN = 1;  /* 允许CH374中断 */
/* 其它电路初始化 */
	EA = 1;  /* 初始化完成,开中断 */

	while ( 1 ) {  /* 纯中断方式主程序只需通过标志判断,无需任何查询 */
		printf( "Wait Udisk In\n" );
		while ( CH374DiskStatus < DISK_CONNECT );
		printf( "Wait Udisk Out\n" );
		while ( CH374DiskStatus >= DISK_CONNECT );
	}
}
