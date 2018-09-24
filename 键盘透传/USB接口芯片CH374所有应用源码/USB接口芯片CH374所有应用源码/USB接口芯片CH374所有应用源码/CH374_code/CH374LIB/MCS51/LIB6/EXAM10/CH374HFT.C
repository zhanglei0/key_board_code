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

/* MCS-51单片机C语言的U盘文件读写示例程序, 适用于89C52或者更大程序空间的单片机 */
/* 本程序用于演示处理文件目录项,例如:修改文件名,设置文件的创建日期和时间等 */
/* CH374的INT#引脚采用查询方式处理, 数据复制方式为"单DPTR复制", 所以速度较慢, 适用于所有MCS51单片机 */


/* C51   CH374HFT.C */
/* LX51  CH374HFT.OBJ , CH374HF6.LIB, C51DPTR1.LIB */
/* OHX51 CH374HFT */

#include <reg52.h>
#include <stdio.h>
#include <string.h>

/* 以下定义的详细说明请看CH374HF6.H文件 */
#define LIB_CFG_INT_EN			0		/* CH374的INT#引脚连接方式,0为"查询方式",1为"中断方式" */

#define CH374_IDX_PORT_ADDR		0xBDF1	/* CH374索引端口的I/O地址 */
#define CH374_DAT_PORT_ADDR		0xBCF0	/* CH374数据端口的I/O地址 */
/* 62256提供的32KB的RAM分为两部分: 0000H-01FFH为磁盘读写缓冲区, 0200H-7FFFH为文件数据缓冲区 */
#define	DISK_BASE_BUF_ADDR		0x0000	/* 外部RAM的磁盘数据缓冲区的起始地址,从该单元开始的缓冲区长度为SECTOR_SIZE */

#define CH374_INT_WIRE			INT0	/* P3.2, INT0, CH374的中断线INT#引脚,连接CH374的INT#引脚,用于查询中断状态 */
/* 如果未连接CH374的中断引脚,那么应该去掉上述定义,自动使用寄存器查询方式 */

//#define DISK_BASE_BUF_LEN		2048	/* 默认的磁盘数据缓冲区大小为512字节,建议选择为2048甚至4096以支持某些大扇区的U盘,为0则禁止在.H文件中定义缓冲区并由应用程序在pDISK_BASE_BUF中指定 */
/* 如果需要复用磁盘数据缓冲区以节约RAM,那么可将DISK_BASE_BUF_LEN定义为0以禁止在.H文件中定义缓冲区,而由应用程序在调用CH375Init之前将与其它程序合用的缓冲区起始地址置入pDISK_BASE_BUF变量 */

#define NO_DEFAULT_CH374_F_ENUM		1		/* 未调用CH374FileEnumer程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_RESET		1		/* 未调用CH374Reset程序故禁止以节约代码 */

#include "..\CH374HF6.H"

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

/* 检查操作状态,如果错误则显示错误代码并停机,应该替换为实际的处理措施 */
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
		CH374DelaymS( 200 );
		LED_OUT_INACT( );
		CH374DelaymS( 200 );
	}
}

/* 为printf和getkey输入输出初始化串口 */
void	mInitSTDIO( )
{
	SCON = 0x50;
	PCON = 0x80;
	TMOD = 0x21;
	TH1 = 0xf3;  /* 24MHz晶振, 9600bps */
	TR1 = 1;
	TI = 1;
}

/* 修改指定文件的文件名,如果是C文件则修改为TXT文件 */
/* 输入参数:   原始文件名在mCmdParam.Open.mPathName中 */
/* 返回状态码: ERR_SUCCESS = 修改文件名成功,
               其它状态码参考CH374HF?.H */
UINT8	RenameFileName( void )
{
	UINT8			i;
	P_FAT_DIR_INFO	mFileDir;
	i = CH374FileOpen( );  /* 打开文件 */
	if ( i == ERR_SUCCESS ) {
		/* 文件读写操作等... */
		i = CH374FileQuery( );  /* 查询文件属性,以便将相关数据调到内存中再修改 */
		if ( i == ERR_SUCCESS ) {
			mFileDir = (P_FAT_DIR_INFO)( pDISK_BASE_BUF + CH374vFdtOffset );  /* 在内存中,当前FDT的起始地址 */
			if ( mFileDir -> DIR_Name[8] == 'C' && mFileDir -> DIR_Name[9] == ' ' ) {  /* 文件扩展名是C */
				mFileDir -> DIR_Name[8] = 'T';  /* 修改文件扩展名为TXT */
				mFileDir -> DIR_Name[9] = 'X';  /* 同样方法可以修改文件主名 */
				mFileDir -> DIR_Name[10] = 'T';
			}
/* 以下将修改过的内容从内存中真正刷新到U盘中 */
			mCmdParam.Modify.mFileAttr = mFileDir -> DIR_Attr;  /* 准备假修改文件属性,实际保持原值 */
			mCmdParam.Modify.mFileDate = mCmdParam.Modify.mFileTime = 0xFFFF;  /* 不修改文件日期和时间 */
			mCmdParam.Modify.mFileSize = 0xFFFFFFFF;  /* 不修改文件长度 */
			i = CH374FileModify( );  /* 实际是将内存中刚刚修改过的文件名保存 */
			if ( i == ERR_SUCCESS ) {
				/* 文件读写操作等... */
				mCmdParam.Close.mUpdateLen = 0;
				i = CH374FileClose( );  /* 关闭文件 */
			}
		}
	}
	return( i );
}

/* 大端与小端格式的数据处理,文件系统本身是小端,而MCS51程序是大端,所以才需要转换 */
UINT16	SwapUINT16( UINT16 d )
{
	return( ( d << 8 ) & 0xFF00 | ( d >> 8 ) & 0xFF );
}

/* 为指定文件设置创建日期和时间 */
/* 输入参数:   原始文件名在mCmdParam.Open.mPathName中, 新的创建日期和时间: iCreateDate, iCreateTime */
/* 返回状态码: ERR_SUCCESS = 设置成功,
               其它状态码参考CH374HF?.H */
UINT8	SetFileCreateTime( UINT16 iCreateDate, UINT16 iCreateTime )
{
	UINT8			i;
	P_FAT_DIR_INFO	mFileDir;
	i = CH374FileOpen( );  /* 打开文件 */
	if ( i == ERR_SUCCESS ) {
		/* 文件读写操作等... */
		i = CH374FileQuery( );  /* 查询文件属性,以便将相关数据调到内存中再修改 */
		if ( i == ERR_SUCCESS ) {
			mFileDir = (P_FAT_DIR_INFO)( pDISK_BASE_BUF + CH374vFdtOffset );  /* 在内存中,当前FDT的起始地址 */
//			mFileDir -> DIR_CrtTime = iCreateTime;  /* 文件创建的时间,适用于小端格式 */
			mFileDir -> DIR_CrtTime = SwapUINT16( iCreateTime );  /* MCS51单片机是大端格式 */
//			mFileDir -> DIR_CrtDate = iCreateDate;  /* 文件创建的日期,适用于小端格式 */
			mFileDir -> DIR_CrtDate = SwapUINT16( iCreateDate );  /* MCS51单片机是大端格式 */

//			mFileDir -> DIR_WrtTime = MAKE_FILE_TIME( 时, 分, 秒 );  /* 文件修改时间 */
//			mFileDir -> DIR_LstAccDate = MAKE_FILE_DATE( 年, 月, 日 );  /* 最近一次存取操作的日期 */

/* 以下将修改过的内容从内存中真正刷新到U盘中 */
			mCmdParam.Modify.mFileAttr = mFileDir -> DIR_Attr;  /* 准备假修改文件属性,实际保持原值 */
			mCmdParam.Modify.mFileDate = mCmdParam.Modify.mFileTime = 0xFFFF;  /* 不修改文件日期和时间 */
			mCmdParam.Modify.mFileSize = 0xFFFFFFFF;  /* 不修改文件长度 */
			i = CH374FileModify( );  /* 实际是将内存中刚刚修改过的文件名保存 */
			if ( i == ERR_SUCCESS ) {
				/* 文件读写操作等... */
				mCmdParam.Close.mUpdateLen = 0;
				i = CH374FileClose( );  /* 关闭文件 */
			}
		}
	}
	return( i );
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
/* 其它电路初始化 */

	while ( 1 ) {
		printf( "Wait Udisk\n" );
		while ( 1 ) {  /* 查询CH374中断并更新中断状态,等待U盘插入 */
			CH374DelaymS( 50 );  /* 没必要频繁查询 */
			if ( CH374DiskConnect( ) == ERR_SUCCESS ) break;  /* 有设备连接则返回成功,CH374DiskConnect同时会更新全局变量CH374DiskStatus */
		}
		LED_OUT_ACT( );  /* LED亮 */
		CH374DelaymS( 200 );  /* 延时,可选操作,有的USB存储器需要几十毫秒的延时 */

		for ( i = 0; i < 5; i ++ ) {  /* 最长等待时间,5*50mS */
			CH374DelaymS( 50 );
			printf( "Ready ?\n" );
			if ( CH374DiskReady( ) == ERR_SUCCESS ) break;  /* 查询磁盘是否准备好 */
		}
		if ( CH374DiskStatus < DISK_MOUNTED ) {  /* 未知USB设备,例如USB键盘、打印机等 */
			printf( "Unknown device\n" );
			continue;  /* 重新等待 */
		}

/* 查询磁盘物理容量 */
//		printf( "DiskSize\n" );
//		i = CH374DiskSize( );
//		mStopIfError( i );
//		printf( "TotalSize = %u MB \n", (unsigned int)( mCmdParam.DiskSize.mDiskSizeSec >> 11 ) );  /* 显示为以MB为单位的容量 */
		LED_RUN_ACT( );  /* 开始操作U盘 */

		printf( "Open and rename CH374HFT.C to CH374HFT.TXT \n" );
		strcpy( mCmdParam.Open.mPathName, "/CH374HFT.C" );  /* 原始文件名,该文件在根目录下 */
		LED_WR_ACT( );  /* 写操作 */
		i = RenameFileName( );  /* 修改文件名, C文件 => TXT文件 */
		mStopIfError( i );
		printf( "Set file create date & time to 2004.06.08 15:39:20 \n" );
		strcpy( mCmdParam.Open.mPathName, "/CH374HFT.TXT" );  /* 原始文件名 */
		i = SetFileCreateTime( MAKE_FILE_DATE( 2004, 6, 8 ), MAKE_FILE_TIME( 15, 39, 20 ) );  /* 为指定文件设置创建日期和时间 */
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
