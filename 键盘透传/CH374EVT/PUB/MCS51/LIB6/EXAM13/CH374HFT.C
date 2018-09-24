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
/* 本程序用于演示列出指定目录下的所有文件，以及用于搜索/枚举文件名 */
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

#define DISK_BASE_BUF_LEN		2048	/* 默认的磁盘数据缓冲区大小为512字节,建议选择为2048甚至4096以支持某些大扇区的U盘,为0则禁止在.H文件中定义缓冲区并由应用程序在pDISK_BASE_BUF中指定 */
/* 如果需要复用磁盘数据缓冲区以节约RAM,那么可将DISK_BASE_BUF_LEN定义为0以禁止在.H文件中定义缓冲区,而由应用程序在调用CH375Init之前将与其它程序合用的缓冲区起始地址置入pDISK_BASE_BUF变量 */

#define NO_DEFAULT_CH374_F_ENUM		1		/* 未调用CH374FileEnumer程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_F_QUERY	1		/* 未调用CH374FileQuery程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_RESET		1		/* 未调用CH374Reset程序故禁止以节约代码 */

#define NO_DEFAULT_FILE_ENUMER		1		/* 禁止默认的文件名枚举回调程序,下面用自行编写的程序代替它 */

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

typedef struct _FILE_NAME {
	UINT32	DirStartClust;				/* 文件所在目录的起始簇号 */
//	UINT32	Size;						/* 文件长度 */
	UINT8	Name[8+3+1+1];				/* 文件名,共8+3字节,分隔符,结束符,因为未包含目录名所以是相对路径 */
	UINT8	Attr;						/* 文件属性 */
} FILE_NAME;
#define		MAX_FILE_COUNT		200
FILE_NAME	xdata	FileNameBuffer[ MAX_FILE_COUNT ];	/* 文件名结构 */
UINT16	FileCount;
UINT32	CurrentDirStartClust;			/* 保存当前目录的起始簇号,用于加快文件枚举和打开速度 */

/* 例子:列举指定目录下的所有文件 */
UINT8	ListFile( void )
// 输入参数mCmdParam.Open.mPathName[]为目录名字符串,形式与文件名相同,单个斜线则代表根目录
{
	UINT8	i;
	printf( "List Directory: %s\n", mCmdParam.Open.mPathName );  /* 显示目录名 */
//	for ( i = 0; i < MAX_PATH_LEN; i ++ ) {  /* 找目录名的结束符 */
//		if ( mCmdParam.Open.mPathName[i] == 0 ) break;
//	}
	i = strlen( mCmdParam.Open.mPathName );  /* 计算路径的长度,找目录名的结束符 */
	if ( i && mCmdParam.Open.mPathName[i-1] == '/' ) { }  /* 是根目录,或者是已经有路径分隔符 */
	else mCmdParam.Open.mPathName[i++] = '/';  /* 在当前目录下进行枚举,除根目录外都是相对路径,不是根目录则加路径分隔符 */
	mCmdParam.Open.mPathName[i++] = '*';  /* 枚举通配符,完整的路径例如"\*"或者"\C51\*"或者"\C51\CH374*"等 */
	mCmdParam.Open.mPathName[i] = 0xFF;  /* 0xFF指定枚举序号在CH374vFileSize中 */
	CH374vFileSize = 0xFFFFFFFF;  /* 快速连续枚举,每找到一个文件调用一次xFileNameEnumer回调子程序,如果值小于0x80000000则每次只枚举一个文件太慢 */
	i = CH374FileOpen( );  /* 枚举,由回调程序xFileNameEnumer产生记录保存到结构中 */
	if ( i == ERR_SUCCESS || i == ERR_FOUND_NAME || i == ERR_MISS_FILE ) {  /* 操作成功,通常不会返回ERR_SUCCESS,仅在xFileNameEnumer提前退出时才会返回ERR_FOUND_NAME */
		printf( "Success, new FileCount = %d\n", FileCount );
		return( ERR_SUCCESS );
	}
	else {
		printf( "Failed, new FileCount = %d\n", FileCount );
		return( i );
	}
}

UINT8	ListAll( void )  /* 以广度优先的算法枚举整个U盘中的所有文件及目录 */
{
	UINT8	i;
	UINT16	OldFileCount;
	OldFileCount = FileCount = 0;  /* 清文件结构计数 */
	FileNameBuffer[ 0 ].Name[0] = '/';  /* 根目录,是完整路径名,除根目录是绝对路径之外都是相对路径 */
	FileNameBuffer[ 0 ].Name[1] = 0;
	FileNameBuffer[ 0 ].DirStartClust = 0;  /* 根目录的这个参数无意义 */
	FileNameBuffer[ 0 ].Attr = ATTR_DIRECTORY;  /* 根目录也是目录,作为第一个记录保存 */

	for ( FileCount = 1; OldFileCount < FileCount; OldFileCount ++ ) {  /* 尚有新枚举到的文件名结构未进行分析 */
		if ( FileNameBuffer[ OldFileCount ].Attr & ATTR_DIRECTORY ) {  /* 是目录则继续进行深度搜索 */
			strcpy( mCmdParam.Open.mPathName, FileNameBuffer[ OldFileCount ].Name );  /* 目录名,除根目录外都是相对路径 */
			CH374vStartCluster = FileNameBuffer[ OldFileCount ].DirStartClust;  /* 当前目录的上级目录的起始簇号,便于用相对路径打开,比完整路径名速度快 */
			i = CH374FileOpen( );  /* 打开目录,仅为了获取目录的起始簇号以提高速度 */
			if ( i == ERR_SUCCESS ) return( ERR_MISS_DIR );  /* 应该是打开了目录,但是返回结果是打开了文件 */
			if ( i != ERR_OPEN_DIR ) return( i );
			if ( OldFileCount ) CurrentDirStartClust = CH374vStartCluster;  /* 不是根目录,获取目录的起始簇号 */
			else {  /* 是根目录,获取根目录的起始簇号 */
				if ( CH374vDiskFat == DISK_FAT32 ) CurrentDirStartClust = CH374vDiskRoot;  /* FAT32根目录 */
				else CurrentDirStartClust = 0;  /* FAT12/FAT16根目录 */
			}
			CH374FileClose( );  /* 对于根目录一定要关闭 */

//			strcpy( mCmdParam.Open.mPathName, FileNameBuffer[ OldFileCount ].Name );  /* 目录名,由于mPathName未被修改所以无需再复制 */
			CH374vStartCluster = FileNameBuffer[ OldFileCount ].DirStartClust;  /* 当前目录的上级目录的起始簇号,便于用相对路径打开,比完整路径名速度快 */
			i = ListFile( );  /* 枚举目录,由回调程序xFileNameEnumer产生记录保存到结构中 */
			if ( i != ERR_SUCCESS ) return( i );
		}
	}

// U盘中的文件及目录全部枚举完毕,下面开始根据结构记录依次打开文件 */
	printf( "Total file&dir = %d, Open every file:\n", FileCount );
	for ( OldFileCount = 0; OldFileCount < FileCount; OldFileCount ++ ) {
		if ( ( FileNameBuffer[ OldFileCount ].Attr & ATTR_DIRECTORY ) == 0 ) {  /* 是文件则打开,目录则跳过 */
			printf( "Open file: %s\n", FileNameBuffer[ OldFileCount ].Name );
			strcpy( mCmdParam.Open.mPathName, FileNameBuffer[ OldFileCount ].Name );  /* 相对路径 */
			CH374vStartCluster = FileNameBuffer[ OldFileCount ].DirStartClust;  /* 当前文件的上级目录的起始簇号,便于用相对路径打开,比完整路径名速度快 */
			i = CH374FileOpen( );  /* 打开文件 */
			if ( i == ERR_SUCCESS ) {  /* 成功打开了文件 */
				mCmdParam.ReadX.mDataBuffer = 0x2000;  /* 指向文件数据缓冲区的起始地址 */
				mCmdParam.ReadX.mSectorCount = 1;  /* 读取扇区数 */
				CH374FileReadX( );
//				CH374FileClose( );  /* 不做写操作可以无需关闭 */
			}
		}
	}
}

void xFileNameEnumer( void )			/* 文件名枚举回调子程序,参考CH374HF6.H文件中的例子 */
{  /* 每搜索到一个文件FileOpen都会调用本回调程序，xFileNameEnumer返回后，FileOpen递减CH374vFileSize并继续枚举直到搜索不到文件或者目录 */
	UINT8			i, c;
	P_FAT_DIR_INFO	pFileDir;
	PUINT8X			pNameBuf;
	pFileDir = (P_FAT_DIR_INFO)( pDISK_BASE_BUF + CH374vFdtOffset );  /* 当前FDT的起始地址 */
	if ( pFileDir -> DIR_Name[0] == '.' ) return;  /* 是本级或者上级目录名,必须丢弃不处理 */
	if ( ( pFileDir -> DIR_Attr & ATTR_DIRECTORY ) == 0 ) {  /* 判断是文件名 */
		if ( pFileDir -> DIR_Name[8] == 'H' && pFileDir -> DIR_Name[9] == ' '  /* 过滤文件的扩展名,是".H"文件,则丢弃,不登记不处理 */
			|| pFileDir -> DIR_Name[8] == 'E' && pFileDir -> DIR_Name[9] == 'X' && pFileDir -> DIR_Name[10] == 'E' ) return;  /* 扩展名是".EXE"的文件,则丢弃 */
	}
	pNameBuf = & FileNameBuffer[ FileCount ].Name;  /* 文件名结构中的文件名缓冲区 */
	for ( i = 0; i < 11; i ++ ) {  /* 复制文件名,长度为11个字符 */
		c = pFileDir -> DIR_Name[ i ];
		if ( i == 0 && c == 0x05 ) c = 0xE5;  /* 特殊字符 */
		if ( c != 0x20 ) {  /* 有效字符 */
			if ( i == 8 ) {  /* 处理扩展名 */
				*pNameBuf = '.';  /* 分隔符 */
				pNameBuf ++;
			}
			*pNameBuf = c;  /* 复制文件名的一个字符 */
			pNameBuf ++;
		}
	}
	*pNameBuf = 0;  /* 当前文件名完整路径的结束符 */
	FileNameBuffer[ FileCount ].DirStartClust = CurrentDirStartClust;  /* 记录当前目录的起始簇号,用于加快文件打开速度 */
	FileNameBuffer[ FileCount ].Attr = pFileDir -> DIR_Attr;  /* 文件属性 */
	if ( pFileDir -> DIR_Attr & ATTR_DIRECTORY ) {  /* 判断是目录名 */
		printf( "Dir %4d#: %s\n", FileCount, FileNameBuffer[ FileCount ].Name );
	}
	else {  /* 判断是文件名 */
		printf( "File%4d#: %s\n", FileCount, FileNameBuffer[ FileCount ].Name );
	}
	FileCount ++;  /* 文件计数 */
	if ( FileCount >= MAX_FILE_COUNT ) {  /* 文件名结构缓冲区太小,结构数量不足 */
		CH374vFileSize = 1;  /* 强行提前结束枚举,本次FileOpen将不再回调xFileNameEnumer而是提前返回,防止缓冲区溢出 */
		printf( "FileName Structure Full\n" );
	}
}

main( ) {
	UINT8	i;
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

#ifdef UNSUPPORT_USB_HUB
/* 如果不需要支持USB-HUB,那么等待U盘插入的程序与CH375相似,都是通过CH374DiskConnect查询连接,已连接则通过CH374DiskReady等待就绪,然后读写 */
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

		CH374DelaymS( 20 );

/* 查询磁盘物理容量 */
//		printf( "DiskSize\n" );
//		i = CH374DiskSize( );
//		mStopIfError( i );
//		printf( "TotalSize = %u MB \n", (unsigned int)( mCmdParam.DiskSize.mDiskSizeSec * ( CH374vSectorSize / 512 ) >> 11 ) );  /* 显示为以MB为单位的容量 */
		printf( "Current disk sector size = %d Bytes \n", CH374vSectorSize );  /* CH374vSectorSize是U盘的实际扇区大小 */
		LED_RUN_ACT( );  /* 开始操作U盘 */

		printf( "List all file \n" );
		i = ListAll( );  /* 枚举整个U盘中的所有文件及目录 */
		mStopIfError( i );

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
