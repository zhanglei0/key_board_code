
/* MCS-51单片机C语言的示例程序,用于其它单片机时一般只要修改前面几个接口子程序及硬件定义 */

#pragma NOAREGS
#include <reg52.h>
#include <string.h>
#include <stdio.h>
#include "CH375INC.H"	/* 头文件,在网上下载的CH372或者CH375评估板资料中有 */
unsigned char buf[8];//={0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08};
unsigned char volatile xdata CH375_CMD_PORT _at_ 0xBDF1;		/* CH375命令端口的I/O地址 */
unsigned char volatile xdata CH375_DAT_PORT _at_ 0xBCF0;		/* CH375数据端口的I/O地址 */
sbit CH375_WIRE	=	P3^2;
/* 延时2微秒,不精确 */
void	delay2us( )
{
	unsigned char i;
	for ( i = 2; i != 0; i -- );  /* 根据单片机的时钟选择初值 */
}

/* 基本操作 */

void CH375_WR_CMD_PORT( unsigned char cmd ) {  /* 向CH375的命令端口写入命令,周期不小于4uS,如果单片机较快则延时 */
	delay2us();
	CH375_CMD_PORT=cmd;
	delay2us();
}

void CH375_WR_DAT_PORT( unsigned char dat ) {  /* 向CH375的数据端口写入数据,周期不小于1.5uS,如果单片机较快则延时 */
	CH375_DAT_PORT=dat;
}

unsigned char CH375_RD_DAT_PORT( void ) {  /* 从CH375的数据端口读出数据,周期不小于1.5uS,如果单片机较快则延时 */
	return( CH375_DAT_PORT );
}

/* 延时50毫秒,不精确 */
void	Delay50ms( )
{
	unsigned char i, j;
	for ( i=200; i!=0; i-- ) for ( j=250; j!=0; j-- );
}

/* CH375初始化子程序 */
void	CH375_Init( )
{
	unsigned char i;
/* 测试CH375是否正常工作,可选操作,通常不需要 */
	CH375_WR_CMD_PORT( CMD_CHECK_EXIST );  /* 测试CH375是否正常工作 */
	CH375_WR_DAT_PORT( 0x55 );  /* 写入测试数据 */
	i = ~ 0x55;  /* 返回数据应该是测试数据取反 */
	if ( CH375_RD_DAT_PORT( ) != i ) {  /* CH375不正常 */
		for ( i=80; i!=0; i-- ) {
			CH375_WR_CMD_PORT( CMD_RESET_ALL );  /* 多次重复发命令,执行硬件复位 */
			CH375_RD_DAT_PORT( );
		}
		CH375_WR_CMD_PORT( 0 );
		Delay50ms( );  /* 延时50ms */
	}
/* 设置USB工作模式, 必要操作 */
	CH375_WR_CMD_PORT( CMD_SET_USB_MODE );
	CH375_WR_DAT_PORT( 2 );  /* 设置为使用内置固件的USB设备方式 */
//	for ( i=100; i!=0; i-- ) {  /* 等待操作成功,通常需要等待10uS-20uS */
//		if ( CH375_RD_DAT_PORT( ) == CMD_RET_SUCCESS ) break;
//	}
/*	if ( i==0 ) { CH372/CH375存在硬件错误 }; */
/* 下述启用中断,假定CH375连接在INT0 */
//	IT0 = 0;  /* 置外部信号为低电平触发 */
//	IE0 = 0;  /* 清中断标志 */
//	EX0 = 1;  /* 允许CH375中断 */
}

void	mCh375Interrupt( ) //interrupt 0 using 1
{
	unsigned char InterruptStatus;
	unsigned char i, length;
//	unsigned char data buffer[ 64 ];
	CH375_WR_CMD_PORT( CMD_GET_STATUS );  /* 获取中断状态并取消中断请求 */
	InterruptStatus = CH375_RD_DAT_PORT( );  /* 获取中断状态 */
	switch ( InterruptStatus ) {  /* 分析中断状态处理 */
		case USB_INT_EP2_OUT: {  /* 批量端点下传成功 */
			CH375_WR_CMD_PORT( CMD_RD_USB_DATA );  /* 从当前USB中断的端点缓冲区读取数据块,并释放缓冲区 */
			length = CH375_RD_DAT_PORT( );  /* 首先读取后续数据长度 */
			for ( i = 0; i < length; i ++ ){ 
			buf[ i ] = CH375_RD_DAT_PORT( );  /* 接收数据包 */
			}
			CH375_WR_CMD_PORT( CMD_WR_USB_DATA5 );  /* 从当前USB中断的端点缓冲区读取数据块,并释放缓冲区 */
			 CH375_WR_DAT_PORT(length);  /* 首先读取后续数据长度 */
			for(i=0;i!=length;i++)
			CH375_WR_DAT_PORT( ~buf[i] );//通过中断端点演示上传，一次上传的长度不能超过8字节,这些在你通过批量端点上传的时候是用不到的
			break;
		}
		case USB_INT_EP2_IN: {  /* 批量数据发送成功 */
			CH375_WR_CMD_PORT( CMD_UNLOCK_USB );  /* 释放当前USB缓冲区 */
			break;
			}
		case USB_INT_EP1_IN: {
			CH375_WR_CMD_PORT( CMD_UNLOCK_USB );  /* 释放当前USB缓冲区 */
			break;
			}
		default: {  /* 其它中断,未用到,解锁后退出即可 */
			CH375_WR_CMD_PORT( CMD_UNLOCK_USB );  /* 释放当前USB缓冲区 */
			break;
		}
	}

}
void sent_data(){
	unsigned char i;
	for(i=0;i!=8;i++)
		buf[i]=i;
	CH375_WR_CMD_PORT(CMD_WR_USB_DATA5);//如果是端点2上传的话，只要将CMD_WR_USB_DATA5改为CMD_WR_USB_DATA7就可以了
	CH375_WR_DAT_PORT(8);
	for(i=0;i!=8;i++)
	CH375_WR_DAT_PORT(buf[i]);
}

void	mInitSTDIO( )
{	SCON = 0x50;
	PCON = 0x80;
	TMOD = 0x20;
	TH1 = 0xf3;  /* 24MHz晶振, 9600bps */
	TR1 = 1;
	TI = 1;
}

main( ) {
	unsigned char c;
	Delay50ms( );	/* 延时等待CH375初始化完成,如果单片机由CH375提供复位信号则不必延时 */
	CH375_Init( );  /* 初始化CH375 */
	mInitSTDIO( );
    while(1){	/*用来确定主机是否准备好*/
	    Delay50ms();
	    CH375_WR_CMD_PORT(0x0a);//GET_TOGGLE命令
	    CH375_WR_DAT_PORT(0x20);
    	c=CH375_RD_DAT_PORT();
    	if((c&0x20)==0x20)break;  
	}
	Delay50ms();
	sent_data();/*发送数据*/
	while(1){
 		if(CH375_WIRE==0){
	 	 	mCh375Interrupt();
			sent_data();
		}
	 }
}
