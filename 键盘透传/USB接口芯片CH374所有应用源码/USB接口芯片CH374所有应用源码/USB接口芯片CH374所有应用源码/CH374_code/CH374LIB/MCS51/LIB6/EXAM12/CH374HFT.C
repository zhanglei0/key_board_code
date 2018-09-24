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
/* 本程序用于演示检查U盘是否写保护,演示模拟计算机端的安全移除,也可以参考用于自行处理其它命令 */
/* CH374的INT#引脚采用查询方式处理, 适用于所有MCS51单片机 */


/* C51   CH374HFT.C */
/* LX51  CH374HFT.OBJ , CH374HF6.LIB, C51DPTR1.LIB */
/* OHX51 CH374HFT */

#include <reg52.h>
#include <stdio.h>
#include <string.h>

/* 以下定义的详细说明请看CH374HF6.H文件 */
#define	MAX_BYTE_IO				48		/* 以字节为单位单次读写文件时的最大长度,默认值是29,值大则占用内存多,值小则超过该长度必须分多次读写 */

#define LIB_CFG_INT_EN			0		/* CH374的INT#引脚连接方式,0为"查询方式",1为"中断方式" */

#define CH374_IDX_PORT_ADDR		0xBDF1	/* CH374索引端口的I/O地址 */
#define CH374_DAT_PORT_ADDR		0xBCF0	/* CH374数据端口的I/O地址 */
/* 62256提供的32KB的RAM分为两部分: 0000H-01FFH为磁盘读写缓冲区, 0200H-7FFFH为文件数据缓冲区 */
#define	DISK_BASE_BUF_ADDR		0x0000	/* 外部RAM的磁盘数据缓冲区的起始地址,从该单元开始的缓冲区长度为CH374vSectorSize */

#define CH374_INT_WIRE			INT0	/* P3.2, INT0, CH374的中断线INT#引脚,连接CH374的INT#引脚,用于查询中断状态 */
/* 如果未连接CH374的中断引脚,那么应该去掉上述定义,自动使用寄存器查询方式 */

//#define DISK_BASE_BUF_LEN		2048	/* 默认的磁盘数据缓冲区大小为512字节,建议选择为2048甚至4096以支持某些大扇区的U盘,为0则禁止在.H文件中定义缓冲区并由应用程序在pDISK_BASE_BUF中指定 */
/* 如果需要复用磁盘数据缓冲区以节约RAM,那么可将DISK_BASE_BUF_LEN定义为0以禁止在.H文件中定义缓冲区,而由应用程序在调用CH375Init之前将与其它程序合用的缓冲区起始地址置入pDISK_BASE_BUF变量 */

#define NO_DEFAULT_CH374_F_ENUM		1		/* 未调用CH374FileEnumer程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_F_QUERY	1		/* 未调用CH374FileQuery程序故禁止以节约代码 */
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

/* 检查U盘是否写保护, 返回ERR_SUCCESS说明可以写,返回0xFF说明只读/写保护,返回其它值说明是错误代码 */
/* 其它BulkOnly传输协议的命令可以参考这个例子处理,修改前必须了解USB MassStorage和SCSI规范 */
UINT8	IsDiskWriteProtect( void )
{
	UINT8	mLength, mStatus, mDevSpecParam;
	mLength = 0x10;
	mBOC.mCBW.mCBW_Flag = 0x80;  /* 传输方向为输入 */
	if ( CH374vRetryCount & (1<<5) ) {  /* 根据子类选择命令码,位5为1则USB存储设备的子类为6 */
		mBOC.mCBW.mCBW_DataLen0 = mLength;  /* 数据传输长度 */
		mBOC.mCBW.mCBW_CB_Len = 0x06;  /* 命令块长度 */
		mBOC.mCBW.mCBW_CB_Buf[0] = 0x1A;  /* 命令块首字节, MODE SENSE(6) */
		mBOC.mCBW.mCBW_CB_Buf[1] = 0x00;
		mBOC.mCBW.mCBW_CB_Buf[2] = 0x3F;
		mBOC.mCBW.mCBW_CB_Buf[3] = 0x00;
		mBOC.mCBW.mCBW_CB_Buf[4] = mLength;
		mBOC.mCBW.mCBW_CB_Buf[5] = 0x00;
	}
	else {
		mBOC.mCBW.mCBW_DataLen0 = mLength;  /* 数据传输长度 */
		mBOC.mCBW.mCBW_CB_Len = 0x0A;  /* 命令块长度 */
		mBOC.mCBW.mCBW_CB_Buf[0] = 0x5A;  /* 命令块首字节, MODE SENSE(10) */
		mBOC.mCBW.mCBW_CB_Buf[1] = 0x00;
		mBOC.mCBW.mCBW_CB_Buf[2] = 0x3F;
		mBOC.mCBW.mCBW_CB_Buf[3] = 0x00;
		mBOC.mCBW.mCBW_CB_Buf[4] = 0x00;
		mBOC.mCBW.mCBW_CB_Buf[5] = 0x00;
		mBOC.mCBW.mCBW_CB_Buf[6] = 0x00;
		mBOC.mCBW.mCBW_CB_Buf[7] = 0x00;
		mBOC.mCBW.mCBW_CB_Buf[8] = mLength;
		mBOC.mCBW.mCBW_CB_Buf[9] = 0x00;
	}
	mStatus = CH374BulkOnlyCmd( );  /* 执行基于BulkOnly协议的命令,对USB存储器执行MODE SENSE命令 */
	if ( mStatus == ERR_SUCCESS ) {  /* 操作成功 */
		mLength -= mBOC.mCBW.mCBW_DataLen0;  /* 操作成功后mBOC.mCBW.mCBW_DataLen0为剩余长度,计算得到实际传输长度 */
		if ( mLength > 3 ) {  /* MODE SENSE命令返回数据的长度有效 */
			if ( CH374vRetryCount & (1<<5) ) mDevSpecParam = *(pDISK_BASE_BUF+2);  /* MODE SENSE(6), device specific parameter */
			else mDevSpecParam = *(pDISK_BASE_BUF+3);  /* MODE SENSE(10), device specific parameter */
			if ( mDevSpecParam & 0x80 ) return( 0xFF );  /* U盘写保护 */
			else return( ERR_SUCCESS );  /* U盘没有写保护 */
		}
		return( ERR_USB_DISK_ERR );
	}
	mLength = 0x12;  /* 如果真的需要REQUEST SENSE命令的返回数据,那么此处的长度必须大于0x12 */
	mBOC.mCBW.mCBW_Flag = 0x80;
	mBOC.mCBW.mCBW_DataLen0 = mLength;
	mBOC.mCBW.mCBW_CB_Len = 0x06;  /* 命令块长度 */
	mBOC.mCBW.mCBW_CB_Buf[0] = SPC_CMD_REQUEST_SENSE;
	mBOC.mCBW.mCBW_CB_Buf[1] = 0;
	mBOC.mCBW.mCBW_CB_Buf[2] = 0;
	mBOC.mCBW.mCBW_CB_Buf[3] = 0;
	mBOC.mCBW.mCBW_CB_Buf[4] = mLength;
	mBOC.mCBW.mCBW_CB_Buf[5] = 0;
	CH374BulkOnlyCmd( );  /* 执行基于BulkOnly协议的命令,对USB存储器执行REQUEST SENSE命令 */
	return( mStatus );
}

/* 安全移除U盘, 返回ERR_SUCCESS说明可以安全移除,否则说明不能安全移除,只能强行移除 */
/* 在操作完U盘准备让用户拔出U盘前调用, 防止用户过早拔出U盘丢失数据 */
UINT8	SafeRemoveDisk( void )
{
	UINT8	i, s;
	for ( i = 0; i < 10; i ++ ) {  /* 有的U盘总是返回未准备好,不过可以被忽略 */
		CH374DelaymS( 100 );
		if ( CH374DiskReady( ) == ERR_SUCCESS ) break;  /* 查询磁盘是否准备好 */
	}
	CH374DelaymS( 10 );
	CH374_WRITE_BLOCK_C( 8, (PUINT8C)"\x00\x09\x00\x00\x00\x00\x00" );
	s = CH374CtrlTransfer( );  /* 设置USB设备端的配置值,取消配置,会导致很多U盘的LED灯灭 */
	CH374DelaymS( 10 );
	if ( i < 5 && s == ERR_SUCCESS ) return( ERR_SUCCESS );  /* 操作成功 */
	else return( 0xFF );
/* 以下取消SOF包会导致绝大多数U盘的LED灯灭,CH374在检测到U盘插入后会自动恢复SOF包 */
//	CH374_WRITE_REGISTER( REG_USB_SETUP, BIT_SETP_HOST_MODE );  /* USB主机模式,USB总线空闲,停止发出SOF包 */
//	CH374DelaymS( 1 );
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

		while ( 1 ) {  /* 支持USB-HUB */
			CH374DelaymS( 50 );  /* 没必要频繁查询 */
			if ( CH374DiskConnect( ) == ERR_SUCCESS ) {  /* 查询方式: 检查磁盘是否连接并更新磁盘状态,返回成功说明连接 */
				CH374DelaymS( 200 );  /* 延时,可选操作,有的USB存储器需要几十毫秒的延时 */

/* 对于检测到USB设备的,最多等待100*50mS,主要针对有些MP3太慢,对于检测到USB设备并且连接DISK_MOUNTED的,最多等待5*50mS,主要针对DiskReady不过的 */
				for ( i = 0; i < 100; i ++ ) {  /* 最长等待时间,100*50mS */
					CH374DelaymS( 50 );
					printf( "Ready ?\n" );
					if ( CH374DiskReady( ) == ERR_SUCCESS ) break;  /* 查询磁盘是否准备好 */
					if ( CH374DiskStatus < DISK_CONNECT ) {  /* 检测到断开,重新检测并计时 */
						printf( "Device gone\n" );
						break;  /* 重新等待 */
					}
					if ( CH374DiskStatus >= DISK_MOUNTED && i > 5 ) break;  /* 有的U盘总是返回未准备好,不过可以忽略,只要其建立连接MOUNTED且尝试5*50mS */
					if ( CH374DiskStatus == DISK_CONNECT ) {  /* 有设备连接 */
						if ( CH374vHubPortCount ) {  /* 连接了一个USB-HUB,但可能没有U盘 */
							printf( "No Udisk in USB_HUB\n" );
							break;
						}
						else {  /* 未知USB设备,有可能是U盘反应太慢,所以要再试试 */
						}
					}
				}
				if ( CH374DiskStatus >= DISK_MOUNTED ) {  /* 是U盘 */
					break;  /* 开始操作U盘 */
				}
				if ( CH374DiskStatus == DISK_CONNECT ) {  /* 多次尝试还是不行,估计不是U盘 */
					if ( CH374vHubPortCount ) {  /* 连接了一个USB-HUB,但可能没有U盘 */
/* 在while中等待HUB端口有U盘 */
					}
					else {  /* 未知USB设备,例如USB键盘、打印机等,而且已经试了很多次还不行 */
						printf( "Unknown device\n" );
						goto UnknownUsbDevice;
					}
				}
			}
		}

		LED_OUT_ACT( );  /* LED亮 */

		CH374DelaymS( 20 );

/* 查询磁盘物理容量 */
//		printf( "DiskSize\n" );
//		i = CH374DiskSize( );
//		mStopIfError( i );
//		printf( "TotalSize = %u MB \n", (unsigned int)( mCmdParam.DiskSize.mDiskSizeSec * ( CH374vSectorSize / 512 ) >> 11 ) );  /* 显示为以MB为单位的容量 */
		printf( "Current disk sector size = %d Bytes \n", CH374vSectorSize );  /* CH374vSectorSize是U盘的实际扇区大小 */
		LED_RUN_ACT( );  /* 开始操作U盘 */

		printf( "Check Disk Write Protect ? ...\n" );
		i = IsDiskWriteProtect( );  /* 检查U盘是否写保护, 返回ERR_SUCCESS说明可以写,返回0xFF说明只读/写保护,返回其它值说明是错误代码 */
		if ( i != ERR_SUCCESS && i != 0xFF ) {  /* 操作失败 */
			printf( "Again ...\n" );
			CH374DelaymS( 100 );
			i = IsDiskWriteProtect( );  /* 再试一次 */
		}
		if ( i == ERR_SUCCESS ) {  /* 可以写 */
			printf( "... No !\n" );
			LED_WR_ACT( );  /* 写操作 */
			printf( "Create a File\n" );
			strcpy( mCmdParam.Create.mPathName, "\\NEWFILE.TXT" );  /* 新文件名,在根目录下 */
			i = CH374FileCreate( );  /* 新建文件并打开,如果文件已经存在则先删除后再新建 */
			mStopIfError( i );
			mCmdParam.ByteWrite.mByteBuffer[0] = 'O';
			mCmdParam.ByteWrite.mByteBuffer[1] = 'K';
			mCmdParam.ByteWrite.mByteCount = 2;  /* 写入数据的字符数,单次读写的长度不能超过MAX_BYTE_IO,第二次调用时接着刚才的向后写 */
			i = CH374ByteWrite( );  /* 向文件写入数据 */
			mStopIfError( i );
			printf( "Close\n" );
			mCmdParam.Close.mUpdateLen = 0;  /* 不要自动计算文件长度,如果自动计算,那么该长度总是CH374vSectorSize的倍数 */
			i = CH374FileClose( );
			mStopIfError( i );
			LED_WR_INACT( );
			if ( SafeRemoveDisk( ) == ERR_SUCCESS ) {  /* 安全移除U盘 */
				printf( "Safe Remove !\n" );
			}
			else {
				printf( "Unsafe Remove !\n" );
			}
		}
		else if ( i == 0xFF ) {  /* 写保护 */
			printf( "... Yes !\n" );
		}
		else {
			mStopIfError( i );  /* 显示错误代码 */
		}
		LED_RUN_INACT( );
UnknownUsbDevice:
		printf( "Take out\n" );
		while ( 1 ) {  /* 支持USB-HUB */
			CH374DelaymS( 10 );  /* 没必要频繁查询 */
			if ( CH374DiskConnect( ) != ERR_SUCCESS ) break;  /* 查询方式: 检查磁盘是否连接并更新磁盘状态,返回成功说明连接 */
		}
		LED_OUT_INACT( );  /* LED灭 */
		CH374DelaymS( 200 );
	}
}
