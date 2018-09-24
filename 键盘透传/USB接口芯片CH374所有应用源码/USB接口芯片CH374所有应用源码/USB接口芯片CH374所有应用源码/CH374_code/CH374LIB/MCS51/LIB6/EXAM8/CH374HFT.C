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
/* 本程序用于演示将ADC模数采集的数据保存到U盘文件MY_ADC.TXT中 */
/* CH374的INT#引脚采用查询方式处理, 数据复制方式为"单DPTR",速度较慢,适用于所有MCS51单片机,
   本例以扇区为单位读写U盘文件,读写速度较字节模式快,
   由于扇区模式以扇区为基本单位,对于需要处理零碎数据的应用,不如字节模式方便,
   本范例演示在扇区模式下处理零碎数据,同时兼顾操作方便和较高速度,需要文件数据缓冲区 */


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
#define NO_DEFAULT_CH374_F_QUERY	1		/* 未调用CH374FileQuery程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_RESET		1		/* 未调用CH374Reset程序故禁止以节约代码 */

#include "..\CH374HF6.H"

UINT8X	my_buffer[ 0x3E00 ];			/* 外部RAM的文件数据缓冲区,缓冲区长度不小于一次读写的数据长度,本例要求不小于0x400即可 */
/* 如果准备使用双缓冲区交替读写,那么可以在参数中指定缓冲区起址 */

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

UINT16	total;	/* 记录当前缓冲区中的数据长度 */

/* 将准备写入U盘的零碎数据进行集中缓冲,组合成大数据块时再通过CH374真正写入U盘 */
/* 这样做的好处是: 提高速度(因为大数据块写入时效率高), 减少U盘损耗(U盘内部的内存寿命有限,不宜频繁擦写) */
void	mFlushBufferToDisk( UINT8 force )
/* force = 0 则自动刷新(检查缓冲区中的数据长度,满则写盘,不满则暂时放在缓冲区中), force != 0 则强制刷新(不管缓冲区中的数据有多少都写盘,通常在系统关机前应该强制写盘) */
{
	UINT8	i;
	UINT32	NewSize;
	if ( force ) {  /* 强制刷新 */
		mCmdParam.WriteX.mSectorCount = ( total + CH374vSectorSize - 1 ) / CH374vSectorSize;  /* 将缓冲区中的字节数转换为扇区数(除以CH374vSectorSize),长度加上CH374vSectorSize-1用于确保写入最后的零头数据 */
		mCmdParam.WriteX.mDataBuffer = my_buffer;  /* 缓冲区 */
		i = CH374FileWriteX( );  /* 以扇区为单位向文件写入数据,写入缓冲区中的所有数据,含最后的零头 */
		mStopIfError( i );
/* 有些U盘可能会要求在写数据后等待一会才能继续操作,所以,如果在某些U盘中发生数据丢失现象,建议在每次写入数据后稍作延时再继续 */
		CH374DelaymS( 1 );  /* 写后延时,可选的,大多数U盘不需要 */
		memcpy( my_buffer, & my_buffer[ total & ~ ( CH374vSectorSize - 1 ) ], total & ( CH374vSectorSize - 1 ) );  /* 将刚才已写入U盘的零头数据复制到缓冲区的头部 */
		total &= CH374vSectorSize - 1;  /* 缓冲区中只剩下刚才已写入U盘的零头数据,继续保留在缓冲区中是为了方便以后在其后面追加数据 */
		if ( total ) NewSize = CH374vFileSize - CH374vSectorSize + total;  /* 以扇区为单位,有零头数据,计算出真正的文件长度(有效数据的长度) */
		else NewSize = CH374vFileSize;  /* 以扇区为单位,没有零头数据,文件长度是CH374vSectorSize的倍数 */
		mCmdParam.Modify.mFileSize = NewSize;   /* 输入参数: 新的文件长度,扇区模式下涉及到零头数据不便自动更新长度 */
		mCmdParam.Modify.mFileAttr = 0xff;  /* 输入参数: 新的文件属性,为0FFH则不修改 */
		mCmdParam.Modify.mFileTime = 0xffff;  /* 输入参数: 新的文件时间,为0FFH则不修改 */
		mCmdParam.Modify.mFileDate = 0xffff;  /* 输入参数: 新的文件日期,为0FFH则不修改 */
		i = CH374FileModify( );   /* 修改当前文件的信息,修改文件长度 */
		mStopIfError( i );
		printf( "Current file size is %ld\n", CH374vFileSize );
		mCmdParam.Locate.mSectorOffset = 0xffffffff;  /* 移到文件的尾部,以扇区为单位,所以会忽略文件尾部的零头数据 */
		i = CH374FileLocate( );  /* 重新回到原文件的尾部,下面如果再写入数据将覆盖尾部零头数据,不过该零头数据有一份副本保留在缓冲区中,所以请放心 */
		mStopIfError( i );
	}
	else if ( total >= sizeof( my_buffer ) - CH374vSectorSize ) {  /* 缓冲区中的数据快要满了,所以应该先将缓冲区中的原有数据写入U盘 */
		mCmdParam.WriteX.mSectorCount = total / CH374vSectorSize;  /* 将缓冲区中的字节数转换为扇区数(除以CH374vSectorSize),最后的零头数据先不管 */
		mCmdParam.WriteX.mDataBuffer = my_buffer;  /* 缓冲区 */
		i = CH374FileWriteX( );  /* 以扇区为单位向文件写入数据,写入缓冲区中的所有数据,不含最后的零头 */
		mStopIfError( i );
		memcpy( my_buffer, & my_buffer[ total & ~ ( CH374vSectorSize - 1 ) ], total & ( CH374vSectorSize - 1 ) );  /* 将刚才未写入U盘的零头数据复制到缓冲区的头部 */
		total &= CH374vSectorSize - 1;  /* 缓冲区中只剩下刚才未写入U盘的零头数据 */
/*		mCmdParam.WriteX.mSectorCount = 0;  指定写入0扇区,用于刷新文件的长度,防止在没有来得及关闭文件之前断电而导致文件长度不符
		CH374FileWriteX( );  以扇区为单位向文件写入数据,因为是0扇区写入,所以只用于更新文件的长度,当阶段性写入数据后,可以用这种办法更新文件长度 */
	}
}

main( ) {
	UINT8	i, month, date, hour;
	UINT16	year, adc;
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
			if ( CH374DiskStatus < DISK_CONNECT ) break;  /* 检测到断开,重新检测并计时 */
			if ( CH374DiskStatus >= DISK_MOUNTED ) break;  /* 有的U盘总是返回未准备好,不过可以忽略,只要其建立连接MOUNTED且尝试5*50mS */
		}
		if ( CH374DiskStatus < DISK_CONNECT ) {  /* 检测到断开,重新检测并计时 */
			printf( "Device gone\n" );
			continue;  /* 重新等待 */
		}
		if ( CH374DiskStatus < DISK_MOUNTED ) {  /* 未知USB设备,例如USB键盘、打印机等 */
			printf( "Unknown device\n" );
			continue;  /* 重新等待 */
		}

#if DISK_BASE_BUF_LEN
		if ( DISK_BASE_BUF_LEN < CH374vSectorSize ) {  /* 检查磁盘数据缓冲区是否足够大,CH374vSectorSize是U盘的实际扇区大小 */
			printf( "Too large sector size\n" );
			continue;
		}
#endif

/* 查询磁盘物理容量 */
		printf( "DiskSize\n" );
		i = CH374DiskSize( );  
		mStopIfError( i );
		printf( "TotalSize = %u MB \n", (unsigned int)( mCmdParam.DiskSize.mDiskSizeSec * ( CH374vSectorSize / 512 ) >> 11 ) );  /* 显示为以MB为单位的容量 */
		LED_RUN_ACT( );  /* 开始操作U盘 */

/* 如果MY_ADC.TXT文件已经存在则添加数据到尾部,如果不存在则新建文件 */
		printf( "Open\n" );
		strcpy( mCmdParam.Open.mPathName, "/MY_ADC.TXT" );  /* 文件名,该文件在根目录下 */
		i = CH374FileOpen( );  /* 打开文件 */
		if ( i == ERR_SUCCESS ) {  /* 文件存在并且已经被打开,移动文件指针到尾部以便添加数据 */
			printf( "File size = %ld\n", CH374vFileSize );  /* 在成功打开文件后,全局变量CH374vFileSize中是文件当前长度 */
			printf( "Locate tail\n" );
			mCmdParam.Locate.mSectorOffset = 0xffffffff;  /* 移到文件的尾部,CH374子程序库内部是将文件长度按扇区长度CH374vSectorSize进行取整处理 */
			i = CH374FileLocate( );  /* 以扇区为单位移到文件尾部,如果文件尾部有不足一个扇区的零头数据则被忽略,如果不做处理那么零头数据将可能被写入数据覆盖 */
			mStopIfError( i );
			total = CH374vFileSize & ( CH374vSectorSize - 1 );  /* 上次保存文件时如果尾部有零头数据,那么先取得零头字节数,不满扇区长度的零碎数据 */
			printf( "Read last tail = %d Bytes\n", total );
			CH374vFileSize += CH374vSectorSize - 1;  /* 人为地将文件长度增加一个扇区减1,以便读出最后一个扇区中的零头数据 */
			mCmdParam.ReadX.mSectorCount = 1;  /* 读取文件尾部的零头数据,如果不人为增加文件长度,那么由于文件长度按CH374vSectorSize取整,导致尾部零头数据无法读出 */
			mCmdParam.ReadX.mDataBuffer = my_buffer;  /* 将读出的数据放到缓冲区中 */
			i = CH374FileReadX();  /* 从文件读取尾部零头数据,如果原尾部没有零头数据,那么什么也读不到,返回时mCmdParam.ReadX.mSectorCount为实际读出扇区数 */
			mStopIfError( i );
			CH374vFileSize -= CH374vSectorSize - 1;  /* 恢复真正的文件长度 */
			mCmdParam.Locate.mSectorOffset = 0xffffffff;  /* 移到文件的尾部,以扇区为单位,所以会忽略文件尾部的零头数据 */
			i = CH374FileLocate( );  /* 重新回到原文件的尾部,下面如果写入数据将覆盖原尾部零头数据,不过原零头数据刚才已经被读入内存,所以请放心 */
			mStopIfError( i );
		}
		else if ( i == ERR_MISS_FILE ) {  /* 没有找到文件,必须新建文件 */
			LED_WR_ACT( );  /* 写操作 */
			printf( "Create\n" );
//			strcpy( mCmdParam.Create.mPathName, "/MY_ADC.TXT" );  /* 文件名,该文件在根目录下,刚才已经提供给CH374FileOpen */
			i = CH374FileCreate( );  /* 新建文件并打开,如果文件已经存在则先删除后再新建 */
			mStopIfError( i );
			total = 0;  /* 此前没有零头数据 */
		}
		else mStopIfError( i );  /* 打开文件时出错 */
		LED_WR_ACT( );  /* 写操作 */
		printf( "Write begin\n" );
		total += sprintf( & my_buffer[ total ], "在本次添加数据之前,该文件已有数据的长度是 %ld 字节\xd\xa", CH374vFileSize );  /* 将新数据添加到缓冲区的尾部,累计缓冲区内的数据长度 */
		mFlushBufferToDisk( 0 );  /* 自动刷新缓冲区,检查缓冲区是否已满,满则写盘 */
		printf( "Write ADC data\n" );
		TR0=1;  /* 用定时器0的计数值代替ADC数据 */
		for ( month = 1; month != 12; month ++ ) {  /* 因为测试板上没有实时时钟芯片,所以用循环方式模拟月份 */
			for ( date = 1; date != 30; date ++ ) {  /* 因为测试板上没有实时时钟芯片,所以用循环方式模拟日期 */
				year = 2004;  /* 假定为2004年 */
				hour = TL1 & 0x1F;  /* 因为测试板上没有实时时钟芯片,所以用定时器1的计数代替进行演示 */
/*				adc = get_adc_data( ); */
				adc = ( (UINT16)TH0 << 8 ) | TL0;  /* 因为测试板上没有ADC,所以用定时器0的计数代替ADC数据演示 */
				total += sprintf( & my_buffer[ total ], "Year=%04d, Month=%02d, Date=%02d, Hour=%02d, ADC_data=%u\xd\xa", year, (UINT16)month, (UINT16)date, (UINT16)hour, adc );  /* 将二制制数据格式为一行字符串 */
				if ( month == 6 && ( date & 0x0F ) == 0 ) mFlushBufferToDisk( 1 );  /* 强制刷新缓冲区,定期强制刷新缓冲区,这样在突然断电后可以减少数据丢失 */
				else mFlushBufferToDisk( 0 );  /* 自动刷新缓冲区,检查缓冲区是否已满,满则写盘 */
				printf( "Current total is %d\n", total );  /* 用于监控检查 */
			}
		}
		printf( "Write end\n" );
		total += sprintf( & my_buffer[ total ], " ********************************* " );  /* 将新数据添加到缓冲区的尾部,累计缓冲区内的数据长度 */
		total += sprintf( & my_buffer[ total ], "这次的ADC数据到此结束,程序即将退出\xd\xa" );  /* 将新数据添加到缓冲区的尾部,累计缓冲区内的数据长度 */
		mFlushBufferToDisk( 1 );  /* 强制刷新缓冲区,因为系统要退出了,所以必须强制刷新 */
		printf( "Close\n" );
		mCmdParam.Close.mUpdateLen = 0;  /* 因为强制刷新缓冲区时已经更新了文件长度,所以这里不需要自动更新文件长度 */
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
