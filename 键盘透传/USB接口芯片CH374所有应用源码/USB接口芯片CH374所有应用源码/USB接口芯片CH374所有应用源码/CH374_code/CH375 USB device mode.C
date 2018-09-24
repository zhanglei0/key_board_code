/*;CH372/CH375 USB device mode & external firmware
; U2(AT89C51) Program
;
; Website:  http://winchiphead.com
; Email:    tech@winchiphead.com
; Author:   W.ch 2003.12, 2005.03
;
;****************************************************************************
/* 单片机和CH375模拟成USB打印机，可以上传设备描述符，配置描述符和设备ID，以及单片机接受打印数据 */
/* 程序示例,C语言,CH375中断为查询方式,只负责数据传输 */
/* MCS-51单片机C语言的示例程序 */
//#pragma NOAREGS
#include <reg52.h>
#include "CH375INC.H"
#include <stdio.h>
#include <string.h>
#define DEF_USB_GET_DEVICE_ID 	0x00   //获取设备ID
#define DEF_USB_GET_PORT_STATUS 0x01 //获取端口状态
#define DEF_USB_SOFT_RESET 		0x02		 //软复位
unsigned char volatile xdata	CH375_CMD_PORT _at_ 0xBDF1;	/* CH375命令端口的I/O地址 */
unsigned char volatile xdata	CH375_DAT_PORT _at_ 0xBCF0;	/* CH375数据端口的I/O地址 */	
sbit	CH375_INT_WIRE	=		P3^2;				//采用查询方式
typedef	union _REQUEST_PACK{
	unsigned char  buffer[8];
	struct{
		unsigned char	 bmReuestType;    	 //标准请求字
		unsigned char	 bRequest;		   	//请求代码
		unsigned int     wValue;			//特性选择高
		unsigned int     wIndx;				//索引
		unsigned int     wLength;				//数据长度
	}r;
}mREQUEST_PACKET,	*mpREQUEST_PACKET;
mREQUEST_PACKET  request;

//typedef	unsigned char BOOL1;  /* typedef	bit	BOOL1; */
//typedef unsigned char	UCHAR;
//typedef unsigned short	USHORT;
unsigned char code  DevDes[]={					//设备描述符
			   0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08,         
               0x48, 0x43, 0x84, 0x55, 0x00, 0x02, 0x00, 0x00,         
               0x00, 0x01
      		   };
unsigned char code ConDes[]={				//配置描述符
			   0x09, 0x02, 0x20, 0x00,  0x01, 0x01, 0x00, 0x80,       
               0x20, 0x09, 0x04, 0x00,  0x00, 0x02, 0x07, 0x01,           
               0x01, 0x00, 0x07, 0x05,  0x82, 0x02, 0x20, 0x00,        
               0x00, 0x07, 0x05, 0x02,  0x02, 0x20, 0x00, 0x00        
			   };
/* 公用缓冲区 */
unsigned char  idata buffer[64];
unsigned char  code buffer_id[]="\x0\x39MFG:HP;CMD:GDI;MDL:HP_LaserJet_5N;CLS:PRINTER;MODE:GDI;";//设备ID
unsigned char mVarSetupRequest;						//	;USB请求码
unsigned int  mVarSetupLength;					//		;后续数据长度
unsigned char *VarSetupDescr;						//	;描述符偏移地址
unsigned char VarUsbAddress;					//						
bit CH375FLAGERR;								//错误清0
bit	CH375CONFLAG;					                     //配置标志

/* 延时100毫秒,不精确 */
void	delay100ms( )
{
	unsigned char	i, j, c;
	for ( i = 200; i != 0; i -- ) for ( j = 200; j != 0; j -- ) c+=3;
}
/* 延时2微秒,不精确 */
void delay1us(){
	;
}

void	delay2us( )
{
	unsigned char i;
#define DELAY_START_VALUE	1  								/* 根据单片机的时钟选择初值,20MHz以下为0,30MHz以上为2 */
	for ( i=DELAY_START_VALUE; i!=0; i-- );
}

/* 基本操作 */
void CH375_WR_CMD_PORT( unsigned char cmd ) { 				 /* 向CH375的命令端口写入命令,周期不小于4uS,如果单片机较快则延时 */
	CH375_CMD_PORT=cmd;
	delay2us( );
}

void CH375_WR_DAT_PORT( unsigned char dat ) { 				 /* 向CH375的数据端口写入数据,周期不小于1.5uS,如果单片机较快则延时 */
	CH375_DAT_PORT=dat;
	delay1us();  											/* 因为MCS51单片机较慢所以实际上无需延时 */
}

unsigned char CH375_RD_DAT_PORT() {  						/* 从CH375的数据端口读出数据,周期不小于1.5uS,如果单片机较快则延时 */
	delay1us( );  										/* 因为MCS51单片机较慢所以实际上无需延时 */
	return( CH375_DAT_PORT );
}

#define TRUE  1
#define FALSE 0
unsigned char set_usb_mode(unsigned char mode){
    unsigned char i;
	 //外部固件模式
	CH375_WR_CMD_PORT( CMD_SET_USB_MODE );
	CH375_WR_DAT_PORT( mode );
	for( i=0; i!=100; i++ ) {  /* 等待设置模式操作完成,不超过30uS */
		if ( CH375_RD_DAT_PORT()==CMD_RET_SUCCESS ) return( TRUE );  /* 成功 */
	}
	return( FALSE );  /* CH375出错,例如芯片型号错或者处于串口方式或者不支持 */
} 

//*********************************************************

//*********************************************************
void mCh375Ep0Up(){
	unsigned char i,len;
	if(mVarSetupLength){												//长度不为0传输具体长度的数据
		if(mVarSetupLength<=8){
			len=mVarSetupLength;
			mVarSetupLength=0;
        }	//长度小于8则传输要求的长度
		else{
			len=8;
			mVarSetupLength-=8;
		}							                        		//长度大于8则传输8个，切总长度减8
	    CH375_WR_CMD_PORT(CMD_WR_USB_DATA3);						//发出写端点0的命令
       	CH375_WR_DAT_PORT(len);										//写入长度
    	for(i=0;i!=len;i++)
        CH375_WR_DAT_PORT(request.buffer[i]);	              		//循环写入数据
    }
	else{
		CH375_WR_CMD_PORT(CMD_WR_USB_DATA3);						//发出写端点0的命令
		CH375_WR_DAT_PORT(0);					                   //上传0长度数据，这是一个状态阶段
	}
}

//*********************************************************

//复制描述符以便上传
void mCh375DesUp(){
	unsigned char k;      
	for (k=0; k!=8; k++ ) {
         request.buffer[k]=*VarSetupDescr;  								//依次复制8个描述符，
         VarSetupDescr++;
    }
}

/* CH375中断服务程序INT0,使用寄存器组1 */
void	mCH375Interrupt( ) 
{
	unsigned char InterruptStatus;
	unsigned char length, c1,len;
	CH375_WR_CMD_PORT(CMD_GET_STATUS);  									/* 获取中断状态并取消中断请求 */
	InterruptStatus =CH375_RD_DAT_PORT();  									/* 获取中断状态 */																/* 清中断标志,对应于INT0中断 */
	switch(InterruptStatus){  // 分析中断状态
		case  USB_INT_EP2_OUT:  											// 批量端点下传成功 
			CH375_WR_CMD_PORT(CMD_RD_USB_DATA);									//发出读数据命令
			length=CH375_RD_DAT_PORT();											//首先读出的是长度														
			for(len=0;len!=length;len++)
				 buffer[len]=CH375_RD_DAT_PORT();	//将数据读入到缓冲区，如果接打印机只要将接收到buffer的数据发送给打印机就可以打印
			for(len=0;len!=length;len++)
				printf("%02x",(unsigned short)buffer[len]);
			break;
		case   USB_INT_EP2_IN:										 //批量端点上传成功,未处理
			CH375_WR_CMD_PORT (CMD_UNLOCK_USB);								//释放缓冲区
			break;
		case   USB_INT_EP1_IN:	 											//中断端点上传成功，未处理
			CH375_WR_CMD_PORT (CMD_UNLOCK_USB);								//释放缓冲区
			break;
		case   USB_INT_EP1_OUT:	  											//中断端点下传成功，未处理
			CH375_WR_CMD_PORT (CMD_UNLOCK_USB);								//释放缓冲区
			break;
		case   USB_INT_EP0_SETUP: 											//控制端点建立成功
	    	CH375_WR_CMD_PORT(CMD_RD_USB_DATA);
			length=CH375_RD_DAT_PORT();
			for(len=0;len!=length;len++)request.buffer[len]=CH375_RD_DAT_PORT();  // 取出数据
			if(length==0x08){
			    if(request.buffer[7]) mVarSetupLength=127;
			     else  mVarSetupLength=request.buffer[6];							//控制传输数据长度,如果request.buffer[7]的数据大于0则长度为127，否则就是位6的长度，
				if((request.r.bmReuestType)&0x40){         					 //厂商请求，未处理
				}
			else   if((request.r.bmReuestType)&0x20){ 					//打印机请求
			    	mVarSetupRequest=request.r.bRequest|0x80;
			    	switch(request.r.bRequest){ 
			    	   case DEF_USB_GET_DEVICE_ID:							//获取设备ID
						   mVarSetupLength=(int)(buffer_id[0]<<8|buffer_id[1]);  //ID的长度，为buffer_id前2位
						   if(mVarSetupLength<=8){							//如果没有传ID则返回2个00
						   mVarSetupLength=2;
						   }
  						   VarSetupDescr=buffer_id;							
							mCh375DesUp();									//ID上传
					       break;
					   case DEF_USB_GET_PORT_STATUS:						//未处理，只是给了一个状态
					  //     s=get_port_status( );
						   mVarSetupLength=1;
						   VarSetupDescr= 0x18; 							// buffer_status;
						   mCh375DesUp();
					       break;
					   case DEF_USB_SOFT_RESET:								//未处理
				      // 	   soft_reset_print();
			 		       break;
						default :
							CH375FLAGERR=1;									//不支持的标准请求
							break;
					}
				}
			else  if(!((request.r.bmReuestType)&0x60)){          				//标准请求
					mVarSetupRequest=request.r.bRequest;							//暂存标准请求码
					switch(request.r.bRequest){  // 分析标准请求
						case DEF_USB_CLR_FEATURE:									//清除特性
							if((request.r.bmReuestType&0x1F)==0X02){					//不是端点不支持
								switch(request.buffer[4]){
									case 0x82:
										CH375_WR_CMD_PORT(CMD_SET_ENDP7);					//清除端点2上传
										CH375_WR_DAT_PORT(0x8E);                			//发命令清除端点
										break;
									case 0x02:
										CH375_WR_CMD_PORT(CMD_SET_ENDP6);
										CH375_WR_DAT_PORT(0x80);							//清除端点2下传
										break;
									case 0x81:
										CH375_WR_CMD_PORT(CMD_SET_ENDP5);					//清除端点1上传
										CH375_WR_DAT_PORT(0x8E);
										break;
									case 0x01:
										CH375_WR_CMD_PORT(CMD_SET_ENDP4);					//清除端点1下传
										CH375_WR_DAT_PORT(0x80);
										break;
									default:
										break;
								}
							}
							else{
								CH375FLAGERR=1;								//不支持的清除特性，置错误标志
							}
							break;
						case DEF_USB_GET_STATUS:								//获得状态
							request.buffer[0]=0;
							request.buffer[1]=0;								//上传状态
							break;
						case DEF_USB_SET_ADDRESS:								//设置地址
							VarUsbAddress=request.buffer[2];					//暂存USB主机发来的地址
//							set_addr(VarUsbAddress);
							break;
						case DEF_USB_GET_DESCR: 								//获得描述符
							if(request.buffer[3]==1)							//设备描述符上传
								VarSetupDescr=DevDes;
							else if(request.buffer[3]==2)		 					//配置描述符上传
								VarSetupDescr=ConDes;
							mCh375DesUp();											//其余描述符不支持					          							
							break;
						case DEF_USB_GET_CONFIG:									//获得配置
							request.buffer[0]=0;									//没有配置则传0
							if(CH375CONFLAG) request.buffer[0]=1;									//已经配置则传1；这是在描述符里规定的
							break;
						case DEF_USB_SET_CONFIG:                 					//设置配置
							CH375CONFLAG=0;
//							CH375ACT=1;
							if ( request.buffer[2] != 0 ) {
								CH375CONFLAG=1;											//设置配置标志
//								CH375ACT=0;												//输出配置完成信号
							}
							break;
						case DEF_USB_GET_INTERF:										//得到接口
							request.buffer[0]=1;									//上传接口数，本事例只支持一个接口
							break;
						default :
							CH375FLAGERR=1;										//不支持的标准请求
							break;
					}
				}
			}
			else {  //不支持的控制传输，不是8字节的控制传输
				CH375FLAGERR=1;
			}
			if(!CH375FLAGERR) mCh375Ep0Up();										//没有错误/调用数据上传，，长度为0上传为状态
			else {
				CH375_WR_CMD_PORT(CMD_SET_ENDP3);								//设置端点1为STALL，指示一个错误
				CH375_WR_DAT_PORT(0x0F);
			}
			break;
		case   USB_INT_EP0_IN:													//控制端点上传成功
			if (mVarSetupRequest==DEF_USB_GET_DESCR){								//描述符上传
				mCh375DesUp();												
				mCh375Ep0Up();
				}
		   else	if (mVarSetupRequest==DEF_USB_SET_ADDRESS){							//设置地址
				CH375_WR_CMD_PORT(CMD_SET_USB_ADDR);
				CH375_WR_DAT_PORT(VarUsbAddress);								//设置USB地址,设置下次事务的USB地址		
				}
		   else	if (mVarSetupRequest==(DEF_USB_GET_DEVICE_ID|0x80)){			//0x80是为了区分标准请求和打印的类请求
				mCh375DesUp();												
				mCh375Ep0Up();
				} 
			CH375_WR_CMD_PORT (CMD_UNLOCK_USB);							//释放缓冲区
			break;
		case   USB_INT_EP0_OUT:													//控制端点下传成功
			CH375_WR_CMD_PORT(CMD_RD_USB_DATA);								//发出读数据命令
			if(length=CH375_RD_DAT_PORT()){										//长度为0跳出
				for(len=0;len!=length;len++)c1=CH375_RD_DAT_PORT();					//取出下传数据
			}
			break;
		default:
			if((InterruptStatus&0x03)==0x03){									//总线复位
				CH375FLAGERR=0;													//错误清0
				CH375CONFLAG=0;													//配置清0
				mVarSetupLength=0;
//				CH375ACT=1;														//清配置完成输出
			}
			else{																//命令不支持
				;
			}
			CH375_WR_CMD_PORT (CMD_UNLOCK_USB);									//释放缓冲区
			CH375_RD_DAT_PORT();
			break;
	}
}
/* 为printf和getkey输入输出初始化串口 */
void	mInitSTDIO( )
{
		PCON = 0x80;
		SCON = 0x50;  /* 8位串行数据 */
		TL2 = RCAP2L = 0 - 13                                                                                                                                                                                                                                                                                ; 
		TH2 = RCAP2H = 0xFF;//57600
		T2CON = 0x34;  /* 定时器2用于串口的波特率发生器 */
		RI = 0; 
		TI = 1;  /* 清除中断标志 */
}

main( ) {
	mInitSTDIO( );
/* 如果单片机对CH375的INT#引脚采用中断方式而不是查询方式,那么应该在复制USB设备期间禁止CH375中断,在USB设备复位完成后清除CH375中断标志再允许中断 */	 	
	delay100ms(); delay100ms();  /* 有些USB设备要等待数百毫秒才能正常工作 */
	set_usb_mode(1);
    while(1){		/* 主程序 */
    	if (CH375_INT_WIRE==0) mCH375Interrupt( );
		}
}
