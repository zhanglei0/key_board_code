/* 2005.01.01
****************************************
**  Copyright  (C)  W.ch  1999-2005   **
**  Web:  http://www.winchiphead.com  **
****************************************
**  KC7.0@MCS51                       **
****************************************
*/
/* CH374评估板演示程序: 演示USB-HOST主机接口和USB-DEVICE设备接口的应用 */
/* MCS-51单片机C语言的示例程序, 适用于89C52或者更大程序空间的单片机,也适用于ATMEL/PHILIPS/SST等具有1KB内部RAM的单片机 */

/* 关于本程序中的CH374主机接口: HOST.C
       可以连接U盘, 支持U盘文件系统FAT12/FAT16/FAT32, 容量不限,
       插入U盘后, 该程序将U盘中的/C51/CH374HFT.C文件中的前600个字符显示出来,
       如果找不到原文件CH374HFT.C, 那么该程序将显示C51子目录下所有以CH374开头的文件名,
       如果找不到C51子目录, 那么该程序将显示根目录下的所有文件名,
       最后将程序ROM中的一个字符串写入写入新建的文件"NEWFILE.TXT"中,
       CH374的INT#引脚采用查询方式处理, 数据复制方式为"单DPTR复制", 兼容性最好但是速度最慢,
       以字节为单位读写U盘文件, 读写速度较扇区模式慢, 不需要文件数据缓冲区FILE_DATA_BUF,
       总共只需要600字节的RAM, 适用于单片机硬件资源有限、数据量小并且读写速度要求不高的系统,
       计算机端可以通过串口监控/调试工具软件以9600bps查看演示情况, 也可以使用CH341的串口工具或者CH374模块的演示工具 */
/* 关于本程序中的CH374设备接口: DEVICE.C
       采用请求+应答模式通讯结构, 强调可靠性和交互性, 不追求传输速度,
       计算机端可以通过CH372/CH374的调试工具中的MCS51监控工具程序CH37XDBG.EXE实现对MCS51单片机的"完全"控制,
       可以读写MCS51单片机的任意外部RAM、内部RAM以及绝大多数SFR, 当然也能够进行数据通讯 */
/* 关于主从切换:
       本程序默认工作于USB-HOST主机方式, 当有USB设备连接时自动处理, 需要作为USB设备与计算机通讯时, 可以按评估板上的按钮由主程序进行切换 */


/* C51   CH374.C */
/* C51   HOST.C */
/* C51   DEVICE.C */
/* LX51  CH374.OBJ, HOST.OBJ, DEVICE.OBJ, CH374HF6.LIB, C51DPTR1.LIB */
/* OHX51 CH374 */


#define		CH374HF_NO_CODE		1
#include "CH374.H"

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

UINT8	IsKeyPress( )
{
	if ( USER_KEY_IN == 0 ) {  /* 有键按下 */
		LED_OUT_INACT( );  /* LED灭 */
		CH374DelaymS( 50 );
		if ( USER_KEY_IN == 0 ) {
			while ( USER_KEY_IN == 0 );  /* 等待按键释放 */
			CH374DelaymS( 50 );
			while ( USER_KEY_IN == 0 );  /* 按键去抖动 */
			LED_OUT_ACT( );  /* LED亮以示工作 */
			return( 1 );
		}
		LED_OUT_ACT( );  /* LED亮以示工作 */
	}
	return( 0 );
}

main( ) {
	LED_OUT_INIT( );
	LED_OUT_ACT( );  /* 开机后LED闪烁一下以示工作 */
	CH374DelaymS( 100 );  /* 延时100毫秒 */
	LED_OUT_INACT( );
	CH374DelaymS( 100 );

	mInitSTDIO( );  /* 为了让计算机通过串口监控演示过程 */
	printf( "Start CH374 demo ...\n" );

	EA = 1;
	LED_OUT_ACT( );  /* LED亮以示工作 */
	while ( 1 ) {  /* 用户按键导致USB主从模式来回切换 */
		LED_HOST( );
		printf( "Set USB host mode\n" );
		host( );
		LED_DEVICE( );
		printf( "Set USB device mode\n" );
		device( );
	}
}
