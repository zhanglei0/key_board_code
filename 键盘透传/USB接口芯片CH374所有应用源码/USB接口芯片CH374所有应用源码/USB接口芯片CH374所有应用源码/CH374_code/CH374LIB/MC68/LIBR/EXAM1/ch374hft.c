/* 2004.06.05
****************************************
**  Copyright  (C)  W.ch  1999-2004   **
**  Web:  http://www.winchiphead.com  **
****************************************
**  USB Host File Interface for CH374 **
**  TC2.0@PC, gcc 2.95.3@MC68         **
****************************************
*/
/* CH374 主机文件系统接口 */
/* 支持: FAT12/FAT16/FAT32 */

/* MC68单片机C语言的U盘文件读写示例程序 */
/* 该程序将U盘中的/C51/CH374HFT.C文件中的小写字母转成大写字母后, 写到新建的文件NEWFILE.TXT中,
   如果找不到原文件CH374HFT.C, 那么该程序将显示C51子目录下所有以CH374开头的文件名, 并新建NEWFILE.TXT文件并写入提示信息,
   如果找不到C51子目录, 那么该程序将显示根目录下的所有文件名, 并新建NEWFILE.TXT文件并写入提示信息
*/
/* CH374的INT#引脚采用查询方式处理, 数据复制方式为"内部复制", 本程序适用于MC68单片机uClinux */

/* m68k-elf-gcc -m68000 ??... */

#include <stdio.h>
#include <string.h>

/* 以下定义的详细说明请看CH374HFR.H文件 */
#define LIB_CFG_INT_EN			0		/* CH374的INT#引脚连接方式,0为"查询方式",1为"中断方式" */

#define CH374_INT_WIRE			( *(UINT8V *)0xFFFFF419 & 0x02 )	/* CH374的中断线INT#引脚,连接CH374的INT#引脚,用于查询中断状态 */
/* 如果未连接CH374的中断引脚,那么应该去掉上述定义,自动使用寄存器查询方式 */

#define DISK_BASE_BUF_LEN		2048	/* 默认的磁盘数据缓冲区大小为512字节,建议选择为2048甚至4096以支持某些大扇区的U盘,为0则禁止在.H文件中定义缓冲区并由应用程序在pDISK_BASE_BUF中指定 */
/* 如果需要复用磁盘数据缓冲区以节约RAM,那么可将DISK_BASE_BUF_LEN定义为0以禁止在.H文件中定义缓冲区,而由应用程序在调用CH375Init之前将与其它程序合用的缓冲区起始地址置入pDISK_BASE_BUF变量 */

#define NO_DEFAULT_CH374_F_ENUM		1		/* 未调用CH374FileEnumer程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_F_QUERY	1		/* 未调用CH374FileQuery程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_RESET		1		/* 未调用CH374Reset程序故禁止以节约代码 */

#include "../CH374HFR.h"

UINT8	my_buffer[ 0x2000 ];			/* 外部RAM的文件数据缓冲区 */
/* 如果准备使用双缓冲区交替读写,那么可以在参数中指定缓冲区起址 */

#define CH374_IDX_PORT	(*(UINT8V *)0x1000001E)		/* CH374索引端口的I/O地址 */
#define CH374_DAT_PORT	(*(UINT8V *)0x1000001C)		/* CH374数据端口的I/O地址 */

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

#include "para.c"	/* 并口时序 */
/*#include "spi.c"*/    /* SPI时序 */

/* 在PIN连接一个LED用于监控演示程序的进度,低电平LED亮 */
#define LED_OUT_INIT( )		{  }	/* 高电平为输出方向 */
#define LED_OUT_ACT( )		{  }	/* 低电平驱动LED显示 */
#define LED_OUT_INACT( )	{  }	/* 低电平驱动LED显示 */

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

int	main( ) {
	UINT8	i, c, SecCount;
	UINT16	NewSize, count;  /* 因为演示板的RAM容量只有32KB,所以NewSize限制为16位,实际上如果文件大于32256字节,应该分几次读写并且将NewSize改为UINT32以便累计 */
	PUINT8	pCodeStr;
	CH374_PORT_INIT( );
	LED_OUT_INIT( );
	LED_OUT_ACT( );  /* 开机后LED亮一下以示工作 */
	CH374DelaymS( 100 );  /* 延时100毫秒 */
	LED_OUT_INACT( );
	printf( "Start\n" );

#if DISK_BASE_BUF_LEN == 0
	pDISK_BASE_BUF = &my_buffer[0];  /* 不在.H文件中定义CH374的专用缓冲区,而是用缓冲区指针指向其它应用程序的缓冲区便于合用以节约RAM */
#endif

	i = CH374LibInit( );  /* 初始化CH374程序库和CH374芯片,操作成功返回0 */
	mStopIfError( i );
/* 其它电路初始化 */

	while ( 1 ) {
		printf( "Wait Udisk\n" );

#ifdef UNSUPPORT_USB_HUB
/* 如果不需要支持USB-HUB,那么等待U盘插入的程序与CH374相似,都是通过CH374DiskConnect查询连接,已连接则通过CH374DiskReady等待就绪,然后读写 */
		while ( CH374DiskStatus < DISK_CONNECT ) {  /* 查询CH374中断并更新中断状态,等待U盘插入 */
			CH374DiskConnect( );
			CH374DelaymS( 50 );  /* 没必要频繁查询 */
		}
		LED_OUT_ACT( );  /* LED亮 */
		CH374DelaymS( 200 );  /* 延时,可选操作,有的USB存储器需要几十毫秒的延时 */

/* 对于检测到USB设备的,最多等待100*50mS,主要针对有些MP3太慢,对于检测到USB设备并且连接DISK_MOUNTED的,最多等待5*50mS,主要针对DiskReady不过的 */
		for ( i = 0; i < 100; i ++ ) {  /* 最长等待时间,100*50mS */
			CH374DelaymS( 50 );
			printf( "Ready ?\n" );
			if ( CH374DiskReady( ) == ERR_SUCCESS ) break;  /* 查询磁盘是否准备好 */
			if ( CH374DiskStatus < DISK_CONNECT ) break;  /* 检测到断开,重新检测并计时 */
			if ( CH374DiskStatus >= DISK_MOUNTED && i > 5 ) break;  /* 有的U盘总是返回未准备好,不过可以忽略,只要其建立连接MOUNTED且尝试5*50mS */
		}
		if ( CH374DiskStatus < DISK_CONNECT ) {  /* 检测到断开,重新检测并计时 */
			printf( "Device gone\n" );
			continue;  /* 重新等待 */
		}
		if ( CH374DiskStatus < DISK_MOUNTED ) {  /* 未知USB设备,例如USB键盘、打印机等 */
			printf( "Unknown device\n" );
			goto UnknownUsbDevice;
		}
#else
/* 如果需要支持USB-HUB,那么必须参考本例中下面的等待程序 */
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
#endif

#if DISK_BASE_BUF_LEN
		if ( DISK_BASE_BUF_LEN < CH374vSectorSize ) {  /* 检查磁盘数据缓冲区是否足够大,CH374vSectorSize是U盘的实际扇区大小 */
			printf( "Too large sector size\n" );
			goto UnknownUsbDevice;
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
		strcpy( (char *)mCmdParam.Open.mPathName, "\\C51\\CH374HFT.C" );  /* 文件名,该文件在C51子目录下 */
		i = CH374FileOpen( );  /* 打开文件 */
		if ( i == ERR_MISS_DIR || i == ERR_MISS_FILE ) {  /* 没有找到文件 */
/* 列出文件 */
			if ( i == ERR_MISS_DIR ) pCodeStr = (PUINT8)"\\*";  /* C51子目录不存在则列出根目录下的文件 */
			else pCodeStr = (PUINT8)"\\C51\\CH*";  /* CH374HFT.C文件不存在则列出\C51子目录下的以CH开头的文件 */
			printf( "List file %s\n", pCodeStr );
			for ( count = 0; count < 10000; count ++ ) {  /* 最多搜索前10000个文件,实际上没有限制 */
				strcpy( (char *)mCmdParam.Open.mPathName, (char *)pCodeStr );  /* 搜索文件名,*为通配符,适用于所有文件或者子目录 */
				i = strlen( (char *)mCmdParam.Open.mPathName );
				mCmdParam.Open.mPathName[ i ] = 0xFF;  /* 根据字符串长度将结束符替换为搜索的序号,从0到254,如果是0xFF即255则说明搜索序号在CH374vFileSize变量中 */
				CH374vFileSize = count;  /* 指定搜索/枚举的序号 */
				i = CH374FileOpen( );  /* 打开文件,如果文件名中含有通配符*,则为搜索文件而不打开 */
/* CH374FileEnum 与 CH374FileOpen 的唯一区别是当后者返回ERR_FOUND_NAME时那么对应于前者返回ERR_SUCCESS */
				if ( i == ERR_MISS_FILE ) break;  /* 再也搜索不到匹配的文件,已经没有匹配的文件名 */
				if ( i == ERR_FOUND_NAME ) {  /* 搜索到与通配符相匹配的文件名,文件名及其完整路径在命令缓冲区中 */
					printf( "  match file %04d#: %s\n", (unsigned int)count, mCmdParam.Open.mPathName );  /* 显示序号和搜索到的匹配文件名或者子目录名 */
					continue;  /* 继续搜索下一个匹配的文件名,下次搜索时序号会加1 */
				}
				else {  /* 出错 */
					mStopIfError( i );
					break;
				}
			}
			pCodeStr = (PUINT8)"找不到/C51/CH374HFT.C文件\xd\n";
			for ( i = 0; i != 255; i ++ ) {
				if ( ( my_buffer[i] = *pCodeStr ) == 0 ) break;
				pCodeStr++;
			}
			NewSize = i;  /* 新文件的长度 */
			SecCount = 1;  /* (NewSize+CH374vSectorSize-1)/CH374vSectorSize, 计算文件的扇区数,因为读写是以扇区为单位的 */
		}
		else {  /* 找到文件或者出错 */
			mStopIfError( i );
/*			printf( "Query\n" );
			i = CH374FileQuery( );  查询当前文件的信息
			mStopIfError( i );*/
			printf( "Read\n" );
			if ( CH374vFileSize > sizeof( my_buffer ) ) {  /* 原文件长度大于缓冲区长度,一次读取不完 */
				SecCount = sizeof( my_buffer ) / CH374vSectorSize;  /* 由于演示板用的62256只有32K字节,其中CH374子程序用CH374vSectorSize个字节,所以只读取不超过60个扇区,也就是不超过30K字节 */
				NewSize = sizeof( my_buffer );  /* 由于RAM有限所以限制长度 */
			}
			else {  /* 如果原文件较小,那么使用原长度 */
				SecCount = ( CH374vFileSize + CH374vSectorSize - 1 ) / CH374vSectorSize;  /* 计算文件的扇区数,因为读写是以扇区为单位的,先加CH374vSectorSize-1是为了读出文件尾部不足1个扇区的部分 */
				NewSize = (UINT16)CH374vFileSize;  /* 原文件的长度 */
			}
			printf( "Size=%ld, Len=%d, Sec=%d\n", CH374vFileSize, NewSize, (UINT16)SecCount );
			mCmdParam.ReadX.mSectorCount = SecCount;  /* 读取全部数据,如果超过60个扇区则只读取60个扇区 */
			mCmdParam.ReadX.mDataBuffer = &my_buffer[0];  /* 指向文件数据缓冲区的起始地址 */
			CH374vFileSize += CH374vSectorSize - 1;  /* 默认情况下,以扇区方式读取数据时,无法读出文件尾部不足1个扇区的部分,所以必须临时加大文件长度以读取尾部零头 */
			i = CH374FileReadX( );  /* 从文件读取数据 */
			CH374vFileSize -= CH374vSectorSize - 1;  /* 恢复原文件长度 */
			mStopIfError( i );
/*
		如果文件比较大,一次读不完,可以再调用CH374FileReadX继续读取,文件指针自动向后移动
		while ( 1 ) {
			c = 32;   每次读取32个扇区
			mCmdParam.ReadX.mSectorCount = c;   指定读取的扇区数
			mCmdParam.ReadX.mDataBuffer = &my_buffer[0];  指向文件数据缓冲区的起始地址
			CH374FileReadX();   读完后文件指针自动后移
			处理数据
			if ( mCmdParam.ReadX.mSectorCount < c ) break;   实际读出的扇区数较小则说明文件已经结束
		}

	    如果希望从指定位置开始读写,可以移动文件指针
		mCmdParam.Locate.mSectorOffset = 3;  跳过文件的前3个扇区开始读写
		i = CH374FileLocate( );
		mCmdParam.ReadX.mSectorCount = 10;
		mCmdParam.ReadX.mDataBuffer = &my_buffer[0];  指向文件数据缓冲区的起始地址
		CH374FileReadX();   直接读取从文件的第(CH374vSectorSize*3)个字节开始的数据,前3个扇区被跳过

	    如果希望将新数据添加到原文件的尾部,可以移动文件指针
		i = CH374FileOpen( );
		mCmdParam.Locate.mSectorOffset = 0xffffffff;  移到文件的尾部,以扇区为单位,如果原文件是3字节,则从CH374vSectorSize个字节处开始添加
		i = CH374FileLocate( );
		mCmdParam.WriteX.mSectorCount = 10;
		mCmdParam.WriteX.mDataBuffer = &my_buffer[0];
		CH374FileWriteX();   在原文件的后面添加数据

使用CH374FileReadX可以自行定义数据缓冲区的起始地址
		mCmdParam.ReadX.mSectorCount = 2;
		mCmdParam.ReadX.mDataBuffer = 0x2000;  将读出的数据放到2000H开始的缓冲区中
		CH374FileReadX();   从文件中读取2个扇区到指定缓冲区

使用CH374FileWriteX可以自行定义数据缓冲区的起始地址
		mCmdParam.WiiteX.mSectorCount = 2;
		mCmdParam.WriteX.mDataBuffer = 0x4600;  将4600H开始的缓冲区中的数据写入
		CH374FileWriteX();   将指定缓冲区中的数据写入2个扇区到文件中

*/
			printf( "Close\n" );
			i = CH374FileClose( );  /* 关闭文件 */
			mStopIfError( i );

			i = my_buffer[100];
			my_buffer[100] = 0;  /* 置字符串结束标志,最多显示100个字符 */
			printf( "Line 1: %s\n", my_buffer );
			my_buffer[100] = i;  /* 恢复原字符 */
			for ( count=0; count < NewSize; count ++ ) {  /* 将文件中的小写字符转换为大写 */
				c = my_buffer[ count ];
				if ( c >= 'a' && c <= 'z' ) my_buffer[ count ] = c - ( 'a' - 'A' );
			}
		}

#ifdef EN_DISK_WRITE  /* 子程序库支持写操作 */
/* 产生新文件 */
		printf( "Create\n" );
		strcpy( (char *)mCmdParam.Create.mPathName, "\\NEWFILE.TXT" );  /* 新文件名,在根目录下 */
		i = CH374FileCreate( );  /* 新建文件并打开,如果文件已经存在则先删除后再新建 */
		mStopIfError( i );
		printf( "Write\n" );
		mCmdParam.WriteX.mSectorCount = SecCount;  /* 写入所有扇区的数据 */
		mCmdParam.WriteX.mDataBuffer = &my_buffer[0];  /* 指向文件数据缓冲区的起始地址 */
		i = CH374FileWriteX( );  /* 向文件写入数据 */
		mStopIfError( i );
/* 默认情况下,如果扇区数mCmdParam.WriteX.mSectorCount不为0那么CH374FileWriteX只负责写入数据而不修改文件长度,
   如果长时间不写入数据则应该更新文件长度,防止突然断电后前面写入的数据与文件长度不相符,
   如果需要写完数据后立即修改/更新文件长度,那么可以置扇区数mCmdParam.WriteX.mSectorCount为0后调用CH374FileWriteX强行更新文件长度,
   如果确定不会突然断电或者后面很快有数据不断写入则不必更新文件长度,可以提高速度并减少U盘损耗(U盘内部的内存寿命有限,不宜频繁改写) */
		printf( "Modify\n" );
		mCmdParam.Modify.mFileAttr = 0xff;  /* 输入参数: 新的文件属性,为0FFH则不修改 */
		mCmdParam.Modify.mFileTime = 0xffff;  /* 输入参数: 新的文件时间,为0FFFFH则不修改,使用新建文件产生的默认时间 */
		mCmdParam.Modify.mFileDate = MAKE_FILE_DATE( 2004, 5, 18 );  /* 输入参数: 新的文件日期: 2004.05.18 */
		mCmdParam.Modify.mFileSize = NewSize;  /* 输入参数: 如果原文件较小,那么新的文件长度与原文件一样长,否则被RAM所限,如果文件长度大于64KB,那么NewSize必须为UINT32 */
		i = CH374FileModify( );  /* 修改当前文件的信息,修改日期和长度 */
		mStopIfError( i );
		printf( "Close\n" );
		mCmdParam.Close.mUpdateLen = 0;  /* 不要自动计算文件长度,如果自动计算,那么该长度总是CH374vSectorSize的倍数 */
		i = CH374FileClose( );
		mStopIfError( i );

/* 删除某文件 */
/*		printf( "Erase\n" );
		strcpy( (char *)mCmdParam.Create.mPathName, "\\OLD" );  将被删除的文件名,在根目录下
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
