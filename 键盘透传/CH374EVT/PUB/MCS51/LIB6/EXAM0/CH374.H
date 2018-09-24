#include <reg52.h>
#include <stdio.h>
#include <string.h>

#define	USER_KEY_IN				INT1	/* P3.3, INT1, 默认为高电平, 当用户按键后为低电平, 本程序仅作查询 */

/* 以下定义的详细说明请看CH374HF6.H文件 */
#define LIB_CFG_INT_EN			0		/* CH374的INT#引脚连接方式,0为"查询方式",1为"中断方式" */

#define CH374_IDX_PORT_ADDR		0xBDF1	/* CH374索引端口的I/O地址 */
#define CH374_DAT_PORT_ADDR		0xBCF0	/* CH374数据端口的I/O地址 */
/* 只使用单片机内置的1KB外部RAM: 0000H-01FFH 为磁盘读写缓冲区, 以字节为单位读写文件不需要文件数据读写缓冲区FILE_DATA_BUF */
#define	DISK_BASE_BUF_ADDR		0x0000	/* 外部RAM的磁盘数据缓冲区的起始地址,从该单元开始的缓冲区长度为CH374vSectorSize */

#define CH374_INT_WIRE			INT0	/* P3.2, INT0, CH374的中断线INT#引脚,连接CH374的INT#引脚,用于查询中断状态 */
/* 如果未连接CH374的中断引脚,那么应该去掉上述定义,自动使用寄存器查询方式 */

//#define DISK_BASE_BUF_LEN		2048	/* 默认的磁盘数据缓冲区大小为512字节,建议选择为2048甚至4096以支持某些大扇区的U盘,为0则禁止在.H文件中定义缓冲区并由应用程序在pDISK_BASE_BUF中指定 */
/* 如果需要复用磁盘数据缓冲区以节约RAM,那么可将DISK_BASE_BUF_LEN定义为0以禁止在.H文件中定义缓冲区,而由应用程序在调用CH375Init之前将与其它程序合用的缓冲区起始地址置入pDISK_BASE_BUF变量 */

#define NO_DEFAULT_CH374_F_ENUM		1		/* 未调用CH374FileEnumer程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_F_QUERY	1		/* 未调用CH374FileQuery程序故禁止以节约代码 */
#define NO_DEFAULT_CH374_RESET		1		/* 未调用CH374Reset程序故禁止以节约代码 */

#include "..\CH374HF6.H"				/* 如果不需要支持FAT32,那么请选用CH374HF4.H */

#ifdef __C51__
#pragma NOAREGS
#endif

/* 在P1.4连接一个LED用于监控演示程序的进度,低电平LED亮,正常工作时亮,按键时灭 */
sbit P1_4  = P1^4;
#define LED_OUT_INIT( )		{ P1_4 = 1; }	/* P1.4 高电平 */
#define LED_OUT_ACT( )		{ P1_4 = 0; }	/* P1.4 低电平驱动LED显示 */
#define LED_OUT_INACT( )	{ P1_4 = 1; }	/* P1.4 低电平驱动LED显示 */
sbit P1_5  = P1^5;
/* 在P1.5连接一个LED用于监控演示程序的进度,低电平LED亮,当U盘插入后亮 */
#define LED_UDISK_IN( )		{ P1_5 = 0; }	/* P1.5 低电平驱动LED显示 */
#define LED_UDISK_OUT( )	{ P1_5 = 1; }	/* P1.5 低电平驱动LED显示 */
sbit P1_6  = P1^6;
/* 在P1.6连接一个LED用于监控演示程序的进度,低电平LED亮,当对U盘写操作时亮 */
#define LED_WR_NOW( )		{ P1_6 = 0; }	/* P1.6 低电平驱动LED显示 */
#define LED_NOT_WR( )		{ P1_6 = 1; }	/* P1.6 低电平驱动LED显示 */
sbit P1_7  = P1^7;
/* 在P1.7连接一个LED用于监控当前USB模式,亮为USB设备方式,灭为USB主机方式 */
#define LED_DEVICE( )		{ P1_7 = 0; }	/* P1.7 低电平驱动LED显示 */
#define LED_HOST( )			{ P1_7 = 1; }	/* P1.7 低电平驱动LED显示 */

/* 公用子程序 */
UINT8	IsKeyPress( void );
void	device( void );
void	host( void );
