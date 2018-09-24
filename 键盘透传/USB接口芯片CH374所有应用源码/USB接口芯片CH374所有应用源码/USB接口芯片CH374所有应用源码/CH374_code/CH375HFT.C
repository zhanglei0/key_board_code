/* 2004.06.05
****************************************
**  Copyright  (C)  W.ch  1999-2004   **
**  Web:  http://www.winchiphead.com  **
****************************************
**  USB Host File Interface for CH375 **
**  TC2.0@PC, KC7.0@MCS51             **
****************************************
*/
/* CH375 主机文件系统接口 */
/* 支持: FAT12/FAT16/FAT32 */

/* MCS-51单片机C语言的U盘文件读写示例程序 */
/* 用于以下情况的MCS51单片机
   1. 高速MCS51单片机,机器周期小于0.3uS,或者在机器周期为12个时钟时的时钟频率大于40MHz
   2. 非总线MCS51单片机,用普通I/O引脚模拟8位并行总线读写,与CH375之间采用并口连接
   3. 单片机与CH375之间采用串口连接
*/
/* 本程序用于演示将ADC模数采集的数据保存到U盘文件MY_ADC.TXT中 */
/* CH375的INT#引脚采用查询方式处理,本例用普通I/O引脚模拟8位并行总线读写,同时提供串口连接示例,
   以字节为单位读写U盘文件,读写速度较扇区模式慢,但是由于字节模式读写文件不需要文件数据缓冲区FILE_DATA_BUF,
   所以总共只需要600字节的RAM,适用于单片机硬件资源有限、数据量小并且读写速度要求不高的系统 */


/* C51   CH375HFT.C */
/* LX51  CH375HFT.OBJ , CH375HF5.LIB */
/* OHX51 CH375HFT */

#include <reg52.h>
#include <stdio.h>

#define	MAX_BYTE_IO				29		/* 以字节为单位单次读写文件时的最大长度,默认值是29,值大则占用内存多,值小则超过该长度必须分多次读写 */

/* 以下定义的详细说明请看CH375HF5.H文件 */
#define LIB_CFG_FILE_IO			1		/* 文件读写的数据的复制方式,0为"外部子程序",1为"内部复制" */
#define LIB_CFG_INT_EN			0		/* CH375的INT#引脚连接方式,0为"查询方式",1为"中断方式" */
/*#define LIB_CFG_UPD_SIZE		1*/		/* 在添加数据后是否自动更新文件长度: 0为"不更新",1为"自动更新" */
/* 默认情况下,如果扇区数/字节数不为0那么CH375FileWrite/CH375ByteWrite只负责写入数据而不修改文件长度,
   如果需要每次写完数据后会自动修改/更新文件长度,那么可以使全局变量CH375LibConfig的位4为1,
   如果长时间不写入数据则应该更新文件长度,防止突然断电后前面写入的数据与文件长度不相符,
   如果确定不会突然断电或者后面很快有数据不断写入则不必更新文件长度,可以提高速度并减少U盘损耗(U盘内部的内存寿命有限,不宜频繁改写) */
unsigned char volatile xdata	CH375_CMD_PORT _at_ 0xAFF1;	/* CH375命令端口的I/O地址 */
unsigned char volatile xdata	CH375_DAT_PORT _at_ 0xADF0;	/* CH375数据端口的I/O地址 */
unsigned char volatile xdata	CH375_CMD_PORT_U0 _at_ 0x9FF1;	/* CH375命令端口的I/O地址 */
unsigned char volatile xdata	CH375_DAT_PORT_U0 _at_ 0x9DF0;	/* CH375数据端口的I/O地址 */
unsigned char Change_Bit;
unsigned char xdata	copy_buf[512],copy_buf_u0[512];
unsigned char xdata copy_mCmdParam[100],copy_mCmdParam_u0[100];
unsigned char status,status_u0;
bit interrupt_bit,interrupt_bit_u0;
/* 只使用单片机内置的1KB外部RAM: 0000H-01FFH 为磁盘读写缓冲区, 以字节为单位读写文件不需要文件数据读写缓冲区FILE_DATA_BUF */
#define	DISK_BASE_BUF_ADDR		0x0000	/* 外部RAM的磁盘数据缓冲区的起始地址,从该单元开始的缓冲区长度为SECTOR_SIZE */
#define FILE_DATA_BUF_ADDR		0x0200	/* 外部RAM的文件数据缓冲区的起始地址,缓冲区长度不小于一次读写的数据长度,字节模式不用该缓冲区 */
/* 由于单片机内置的外部RAM只有1KB, 有些单片机还要去掉256字节内部RAM, 只剩下768字节的外部RAM, 其中前512字节由CH375子程序用于磁盘数据缓冲 */
#define FILE_DATA_BUF_LEN		0x4000	/* 外部RAM的文件数据缓冲区,缓冲区长度不小于一次读写的数据长度,字节模式不用该缓冲区 */

sbit CH375_INT_WIRE		=	P1^1;	/* P1.1, INT0, CH375的中断线INT#引脚,连接CH375的INT#引脚,用于查询中断状态 */
sbit CH375_INT_WIRE_U0	=	P1^2;	/* P1.1, INT0, CH375的中断线INT#引脚,连接CH375的INT#引脚,用于查询中断状态 */

#define NO_DEFAULT_CH375_F_ENUM		1		/* 未调用CH375FileEnumer程序故禁止以节约代码 */
#define NO_DEFAULT_CH375_F_QUERY	1		/* 未调用CH375FileQuery程序故禁止以节约代码 */

#include "CH375HFC.H"

/* 本例用I/O引脚模拟产生CH375的并口读写时序 */
/* 本例中的硬件连接方式如下(实际应用电路可以参照修改下述3个并口读写子程序) */
/* 单片机的引脚    CH375芯片的引脚
      P3.2                 INT#
      P1.0                 A0
      P1.1                 CS#    如果模拟出的并口上只有CH375,那么CS#可以直接接低电平,强制片选
      P1.2                 WR#
      P1.3                 RD#
      P0(8位端口)         D7-D0       */
//void mDelay1_2uS( )  /* 至少延时1.2uS,根据单片机主频调整 */
//{
//	return;
//}

void CH375_PORT_INIT( )  /* 由于使用通用I/O模块并口读写时序,所以进行初始化 */
{
	P0 = 0xFF;  /* 并口输入 */
}

void xWriteCH375Cmd( UINT8 mCmd )		/* 外部定义的被CH375程序库调用的子程序,向CH375写命令 */
{
//	mDelay1_2uS( );  /* 至少延时1uS */
	if(Change_Bit==0x01)
		CH375_CMD_PORT=mCmd;
	else if(Change_Bit==0x02)
		CH375_CMD_PORT_U0=mCmd;
//	mDelay1_2uS( );  /* 至少延时2uS */
}

void xWriteCH375Data( UINT8 mData )		/* 外部定义的被CH375程序库调用的子程序,向CH375写数据 */
{
	if(	Change_Bit==0x01)
		CH375_DAT_PORT=mData;
	else if(Change_Bit==0x02)
		CH375_DAT_PORT_U0=mData;		
//	mDelay1_2uS( );  /* 至少延时1.2uS */
}

UINT8 xReadCH375Data( void )			/* 外部定义的被CH375程序库调用的子程序,从CH375读数据 */
{
	UINT8	mData;
//	mDelay1_2uS( );  /* 至少延时1.2uS */
	if(	Change_Bit==0x01)
		mData=CH375_DAT_PORT;
	if(	Change_Bit==0x02)
		mData=CH375_DAT_PORT_U0;
	return( mData );
}

/* 延时100毫秒,不精确 */
void	mDelay100mS( )
{
	UINT8	i, j, c;
	for ( i = 200; i != 0; i -- ) for ( j = 200; j != 0; j -- ) c+=3;
}

UINT8	mCopyCodeStringToXRAM( UINT8 xdata *iDestination, UINT8 code *iSource )
{
	UINT8	i = 0;
	while ( *iDestination = *iSource ) {
		iDestination ++;
		iSource ++;
		i ++;
	}
	return( i );
}

void mCopy_Data_To_Data(unsigned char xdata *data_b0,unsigned char xdata *data_b1,unsigned short len)
{
	unsigned short	i;
	for(i=0;i!=len;i++){
		*data_b0=*data_b1;
		data_b0++;
		data_b1++;
		}
}
/* 检查操作状态,如果错误则显示错误代码并停机 */
void	mStopIfError( UINT8 iError )
{
	if ( iError == ERR_SUCCESS ) return;  /* 操作成功 */
	printf( "Error: %02X\n", (UINT16)iError );  /* 显示错误 */
	while ( 1 ) {
		mDelay100mS( );
		mDelay100mS( );
	}
}

/* 为printf和getkey输入输出初始化串口 */
void	mInitSTDIO( )
{
	SCON = 0x50;
	PCON = 0x80;
	TMOD = 0x20;
	TH1 = 0xfe;  /* 24MHz晶振, 9600bps */
	TR1 = 1;
	TI = 1;
}

void wire_interrupt( )/*检测设备插拔*/
{
	if(CH375_INT_WIRE==0){
		xWriteCH375Cmd(CMD_GET_STATUS);
		status=xReadCH375Data();
//		printf("status=%02x\n",(unsigned short)status);
	}
		
}

void wire_interrupt_u0( )/*检测设备插拔*/
{
	if(CH375_INT_WIRE_U0==0){
		xWriteCH375Cmd(CMD_GET_STATUS);
		status_u0=xReadCH375Data();
//		printf("status_u0=%02x\n",(unsigned short)status_u0);
	}

}

main( ) {
	UINT8	i;
	UINT32	TotalLen,NewSize;
	CH375_PORT_INIT( );  /* 由于使用通用I/O模块并口读写时序,所以进行初始化 */
	mDelay100mS( );  /* 延时100毫秒 */
	mInitSTDIO( );  /* 为了让计算机通过串口监控演示过程 */
	printf( "Start\n" );
	Change_Bit=0x01;
	i = CH375LibInit( );  /* 初始化CH375程序库和CH375芯片,操作成功返回0 */
	mStopIfError( i );
	Change_Bit=0x02;
	i = CH375LibInit( );  /* 初始化CH375程序库和CH375芯片,操作成功返回0 */
	mStopIfError( i );
/* 其它电路初始化 */

	while ( 1 ) {
//		printf( "Wait Udisk\n" );
		while(1){
			if(interrupt_bit==0){
		 		 Change_Bit=0x01;
				 wire_interrupt( );  /* 查询CH375中断并更新中断状态,等待U盘插入 */
				 if(status==0x15)interrupt_bit=1;
			 }
			 if(interrupt_bit_u0==0){
				Change_Bit=0x02;
				wire_interrupt_u0( );
				if ( status_u0 == 0x15 )interrupt_bit_u0=1;   /* 查询CH375中断并更新中断状态,等待U盘插入 */
				}
			if((interrupt_bit==1)&(interrupt_bit_u0==1))break;
		}
		interrupt_bit=0;
		interrupt_bit_u0=0;
		mDelay100mS( );  /* 延时,可选操作,有的USB存储器需要几十毫秒的延时 */
		mDelay100mS( );

/* 检查U盘是否准备好,有些U盘不需要这一步,但是某些U盘必须要执行这一步才能工作 */
		Change_Bit=0x01;
		for ( i = 0; i < 5; i ++ ) {  /* 有的U盘总是返回未准备好,不过可以被忽略 */
			mDelay100mS( );
//			printf( "Ready ?\n" );
			if ( CH375DiskReady( ) == ERR_SUCCESS ){
			mCmdParam.SaveVariable.mSaveVariable=1;			//为0时恢复变量，为1时保存变量
			mCmdParam.SaveVariable.mBuffer=copy_mCmdParam;	//指向要保存的缓冲区
//			mCopy_Data_To_Data(copy_buf,DISK_BASE_BUF,0x200);
			CH375SaveVariable( );		 
			break;  /* 查询磁盘是否准备好 */
			}
		}
		Change_Bit=0x02;
		for ( i = 0; i < 5; i ++ ) {  /* 有的U盘总是返回未准备好,不过可以被忽略 */
			mDelay100mS( );
//			printf( "Ready1 ?\n" );
			if ( CH375DiskReady( ) == ERR_SUCCESS )
			{	
			mCmdParam.SaveVariable.mSaveVariable=1;			//为0时恢复变量，为1时保存变量
			mCmdParam.SaveVariable.mBuffer=copy_mCmdParam_u0;	//指向要保存的缓冲区
//			mCopy_Data_To_Data(copy_buf_u0,DISK_BASE_BUF,0x200);
			CH375SaveVariable( );		 
			 break;  /* 查询磁盘是否准备好 */			
			}
		}
		Change_Bit=0x01;
		mCmdParam.SaveVariable.mSaveVariable=0;			//为0时恢复变量，为1时保存变量
		mCmdParam.SaveVariable.mBuffer=copy_mCmdParam;	//指向要保存的缓冲区
//		mCopy_Data_To_Data(DISK_BASE_BUF,copy_buf,0x200);
		CH375SaveVariable( );
		CH375DirtyBuffer(); 	
//		printf( "Open\n" );
		mCopyCodeStringToXRAM( mCmdParam.Open.mPathName, "\\12.TXT" );  /* 文件名,该文件在根目录下 */
		i = CH375FileOpen( );  /* 打开文件 */
		if ( i == ERR_SUCCESS ) {  /* 文件存在并且已经被打开,移动文件指针到尾部以便添加数据 */
			mCmdParam.SaveVariable.mSaveVariable=1;			//为0时恢复变量，为1时保存变量
			mCmdParam.SaveVariable.mBuffer=copy_mCmdParam;	//指向要保存的缓冲区
//			mCopy_Data_To_Data(copy_buf,DISK_BASE_BUF,512);
			CH375SaveVariable( );
			NewSize=CH375vFileSize;
			TotalLen=(CH375vFileSize+511)>>9;
			Change_Bit=0x02;
			mCmdParam.SaveVariable.mSaveVariable=0;			//为0时恢复变量，为1时保存变量
			mCmdParam.SaveVariable.mBuffer=copy_mCmdParam_u0;	//指向要保存的缓冲区
//			mCopy_Data_To_Data(DISK_BASE_BUF,copy_buf_u0,0x200);
			CH375SaveVariable( );	
	 		CH375DirtyBuffer();
		mCopyCodeStringToXRAM( mCmdParam.Open.mPathName, "\\345.TXT" );  /* 文件名,该文件在根目录下 */
			i = CH375FileCreate( );  /* 新建文件并打开,如果文件已经存在则先删除后再新建 */
			mStopIfError( i );
			mCmdParam.SaveVariable.mSaveVariable=1;			//为0时恢复变量，为1时保存变量
			mCmdParam.SaveVariable.mBuffer=copy_mCmdParam_u0;	//指向要保存的缓冲区
//			mCopy_Data_To_Data(copy_buf_u0,DISK_BASE_BUF,0x200);
			CH375SaveVariable();
			printf("write\n");
		while(1){
				printf("TotalLen=%02x\n",(unsigned short)TotalLen);
				Change_Bit=0x01;//设备1
				mCmdParam.SaveVariable.mSaveVariable=0;			//为0时恢复变量，为1时保存变量
				mCmdParam.SaveVariable.mBuffer=copy_mCmdParam;	//指向要保存的缓冲区
//				mCopy_Data_To_Data(DISK_BASE_BUF,copy_buf,512);
				CH375SaveVariable();
				CH375DirtyBuffer();
/*				printf("a:\n");
				for(i=0;i!=100;i++)
					printf("%02x ",(unsigned short)copy_mCmdParam[i]);
				printf("\n");*/
				mCmdParam.Read.mSectorCount = 32;  /* 读取全部数据,如果超过60个扇区则只读取60个扇区 */
				CH375vFileSize += 511;  /* 默认情况下,以扇区方式读取数据时,无法读出文件尾部不足1个扇区的部分,所以必须临时加大文件长度以读取尾部零头 */
				i = CH375FileRead( );  /* 从文件读取数据 */
				mStopIfError( i );
				CH375vFileSize -= 511;  /* 恢复原文件长度 */
				mCmdParam.SaveVariable.mSaveVariable=1;			//为0时恢复变量，为1时保存变量
				mCmdParam.SaveVariable.mBuffer=copy_mCmdParam;	//指向要保存的缓冲区
//				mCopy_Data_To_Data(copy_buf,DISK_BASE_BUF,0x200);
				CH375SaveVariable();
/*				printf("b:\n");
				for(i=0;i!=100;i++)
					printf("%02x ",(unsigned short)copy_mCmdParam[i]);
				printf("\n");*/
				Change_Bit=0x02;	//设备2
				mCmdParam.SaveVariable.mSaveVariable=0;			//为0时恢复变量，为1时保存变量
				mCmdParam.SaveVariable.mBuffer=copy_mCmdParam_u0;	//指向要保存的缓冲区
//				mCopy_Data_To_Data(DISK_BASE_BUF,copy_buf_u0,0x200);
				CH375SaveVariable();
				CH375DirtyBuffer();
				mCmdParam.Write.mSectorCount = 32;  /* 写入所有扇区的数据 */
			i = CH375FileWrite( );  /* 向文件写入数据 */
				mCmdParam.SaveVariable.mSaveVariable=1;			//为0时恢复变量，为1时保存变量
				mCmdParam.SaveVariable.mBuffer=copy_mCmdParam_u0;	//指向要保存的缓冲区
//				mCopy_Data_To_Data(copy_buf_u0,DISK_BASE_BUF,0x200);
				CH375SaveVariable();
				if(TotalLen<32)break;
			TotalLen-=32;
			}
				Change_Bit=0x02;		
				mCmdParam.SaveVariable.mSaveVariable=0;			//为0时恢复变量，为1时保存变量
				mCmdParam.SaveVariable.mBuffer=copy_mCmdParam_u0;	//指向要保存的缓冲区
//				mCopy_Data_To_Data(DISK_BASE_BUF,copy_buf_u0,0x200);
				CH375SaveVariable();
				CH375DirtyBuffer();
				mCmdParam.Modify.mFileAttr = 0xff;  /* 输入参数: 新的文件属性,为0FFH则不修改 */
				mCmdParam.Modify.mFileTime = 0xffff;  /* 输入参数: 新的文件时间,为0FFFFH则不修改,使用新建文件产生的默认时间 */
				mCmdParam.Modify.mFileDate = MAKE_FILE_DATE( 2004, 5, 18 );  /* 输入参数: 新的文件日期: 2004.05.18 */
				mCmdParam.Modify.mFileSize = NewSize;  /* 输入参数: 如果原文件较小,那么新的文件长度与原文件一样长,否则被RAM所限,如果文件长度大于64KB,那么NewSize必须为UINT32 */
				i = CH375FileModify( );  /* 修改当前文件的信息,修改日期和长度 */
				mStopIfError( i );
				mCmdParam.Close.mUpdateLen = 0;  /* 不要自动计算文件长度,如果自动计算,那么该长度总是512的倍数 */
				i = CH375FileClose( );
				printf("close\n");
				mStopIfError( i );
				Change_Bit=0x01;
				mCmdParam.SaveVariable.mSaveVariable=0;			//为0时恢复变量，为1时保存变量
				mCmdParam.SaveVariable.mBuffer=copy_mCmdParam;	//指向要保存的缓冲区
//				mCopy_Data_To_Data(DISK_BASE_BUF,copy_buf,0x200);
				CH375SaveVariable();
				CH375DirtyBuffer();
				mCmdParam.Close.mUpdateLen = 0;  /* 不要自动计算文件长度,如果自动计算,那么该长度总是512的倍数 */
				i = CH375FileClose( );
				mStopIfError( i );
		}
		while(1){
			if(interrupt_bit==0){
				 Change_Bit=0x01;
				 wire_interrupt( );  /* 查询CH375中断并更新中断状态,等待U盘插入 */
				 if(status==0x16)interrupt_bit=1;
			 }
			 if(interrupt_bit_u0==0){
				Change_Bit=0x02;
				wire_interrupt_u0( );
				if ( status_u0 == 0x16 )interrupt_bit_u0=1;   /* 查询CH375中断并更新中断状态,等待U盘插入 */
				}
			if((interrupt_bit==1)||(interrupt_bit_u0==1)){
				if(interrupt_bit==1)
					interrupt_bit=0;
				if(interrupt_bit_u0==1)
					interrupt_bit_u0=0;
				break;
				}
		}
		printf("again\n");
//		while ( CH375DiskStatus != DISK_DISCONNECT ) xQueryInterrupt( );  /* 查询CH375中断并更新中断状态,等待U盘拔出 */
		mDelay100mS( );
		mDelay100mS( );
	}
}
