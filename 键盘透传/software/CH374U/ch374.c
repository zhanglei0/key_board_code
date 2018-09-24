
#include <rthw.h>
#include "port.h"


#define GET_STATUS              0
#define CLEAR_FEATURE		1
#define GET_STATE               2
#define SET_FEATURE             3
#define GET_DESCRIPTOR		6
#define SET_DESCRIPTOR		7
// 获取设备描述符
const	UINT8	SetupGetDevDescr[] = { 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00 };
// 获取配置描述符
const	UINT8	SetupGetCfgDescr[] = { 0x80, 0x06, 0x00, 0x02, 0x00, 0x00, 0x04, 0x00 };
// 设置USB地址
const	UINT8	SetupSetUsbAddr[] = { 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
// 设置USB配置
const	UINT8	SetupSetUsbConfig[] = { 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* SET IDLE */
const unsigned char  SetupSetidle[]={0x21,0x0a,0x00,0x00,0x00,0x00,0x00,0x00};        
/* 获取HID 报告描述符 */
unsigned char  SetupGetHidDes[]={0x81,0x06,0x00,0x22,0x00,0x00,0x81,0x00};    
/* SET REPORT */
unsigned char  SetupSetReport[]={0x21,0x09,0x00,0x02,0x00,0x00,0x01,0x00};     

UINT8	UsbDevEndpSize = DEFAULT_ENDP0_SIZE;	/* USB设备的端点0的最大包尺寸 */

UINT8	FlagDeviceStatus;						/* 当前USB设备状态，通常用于中断方式的全局变量，本程序中未使用 */


UINT8	GetDeviceDescr( PUINT8 buf );  // 获取设备描述符

UINT8	GetConfigDescr( PUINT8 buf );  // 获取配置描述符

UINT8	SetUsbAddress( UINT8 addr );  // 设置USB设备地址

UINT8	SetUsbConfig( UINT8 cfg );  // 设置USB设备配置




UINT8    LOW_SPEED_BIT;
UINT8	 tog1;              //读取数据时的同步标志
UINT8    endp_out_addr;	    // out端点地址,由于一般鼠标键盘不支持out端点,一般用不到 
UINT8    endp_in_addr;		// in 端点地址 
UINT8    hid_des_leng;      // HID类报告描述符的长度
UINT8    endp_num;          // 数据 hid 类键盘、鼠标的端点数目
UINT8	 FlagDeviceStatus;	// 当前USB设备状态，通常用于中断方式的全局变量，本程序中未使用 


UINT8	NewDevCount;
_RootHubDev RootHubDev[3];
_DevOnHubPort DevOnHubPort[3][4];
UINT8	TempBuf[64];
UINT8	CtrlBuf[8];

/* 延时指定微秒时间,根据单片机主频调整,不精确 */
void	mDelayuS( UINT8 us )
{
  for(uint8_t i = 0; i < 36;i++) {
    for(uint8_t j = 0; j < us;j++)
    {}
  }
}

/* 延时指定毫秒时间 */
void	mDelaymS( UINT8 ms )
{
	Delay(ms);
}

// 查询CH374中断(INT#低电平)
BOOL	Query374Interrupt( void )
{
  return( CH374_INT_WIRE ? FALSE : TRUE );  /* 如果连接了CH374的中断引脚则直接查询中断引脚 */
}
// 等待CH374中断(INT#低电平)，超时则返回ERR_USB_UNKNOWN
UINT8	Wait374Interrupt( void )
{
	UINT16	i;
	for ( i = 0; i < 10000; i ++ ) {  // 计数防止超时
		if ( Query374Interrupt( ) ) return( 0 );
	}
	return( ERR_USB_UNKNOWN );  // 不应该发生的情况
}

// CH374传输事务，输入目的端点地址/PID令牌/同步标志，返回同CH375，NAK不重试，超时/出错重试
// CH374传输事务，输入目的端点地址/PID令牌/同步标志，返回同CH375，NAK不重试，超时/出错重试
UINT8	HostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog )
{  // 本子程序着重于易理解,而在实际应用中,为了提供运行速度,应该对本子程序代码进行优化
  UINT8	retry;
  UINT8	s, r;
  for ( retry = 0; retry < 3; retry ++ ) {
    Write374Byte( REG_USB_H_PID, M_MK_HOST_PID_ENDP( pid, endp_addr ) );  // 指定令牌PID和目的端点号
    Write374Byte( REG_USB_H_CTRL, ( tog ? ( BIT_HOST_START | BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : BIT_HOST_START ) );  // 设置同步标志并启动传输
    s = Wait374Interrupt( );
    if ( s == ERR_USB_UNKNOWN ) return( s );  // 中断超时,可能是硬件异常
    s = Read374Byte( REG_INTER_FLAG );  // 获取中断状态
    if ( s & BIT_IF_DEV_DETECT ) {  // USB设备插拔事件
      AnalyzeRootHub( );   // 分析ROOT-HUB状态
      Write374Byte( REG_INTER_FLAG, BIT_IF_DEV_DETECT );  // 清中断标志
      s = Read374Byte( REG_INTER_FLAG );  // 获取中断状态
      if ( ( s & BIT_IF_DEV_ATTACH ) == 0x00 ) return( USB_INT_DISCONNECT ); 
      // USB设备断开事件
    }
    if ( s & BIT_IF_TRANSFER ) {
      // 传输完成
      Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_TRANSFER );  // 清中断标志
      s = Read374Byte( REG_USB_STATUS );  // USB状态
      r = s & BIT_STAT_DEV_RESP;  // USB设备应答状态
      switch ( pid ) {
        case DEF_USB_PID_SETUP:
        case DEF_USB_PID_OUT:
          if ( r == DEF_USB_PID_ACK ) return( USB_INT_SUCCESS );
          else if ( r == DEF_USB_PID_STALL || r == DEF_USB_PID_NAK ) return( r | 0x20 );
          else if ( ! M_IS_HOST_TIMEOUT( s ) ) return( r | 0x20 );  // 不是超时/出错，意外应答
          break;
        case DEF_USB_PID_IN:
          if ( M_IS_HOST_IN_DATA( s ) ) {  // DEF_USB_PID_DATA0 or DEF_USB_PID_DATA1
                  if ( s & BIT_STAT_TOG_MATCH ) return( USB_INT_SUCCESS );  // 不同步则需丢弃后重试
          }
          else if ( r == DEF_USB_PID_STALL || r == DEF_USB_PID_NAK ) return( r | 0x20 );
          else if ( ! M_IS_HOST_TIMEOUT( s ) ) return( r | 0x20 );  // 不是超时/出错，意外应答
          break;
        default:
          return( ERR_USB_UNKNOWN );  // 不可能的情况
          break;
      }
    }
    else {  // 其它中断,不应该发生的情况
      mDelayuS( 200 );  // 等待传输完成
      Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  /* 清中断标志 */
      if ( retry ) return( ERR_USB_UNKNOWN );  /* 不是第一次检测到则返回错误 */
    }
  }
  return( 0x20 );  // 应答超时
}

// CH374传输事务，输入目的端点地址/PID令牌/同步标志/以mS为单位的NAK重试总时间(0xFFFF无限重试)，返回同CH375，NAK重试，超时出错重试
UINT8	WaitHostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog, UINT16 timeout )
{
	UINT8	i, s;
	while ( 1 ) {
		for ( i = 0; i < 40; i ++ ) {
			s = HostTransact374( endp_addr, pid, tog );
			if ( s != ( DEF_USB_PID_NAK | 0x20 ) || timeout == 0 ) 
                          return( s );
			mDelayuS( 20 );
		}
		if ( timeout < 0xFFFF ) timeout --;
	}
}

UINT8	HostCtrlTransfer374( PUINT8 ReqBuf, PUINT8 DatBuf, PUINT8 RetLen )
{
	UINT8	s, len, count, total;
	BOOL	tog;
	Write374Block( RAM_HOST_TRAN, 8, ReqBuf );
	Write374Byte( REG_USB_LENGTH, 8 );
	mDelayuS( 100 );
	s = WaitHostTransact374( 0, DEF_USB_PID_SETUP, FALSE, 200 );  // SETUP阶段，200mS超时
	if ( s == USB_INT_SUCCESS ) {  // SETUP成功
		tog = TRUE;  // 默认DATA1,默认无数据故状态阶段为IN
		total = *( ReqBuf + 6 );
		if ( total && DatBuf ) {  // 需要收发数据
			len = total;
			if ( *ReqBuf & 0x80 ) {  // 收
				while ( len ) {
					mDelayuS( 100 );
					s = WaitHostTransact374( 0, DEF_USB_PID_IN, tog, 200 );  // IN数据
					if ( s != USB_INT_SUCCESS ) break;
					count = Read374Byte( REG_USB_LENGTH );
					Read374Block( RAM_HOST_RECV, count, DatBuf );
					DatBuf += count;
					if ( count <= len ) len -= count;
					else len = 0;
					if ( count == 0 || ( count & ( UsbDevEndpSize - 1 ) ) ) break;  // 短包
					tog = tog ? FALSE : TRUE;
				}
				tog = FALSE;  // 状态阶段为OUT
			}
			else {  // 发
				while ( len ) {
					mDelayuS( 100 );
					count = len >= UsbDevEndpSize ? UsbDevEndpSize : len;
					Write374Block( RAM_HOST_TRAN, count, DatBuf );
					Write374Byte( REG_USB_LENGTH, count );
					s = WaitHostTransact374( 0, DEF_USB_PID_OUT, tog, 200 );  // OUT数据
					if ( s != USB_INT_SUCCESS ) break;
					DatBuf += count;
					len -= count;
					tog = tog ? FALSE : TRUE;
				}
				tog = TRUE;  // 状态阶段为IN
			}
			total -= len;  // 减去剩余长度得实际传输长度
		}
		if ( s == USB_INT_SUCCESS ) {  // 数据阶段成功
			Write374Byte( REG_USB_LENGTH, 0 );
			mDelayuS( 100 );
			s = WaitHostTransact374( 0, ( tog ? DEF_USB_PID_IN : DEF_USB_PID_OUT ), TRUE, 200 );  // STATUS阶段
			if ( tog && s == USB_INT_SUCCESS ) {  // 检查IN状态返回数据长度
				if ( Read374Byte( REG_USB_LENGTH ) ) s = USB_INT_BUF_OVER;  // 状态阶段错误
			}
		}
	}
	if ( RetLen ) *RetLen = total;  // 实际成功收发的总长度
	return( s );
}

// 查询当前是否存在USB设备
//BOOL	Query374DeviceIn( void )
//#define	Query374DeviceIn( )	( ( Read374Byte( REG_HUB_SETUP ) & BIT_HUB0_ATTACH ) ? TRUE : FALSE )

// 查询当前的USB设备是全速还是低速, 返回TRUE为全速
//BOOL	Query374DevFullSpeed( void )
//#define	Query374DevFullSpeed( )	( ( Read374Byte( REG_SYS_INFO ) & BIT_INFO_USB_DP ) ? TRUE : FALSE )

void DisableRootHubPort( UINT8 index )  // 关闭指定的ROOT-HUB端口,实际上硬件已经自动关闭,此处只是清除一些结构状态
{
  RootHubDev[ index ].DeviceStatus = ROOT_DEV_DISCONNECT;
  RootHubDev[ index ].DeviceAddress = 0x00;
  if ( index == 1 ) 
    Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL)&0xF0 );  // 清除有关HUB1的控制数据,实际上不需要清除
  else if ( index == 2 ) 
    Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL)&0x0F );  // 清除有关HUB2的控制数据,实际上不需要清除
  else 
    Write374Byte( REG_HUB_SETUP, Read374Byte(REG_HUB_SETUP)&0xF0 );  // 清除有关HUB0的控制数据,实际上不需要清除
}

void	AnalyzeRootHub( void )   // 分析ROOT-HUB状态,处理ROOT-HUB端口的设备插拔事件
{ //处理HUB端口的插拔事件，如果设备拔出，函数中调用DisableHubPort()函数，将端口关闭，插入事件，置相应端口的状态位
  if ( ( Read374Byte( REG_HUB_SETUP ) & BIT_HUB0_ATTACH ) &&
         RootHubDev[0].DeviceStatus == ROOT_DEV_DISCONNECT  //检测到有设备插入
         || ( Read374Byte( REG_HUB_SETUP ) & (BIT_HUB0_ATTACH|BIT_HUB0_EN) ) == BIT_HUB0_ATTACH ) {  //检测到有设备插入,但尚未允许,说明是刚插入
    DisableRootHubPort( 0 );  // 关闭端口
    RootHubDev[0].DeviceStatus=ROOT_DEV_CONNECTED;  //置连接标志
    printf( "HUB 0 device in\n" );
    NewDevCount++;
  }
  if( ! ( Read374Byte(REG_HUB_SETUP) & BIT_HUB0_ATTACH ) && RootHubDev[0].DeviceStatus >= ROOT_DEV_CONNECTED ) {  //检测到设备拔出
          DisableRootHubPort( 0 );  // 关闭端口
          printf( "HUB 0 device out\n" );
  }
  if ( ( Read374Byte( REG_HUB_CTRL ) & BIT_HUB1_ATTACH ) && RootHubDev[1].DeviceStatus == ROOT_DEV_DISCONNECT  //检测到有设备插入
          || ( Read374Byte( REG_HUB_CTRL ) & (BIT_HUB1_ATTACH|BIT_HUB1_EN) ) == BIT_HUB1_ATTACH ) {  //检测到有设备插入,但尚未允许,说明是刚插入
          DisableRootHubPort( 1 );  // 关闭端口
          RootHubDev[1].DeviceStatus=ROOT_DEV_CONNECTED;  //置连接标志
          printf( "HUB 1 device in\n" );
          NewDevCount++;
  }
  if( ! ( Read374Byte(REG_HUB_CTRL) & BIT_HUB1_ATTACH ) && RootHubDev[1].DeviceStatus >= ROOT_DEV_CONNECTED ) {  //检测到设备拔出
          DisableRootHubPort( 1 );  // 关闭端口
          printf( "HUB 1 device out\n" );
  }
  if ( ( Read374Byte( REG_HUB_CTRL ) & BIT_HUB2_ATTACH ) && RootHubDev[2].DeviceStatus == ROOT_DEV_DISCONNECT  //检测到有设备插入
          || ( Read374Byte( REG_HUB_CTRL ) & (BIT_HUB2_ATTACH|BIT_HUB2_EN) ) == BIT_HUB2_ATTACH ) {  //检测到有设备插入,但尚未允许,说明是刚插入
          DisableRootHubPort( 2 );  // 关闭端口
          RootHubDev[2].DeviceStatus=ROOT_DEV_CONNECTED;  //置连接标志
          printf( "HUB 2 device in\n" );
          NewDevCount++;
  }
  if( ! ( Read374Byte(REG_HUB_CTRL) & BIT_HUB2_ATTACH ) && RootHubDev[2].DeviceStatus >= ROOT_DEV_CONNECTED ) {  //检测到设备拔出
          DisableRootHubPort( 2 );  // 关闭端口
          printf( "HUB 2 device out\n" );
  }
}

void	HostDetectInterrupt( void )  // 处理USB设备插拔事件中断
{
  UINT8	s;
  s = Read374Byte( REG_INTER_FLAG );  // 获取中断状态
  if ( s & BIT_IF_DEV_DETECT ) {  // USB设备插拔事件
    AnalyzeRootHub( );   // 分析ROOT-HUB状态
    Write374Byte( REG_INTER_FLAG, BIT_IF_DEV_DETECT );  // 清中断标志
  }
  else {  // 意外的中断
    Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_TRANSFER | BIT_IF_USB_SUSPEND | BIT_IF_WAKE_UP );  // 清中断标志
  }
}

void	SetHostUsbAddr( UINT8 addr )  // 设置USB主机当前操作的USB设备地址
{
  Write374Byte( REG_USB_ADDR, addr );
}

void	HostSetBusFree( void )  // USB总线空闲
{
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE );  // USB主机方式
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // USB主机方式,允许SOF
	Write374Byte( REG_HUB_SETUP, 0x00 );  // 清BIT_HUB_DISABLE,允许内置的ROOT-HUB
	Write374Byte( REG_HUB_CTRL, 0x00 );  // 清除ROOT-HUB信息
}

void	HostSetBusReset( void )  // USB总线复位
{
	UsbDevEndpSize = DEFAULT_ENDP0_SIZE;  /* USB设备的端点0的最大包尺寸 */
	SetHostUsbAddr( 0x00 );
	Write374Byte( REG_USB_H_CTRL, 0x00 );
	Write374Byte( REG_HUB_SETUP, BIT_HUB0_RESET );  // 默认为全速,开始复位
	mDelaymS( 15 );  // 复位时间10mS到20mS
	Write374Byte( REG_HUB_SETUP, 0x00 );  // 结束复位
	mDelaymS( 1 );
	Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_USB_SUSPEND );  // 清中断标志
}

void	HostSetFullSpeed( void )  // 设定全速USB设备运行环境
{
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // 全速且发SOF
	Write374Byte( REG_HUB_SETUP, BIT_HUB0_EN );  // 使能HUB0端口
}

void	HostSetLowSpeed( void )  // 设定低速USB设备运行环境
{
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF | BIT_SETP_LOW_SPEED );  // 低速且发SOF
	Write374Byte( REG_HUB_SETUP, BIT_HUB0_EN | BIT_HUB0_POLAR );  // 使能HUB0端口
}

static uint8_t test_val;
void	Init374Host( void )  // 初始化USB主机
{
        test_val = Read374Byte(REG_SYS_INFO);
        printf("testcal :%d\r\n",test_val);
        Read374Byte(REG_USB_SETUP);
	SetHostUsbAddr( 0x00 );
        Write374Byte( REG_HUB_SETUP, 0x00 );   //Enable HUB
	Write374Byte( REG_USB_H_CTRL, 0x00 );
	Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  // 清所有中断标志
//	Write374Byte( REG_INTER_EN, BIT_IE_TRANSFER );  // 允许传输完成中断,因为本程序使用查询方式检测USB设备插拔,所以无需USB设备检测中断
	Write374Byte( REG_INTER_EN, BIT_IE_TRANSFER | BIT_IE_DEV_DETECT );  // 允许传输完成中断和USB设备检测中断
	Write374Byte( REG_SYS_CTRL, BIT_CTRL_OE_POLAR );  // 对于CH374T或者UEN引脚悬空的CH374S必须置BIT_CTRL_OE_POLAR为1
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // 全速且发SOF
        HostSetBusFree();  // USB总线空闲
}

UINT8	GetDeviceDescr( PUINT8 buf )  // 获取设备描述符
{
  UINT8	s, len;
  UsbDevEndpSize = DEFAULT_ENDP0_SIZE;
  s = HostCtrlTransfer374( (PUINT8)SetupGetDevDescr, buf, &len );  // 执行控制传输
  if ( s == USB_INT_SUCCESS ) {
    UsbDevEndpSize = ( (PUSB_DEV_DESCR)buf ) -> bMaxPacketSize0;  // 端点0最大包长度,这是简化处理,正常应该先获取前8字节后立即更新UsbDevEndpSize再继续
    if ( len < ( (PUSB_SETUP_REQ)SetupGetDevDescr ) -> wLengthL )
      s = USB_INT_BUF_OVER;  // 描述符长度错误
  }
  return( s );
}

UINT8 GetConfigDescr( PUINT8 buf )  // 获取配置描述符
{
  UINT8	s, len,i,c,j;
  UINT8	BufLogDescr[ sizeof( SetupGetCfgDescr ) ] ;
  s = HostCtrlTransfer374( (PUINT8)SetupGetCfgDescr, buf, &len );  // 执行控制传输
  if ( s == USB_INT_SUCCESS ) 
  {
    if ( len < ( (PUSB_SETUP_REQ)SetupGetCfgDescr )->wLengthL )
      s = USB_INT_BUF_OVER;  // 返回长度错误
    else 
    {
      memcpy ( BufLogDescr, SetupGetCfgDescr, sizeof( SetupGetCfgDescr ) );
      ( (PUSB_SETUP_REQ)BufLogDescr )->wLengthL = ( (PUSB_CFG_DESCR)buf )->wTotalLengthL;  // 完整配置描述符的总长度
      s = HostCtrlTransfer374( BufLogDescr, buf, &len );  // 执行控制传输
      if ( s == USB_INT_SUCCESS ) 
      {
        if ( len < ( (PUSB_CFG_DESCR)buf ) -> wTotalLengthL )
          s = USB_INT_BUF_OVER;  // 描述符长度错误
        else 
        {
#if 0
          for(i=0;i<( (PUSB_CFG_DESCR)buf ) -> wTotalLengthL;i++) 
          {
              if((buf[i]==0x09)&&(buf[i+1]==0x21)&&(buf[i+6]==0x22))  
                hid_des_leng = buf[i+7];    //获取报告描述符的长度
          }
          printf("hid_des_leng=%02x\n",(unsigned short)hid_des_leng);
          endp_out_addr = endp_in_addr = 0;
          endp_num = 0 ;
          for(i=0;i<( (PUSB_CFG_DESCR)buf )->wTotalLengthL;i++) 
          {
            if((buf[i]==0x09) && (buf[i+1]==0x04) &&(buf[i+5]==0x03)
               &&(buf[i+7]==0x01) || (buf[i+7] ==0x02 ))  //接口描述符为HID的鼠标、键盘   
            {
              for(j=0;j<( (PUSB_CFG_DESCR)buf ) -> wTotalLengthL-i;j++)
              {
                if((buf[i+j] == 0x07) && (buf[i+j+1] == 0x05) && (buf[i+j+3] == 0x03)) {
                    c = buf[i+j+2];        //判断是否为中断端点
                  if ( c & 0x80 )  
                    endp_in_addr = c & 0x0f;         // IN端点的地址                                
                  else
                  {
                    endp_out_addr = c & 0x0f;         // OUT端点 
                  }
                }
                if((endp_out_addr!=0) || (endp_in_addr!=0))
                  break;                                       
              }
            }
           if((endp_out_addr!=0) || (endp_in_addr!=0))  
             break;
          }
          printf("endp_in_addr=%02x\n",(unsigned short)endp_in_addr);
          printf("endp_out_addr=%02x\n",(unsigned short)endp_out_addr);
#endif
        }
      }
    }
}
return( s );
}

UINT8	SetUsbAddress( UINT8 addr )  // 设置USB设备地址
{
	UINT8	s;
	memcpy ( CtrlBuf, SetupSetUsbAddr, sizeof( SetupSetUsbAddr ) );
	( (PUSB_SETUP_REQ)CtrlBuf ) -> wValueL = addr;  // USB设备地址
	s = HostCtrlTransfer374( CtrlBuf, NULL, NULL );  // 执行控制传输
	if ( s == USB_INT_SUCCESS ) {
		SetHostUsbAddr( addr );  // 设置USB主机当前操作的USB设备地址
	}
	mDelaymS( 10 );  // 等待USB设备完成操作
	return( s );
}

UINT8	SetUsbConfig( UINT8 cfg )  // 设置USB设备配置
{
  UINT8	BufSetCfg[ sizeof( SetupSetUsbConfig ) ] ;
  memcpy ( BufSetCfg, SetupSetUsbConfig, sizeof( SetupSetUsbConfig ) );
  ( (PUSB_SETUP_REQ)BufSetCfg ) -> wValueL = cfg;  // USB设备配置
  return( HostCtrlTransfer374( BufSetCfg, NULL, NULL ) );  // 执行控制传输
  }



/* 设置Idle */
UINT8  Set_Idle(void)
{
  UINT8  s;
  s=HostCtrlTransfer374((PUINT8)SetupSetidle,NULL,NULL);
  return s;
}

/* 获取报表描述符 */
UINT8  Get_Hid_Des(unsigned char *p)
{
  UINT8  s;
  //leng=SetupGetHidDes[0x06]-0x40;   //报表描述符的长度在发送数据长度的基础上减去0X40
  s=HostCtrlTransfer374((PUINT8)SetupGetHidDes,p,(PUINT8)&SetupGetHidDes[0x06]);
  return s;
}


/* 设置报表 */
UINT8  Set_Report(unsigned char *p)
{
  UINT8  s,l=1;
  s=HostCtrlTransfer374((PUINT8)SetupSetReport,p,&l); //实际的数据可以写别的数据
  return s;
}

/*通过中断端点获取鼠标、键盘上传的数据 */
unsigned char Interrupt_Data_Trans(UINT8 in_addr,PUINT8 p,PUINT8 counter)
{
  UINT8  s,count;
//  static UINT8 tog1;
  s = WaitHostTransact374( in_addr, DEF_USB_PID_IN, tog1, 10 );  // IN数据
  if ( s != USB_INT_SUCCESS ) return s;
  else
  {
    count = Read374Byte( REG_USB_LENGTH );
    Read374Block( RAM_HOST_RECV, count, p );
    tog1 = tog1 ? FALSE : TRUE;
    *counter = count;
  }
  return s;
}

int Ch374_CheckPortAttach(void)
{
  uint8_t status = 0;
  status = Read374Byte( REG_HUB_SETUP );
  status &= 0x08;
  return ((status) ? TRUE : FALSE );
}

int Query374DevPortSpeed(void)
{
  uint8_t reg_val;
  reg_val = Read374Byte(REG_HUB_SETUP);
  reg_val &= 0x04;
  return ((reg_val)?TRUE:FALSE);
}

/*
 * The following part is used to deal with ROOTHUB
*/
void HostEnableRootHub( void )  // 启用内置的Root-HUB
{
  Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // USB主机方式,允许SOF
  Write374Byte( REG_HUB_SETUP, 0x00 );  // 清BIT_HUB_DISABLE,允许内置的ROOT-HUB
  Write374Byte( REG_HUB_CTRL, 0x00 );  // 清除ROOT-HUB信息
}



void ResetRootHubPort( UINT8 index )  // 检测到设备后,复位相应端口的总线,为枚举设备准备,设置为默认为全速
{
  UsbDevEndpSize = DEFAULT_ENDP0_SIZE;  /* USB设备的端点0的最大包尺寸 */
  SetHostUsbAddr( 0x00 );
  Write374Byte( REG_USB_H_CTRL, 0x00 );
  if ( index == 1 ) {
    Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL) & ~ BIT_HUB1_POLAR | BIT_HUB1_RESET );  // 默认为全速,开始复位
    mDelaymS( 15 );																 // 复位时间10mS到20mS
    Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL) & ~ BIT_HUB1_RESET );  // 结束复位
  }
  else if ( index == 2 ) {
    Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL) & ~ BIT_HUB2_POLAR | BIT_HUB2_RESET );  // 默认为全速,开始复位
    mDelaymS( 15 );																 // 复位时间10mS到20mS
    Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL) & ~ BIT_HUB2_RESET );  // 结束复位
  }
  else {
    Write374Byte( REG_HUB_SETUP, Read374Byte(REG_HUB_SETUP) & ~ BIT_HUB0_POLAR | BIT_HUB0_RESET );  // 默认为全速,开始复位
    mDelaymS( 15 );																 // 复位时间10mS到20mS
    Write374Byte( REG_HUB_SETUP, Read374Byte(REG_HUB_SETUP) & ~ BIT_HUB0_RESET );  // 结束复位
  }
  mDelayuS( 250 );
  Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_USB_SUSPEND );  // 清中断标志
}


BOOL EnableRootHubPort( UINT8 index )  // 使能ROOT-HUB端口,相应的BIT_HUB?_EN置1开启端口,返回FALSE设置失败(可能是设备断开了)
{
  if ( RootHubDev[ index ].DeviceStatus < ROOT_DEV_CONNECTED )
    RootHubDev[ index ].DeviceStatus = ROOT_DEV_CONNECTED;
  if ( index == 1 ) {
    if ( Read374Byte(REG_HUB_CTRL)&BIT_HUB1_ATTACH ) {  // 有设备
      if ( !(Read374Byte(REG_HUB_CTRL)&BIT_HUB1_EN) ) {  // 尚未使能
        if ( !(Read374Byte(REG_HUB_SETUP) & BIT_HUB1_DX_IN) ) 
          Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL) ^ BIT_HUB1_POLAR );  // 如果速度不匹配则设置极性
//        Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL)  & (~0x04));
        RootHubDev[1].DeviceSpeed= !( Read374Byte(REG_HUB_CTRL) & BIT_HUB1_POLAR );
      }
      Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL)|BIT_HUB1_EN );	//使能HUB端口
      return( TRUE );
    }
  }
  else if ( index == 2 ) {
    if ( Read374Byte(REG_HUB_CTRL)&BIT_HUB2_ATTACH ) {  // 有设备
      if ( !(Read374Byte(REG_HUB_CTRL)&BIT_HUB2_EN) ) {  // 尚未使能
        if ( !(Read374Byte(REG_HUB_SETUP) & BIT_HUB2_DX_IN) ) Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL) ^ BIT_HUB2_POLAR );  // 如果速度不匹配则设置极性
        RootHubDev[2].DeviceSpeed= !( Read374Byte(REG_HUB_CTRL) & BIT_HUB2_POLAR );
      }
      Write374Byte( REG_HUB_CTRL, Read374Byte(REG_HUB_CTRL)|BIT_HUB2_EN );	//使能HUB端口
      return( TRUE );
    }
  }
  else {
    if ( Read374Byte(REG_HUB_SETUP)&BIT_HUB0_ATTACH ) {  // 有设备
      if ( !(Read374Byte(REG_HUB_SETUP)&BIT_HUB0_EN) ) {  // 尚未使能
        if ( !(Read374Byte(REG_INTER_FLAG) & BIT_HUB0_DX_IN) ) Write374Byte( REG_HUB_SETUP, Read374Byte(REG_HUB_SETUP) ^ BIT_HUB0_POLAR );  // 如果速度不匹配则设置极性
        RootHubDev[0].DeviceSpeed= !( Read374Byte(REG_HUB_SETUP) & BIT_HUB0_POLAR );
      }
      Write374Byte( REG_HUB_SETUP, Read374Byte(REG_HUB_SETUP)|BIT_HUB0_EN );  //使能HUB端口
      return( TRUE );
    }
  }
  return( FALSE );
}


void	SetUsbSpeed( BOOL FullSpeed )  // 设置当前USB速度
{
  if ( FullSpeed ) {  // 全速
    Write374Byte( REG_USB_SETUP, Read374Byte( REG_USB_SETUP ) & BIT_SETP_RAM_MODE | BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // 全速
    Write374Byte( REG_HUB_SETUP, Read374Byte( REG_HUB_SETUP ) & ~ BIT_HUB_PRE_PID );  // 禁止PRE PID
  }
  else 
    Write374Byte( REG_USB_SETUP, 
                 Read374Byte( REG_USB_SETUP ) & BIT_SETP_RAM_MODE | BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF | BIT_SETP_LOW_SPEED );  // 低速
}


UINT8	InitDevice( UINT8 index )  // 初始化/枚举指定ROOT-HUB端口的USB设备
// 输入: 内置HUB端口号0/1/2
// 输出: 0-操作失败, 0x31-成功枚举到USB键盘, 0x32-成功枚举到鼠标, 0x70-成功枚举到打印机, 0x80-成功枚举到U盘, 0xFF-成功枚举未知设备, 其它值暂未定义
{
  UINT8	i, s, cfg, dv_cls, if_cls;
  printf( "Start reset HUB%01d port\n", (UINT16)index );
  ResetRootHubPort( index );  // 检测到设备后,复位相应端口的USB总线
  for ( i = 0, s = 0; i < 100; i ++ ) {  // 等待USB设备复位后重新连接
    if ( EnableRootHubPort( index ) ) {  // 使能ROOT-HUB端口
            i = 0;
            s ++;  // 计时等待USB设备连接后稳定
            if ( s > 100 ) break;  // 已经稳定连接
    }
    mDelaymS( 2 );
  }
  if ( i ) {
    // 复位后设备没有连接
    DisableRootHubPort( index );
    printf( "Disable HUB%01d port because of disconnect\n", (UINT16)index );
    return( DEV_ERROR );
  }
  SetUsbSpeed( RootHubDev[index].DeviceSpeed );  // 设置当前USB速度
  printf( "GetDeviceDescr @HUB%1d: ", (UINT16)index );
  s = GetDeviceDescr( TempBuf );  // 获取设备描述符
  if ( s == USB_INT_SUCCESS ) {
    for ( i = 0; i < ( (PUSB_SETUP_REQ)SetupGetDevDescr ) -> wLengthL; i ++ )
      printf( "0x%02X ", (UINT16)( TempBuf[i] ) );
    printf( "\n" ); // 显示出描述符
    dv_cls = ( (PUSB_DEV_DESCR)TempBuf ) -> bDeviceClass;  // 设备类代码
    s = SetUsbAddress( index + ( (PUSB_SETUP_REQ)SetupSetUsbAddr ) -> wValueL );  // 设置USB设备地址,加上index可以保证三个HUB端口分配不同的地址
    if ( s == USB_INT_SUCCESS ) {
      RootHubDev[index].DeviceAddress = 
        index + ( (PUSB_SETUP_REQ)SetupSetUsbAddr ) -> wValueL;  // 保存USB地址
      printf("GetConfigDescr: " );
      s = GetConfigDescr( TempBuf );  // 获取配置描述符
      if ( s == USB_INT_SUCCESS ) {
        cfg = ((PUSB_CFG_DESCR)TempBuf ) -> bConfigurationValue;
        for ( i = 0; i < ( (PUSB_CFG_DESCR)TempBuf ) -> wTotalLengthL; i ++ ) printf( "0x%02X ", (UINT16)( TempBuf[i] ) );
        printf("\n");
/* 分析配置描述符，获取端点数据/各端点地址/各端点大小等，更新变量endp_addr和endp_size等 */
        if_cls = ( (PUSB_CFG_DESCR_LONG)TempBuf ) -> itf_descr.bInterfaceClass;  // 接口类代码
        if ( dv_cls == 0x00 && if_cls == 0x08 ) {  // 是USB存储类设备,基本上确认是U盘
          s = SetUsbConfig( cfg );  // 设置USB设备配置
          if ( s == USB_INT_SUCCESS ) {
            RootHubDev[index].DeviceStatus =ROOT_DEV_SUCCESS;
            printf( "USB-Disk Ready\n" );
            return( DEV_DISK );  /* U盘初始化成功 */
          }
        }
        else if (dv_cls == 0x00 && 
                 if_cls == 0x07 && 
                 ( (PUSB_CFG_DESCR_LONG)TempBuf ) -> itf_descr.bInterfaceSubClass == 0x01 ) {  // 是打印机类设备
          s = SetUsbConfig( cfg );  // 设置USB设备配置
          if ( s == USB_INT_SUCCESS ) {
            RootHubDev[index].DeviceStatus =ROOT_DEV_SUCCESS;
            SetUsbSpeed( TRUE );  // 默认为全速
            printf( "USB-Print Ready\n" );
            return( DEV_PRINT );  /* 打印机初始化成功 */
          }
        }
        else if (dv_cls == 0x00 && 
                 if_cls == 0x03 && 
                 ((PUSB_CFG_DESCR_LONG)TempBuf ) -> itf_descr.bInterfaceSubClass <= 0x01 ) 
        {
          // 是HID类设备,键盘/鼠标等
          s = SetUsbConfig( cfg );  // 设置USB设备配置
          if ( s == USB_INT_SUCCESS ) {
            s = AnalyzeHidIntEndp();  // 从描述符中分析出HID中断端点的地址
            RootHubDev[index].GpVar = s;  // 保存中断端点的地址,位7用于同步标志位,清0
            RootHubDev[index].DeviceStatus =ROOT_DEV_SUCCESS;
            SetUsbSpeed( TRUE );  // 默认为全速
            s = ( (PUSB_CFG_DESCR_LONG)TempBuf )->itf_descr.bInterfaceProtocol;
            if ( s == 1 ) {
              //进一步初始化,例如设备键盘指示灯LED等
              Set_Idle();
              printf( "USB-Keyboard Ready\n" );
              return( DEV_KEYBOARD );  /* 键盘初始化成功 */
            }
            else if ( s == 2 ) {
              //为了以后查询鼠标状态,应该分析描述符,取得中断端口的地址,长度等信息
              printf( "USB-Mouse Ready\n" );
              return( DEV_MOUSE );  /* 鼠标初始化成功 */
            }
          }
        }
        else {   // 可以进一步分析
          s = SetUsbConfig( cfg );  // 设置USB设备配置
          if ( s == USB_INT_SUCCESS ) {
            //需保存端点信息以便主程序进行USB传输
            RootHubDev[index].DeviceStatus =ROOT_DEV_SUCCESS;
            SetUsbSpeed( TRUE );  // 默认为全速
            return( DEV_UNKNOWN );  /* 未知设备初始化成功 */
          }
        }
      }
    }
  }
  printf( "InitDevice Error = %02X\n", (UINT16)s );
  RootHubDev[index].DeviceStatus =ROOT_DEV_FAILED;
  SetUsbSpeed( TRUE );  // 默认为全速
  return( DEV_ERROR );
}




UINT8	AnalyzeHidIntEndp(void)  // 从描述符中分析出HID中断端点的地址
{
  UINT8	i, s, l;
  s = 0;
  for ( i = 0; i < ( (PUSB_CFG_DESCR)TempBuf ) -> wTotalLengthL; i += l ) {  // 搜索中断端点描述符,跳过配置描述符和接口描述符
    if ( ( (PUSB_ENDP_DESCR)(TempBuf+i) ) -> bDescriptorType == USB_ENDP_DESCR_TYPE  // 是端点描述符
            && ( (PUSB_ENDP_DESCR)(TempBuf+i) ) -> bmAttributes == USB_ENDP_TYPE_INTER  // 是中断端点
            && ( ( (PUSB_ENDP_DESCR)(TempBuf+i) ) -> bEndpointAddress & 0x80 ) ) {  // 是IN端点
      s = ( (PUSB_ENDP_DESCR)(TempBuf+i) ) -> bEndpointAddress & 0x7F;  // 中断端点的地址
      break;  // 可以根据需要保存wMaxPacketSize和bInterval
    }
    l = ((PUSB_ENDP_DESCR)(TempBuf+i))->bLength;  // 当前描述符长度,跳过
    if (l > 16) break;
  }
  return(s);
}

UINT16	SearchAllHubPort( UINT8 type )  // 在ROOT-HUB以及外部HUB各端口上搜索指定类型的设备所在的端口号,输出端口号为0xFFFF则未搜索到
{
  // 输出高8位为ROOT-HUB端口号,低8位为外部HUB的端口号,低8位为0则设备直接在ROOT-HUB端口上
  // 当然也可以根据USB的厂商VID产品PID进行搜索(事先要记录各设备的VID和PID),以及指定搜索序号
  UINT8	i, port;
  i = SearchRootHubPort( type );  // 搜索指定类型的设备所在的端口号
  if ( i != 0xFF ) 
    return( (UINT16)i << 8 );  // 在ROOT-HUB端口上
  for ( i = 0; i < 3; i ++ ) {  // 现时搜索可以避免设备中途拔出而某些信息未及时更新的问题
    if ( RootHubDev[i].DeviceType == DEV_HUB && RootHubDev[i].DeviceStatus >= ROOT_DEV_SUCCESS ) {  // 外部集线器HUB且枚举成功
      for ( port = 1; port <= RootHubDev[i].GpVar; port ++ ) {  // 搜索外部HUB的各个端口
        if ( DevOnHubPort[i][port-1].DeviceType == type && DevOnHubPort[i][port-1].DeviceStatus >= ROOT_DEV_SUCCESS )
                return( ( (UINT16)i << 8 ) | port );  // 类型匹配且枚举成功
      }
    }
  }
  return(0xFFFF);
}



void SelectHubPort( UINT8 HubIndex, UINT8 PortIndex )  // PortIndex=0选择操作指定的ROOT-HUB端口,否则选择操作指定的ROOT-HUB端口的外部HUB的指定端口
{
  if ( PortIndex ) {  
    // 选择操作指定的ROOT-HUB端口的外部HUB的指定端口
    SetHostUsbAddr( DevOnHubPort[HubIndex][PortIndex-1].DeviceAddress );  // 设置USB主机当前操作的USB设备地址
    if ( DevOnHubPort[HubIndex][PortIndex-1].DeviceSpeed == 0 )  // 通过外部HUB与低速USB设备通讯需要前置ID
            Write374Byte( REG_HUB_SETUP, Read374Byte( REG_HUB_SETUP ) | BIT_HUB_PRE_PID );  // 启用PRE PID
    SetUsbSpeed( DevOnHubPort[HubIndex][PortIndex-1].DeviceSpeed );  // 设置当前USB速度
  }
  else {
    // 选择操作指定的ROOT-HUB端口
    SetHostUsbAddr( RootHubDev[HubIndex].DeviceAddress );  // 设置USB主机当前操作的USB设备地址
    SetUsbSpeed( RootHubDev[HubIndex].DeviceSpeed );  // 设置当前USB速度
  }
}



UINT8	Level2DevEnum( UINT8 HubIndex, UINT8 PortIndex )  // 初始化枚举外部HUB后的二级USB设备
{
	UINT8	i, s, cfg, dv_cls, if_cls;
	printf( "Enum dev @ExtHub-port%01d ", (UINT16)PortIndex );
	printf( "@RootHub%01d\n", (UINT16)HubIndex );
	if ( PortIndex == 0 ) return( DEV_ERROR );
	SelectHubPort( HubIndex, PortIndex );  // 选择操作指定的ROOT-HUB端口的外部HUB的指定端口,选择速度
	printf( "GetDeviceDescr: " );
	s = GetDeviceDescr( TempBuf );  // 获取设备描述符
	if ( s != USB_INT_SUCCESS ) return( DEV_ERROR );
	dv_cls = ( (PUSB_DEV_DESCR)TempBuf ) -> bDeviceClass;  // 设备类代码
	cfg = ( (HubIndex+1)<<4 ) + PortIndex;  // 计算出一个USB地址,避免地址重叠
	s = SetUsbAddress( cfg );  // 设置USB设备地址
	if ( s != USB_INT_SUCCESS ) return( DEV_ERROR );
	DevOnHubPort[HubIndex][PortIndex-1].DeviceAddress = cfg;  // 保存分配的USB地址
	printf("GetConfigDescr: " );
	s = GetConfigDescr( TempBuf );  // 获取配置描述符
	if ( s != USB_INT_SUCCESS ) return( DEV_ERROR );
	cfg = ((PUSB_CFG_DESCR)TempBuf ) -> bConfigurationValue;
	for ( i = 0; i < ( (PUSB_CFG_DESCR)TempBuf ) -> wTotalLengthL; i ++ ) printf( "0x%02X ", (UINT16)( TempBuf[i] ) );
	printf("\n");
	/* 分析配置描述符，获取端点数据/各端点地址/各端点大小等，更新变量endp_addr和endp_size等 */
	if_cls = ( (PUSB_CFG_DESCR_LONG)TempBuf ) -> itf_descr.bInterfaceClass;  // 接口类代码
	if ( dv_cls == 0x00 && if_cls == 0x08 ) {  // 是USB存储类设备,基本上确认是U盘
		s = SetUsbConfig( cfg );  // 设置USB设备配置
		if ( s == USB_INT_SUCCESS ) {
			DevOnHubPort[HubIndex][PortIndex-1].DeviceStatus =ROOT_DEV_SUCCESS;
//			SetUsbSpeed( TRUE );  // 默认为全速
			printf( "USB-Disk Ready\n" );
			return( DEV_DISK );  /* U盘初始化成功 */
		}
	}
	else if ( dv_cls == 0x00 && if_cls == 0x03 && ( (PUSB_CFG_DESCR_LONG)TempBuf ) -> itf_descr.bInterfaceSubClass <= 0x01 ) {  // 是HID类设备,键盘/鼠标等
		s = SetUsbConfig( cfg );  // 设置USB设备配置
		if ( s == USB_INT_SUCCESS ) {
//			需保存端点信息以便主程序进行USB传输
			s = AnalyzeHidIntEndp( );  // 从描述符中分析出HID中断端点的地址
			DevOnHubPort[HubIndex][PortIndex-1].GpVar = s;  // 保存中断端点的地址,位7用于同步标志位,清0
			DevOnHubPort[HubIndex][PortIndex-1].DeviceStatus =ROOT_DEV_SUCCESS;
			SetUsbSpeed( TRUE );  // 默认为全速
			s = ( (PUSB_CFG_DESCR_LONG)TempBuf ) -> itf_descr.bInterfaceProtocol;
			if ( s == 1 ) {
//				进一步初始化,例如设备键盘指示灯LED等
				printf( "USB-Keyboard Ready\n" );
				return( DEV_KEYBOARD );  /* 键盘初始化成功 */
			}
			else if ( s == 2 ) {
//				为了以后查询鼠标状态,应该分析描述符,取得中断端口的地址,长度等信息
				printf( "USB-Mouse Ready\n" );
				return( DEV_MOUSE );  /* 鼠标初始化成功 */
			}
		}
	}
	else if ( dv_cls == 0x09 ) {  // 是HUB类设备,集线器等
		printf( "This program don't support Level 2 HUB\n");  // 需要支持多级HUB级联请参考本程序进行扩展
	}
	else {   // 可以进一步分析
		s = SetUsbConfig( cfg );  // 设置USB设备配置
		if ( s == USB_INT_SUCCESS ) {
//			需保存端点信息以便主程序进行USB传输
			DevOnHubPort[HubIndex][PortIndex-1].DeviceStatus =ROOT_DEV_SUCCESS;
			SetUsbSpeed( TRUE );  // 默认为全速
			return( DEV_UNKNOWN );  /* 未知设备初始化成功 */
		}
	}
	printf( "InitDevice Error = %02X\n", (UINT16)s );
	DevOnHubPort[HubIndex][PortIndex-1].DeviceStatus =ROOT_DEV_FAILED;
	SetUsbSpeed( TRUE );  // 默认为全速
	return( DEV_ERROR );
}



UINT8 HubPortEnum( UINT8 index )  // 枚举指定ROOT-HUB端口上的外部HUB集线器的各个端口,检查各端口有无连接或移除事件
{
  UINT8	i, s;
  //	printf( "Enum external HUB port\n" );
  for ( i = 1; i <= RootHubDev[index].GpVar; i ++ ) {  // 查询集线器的端口是否有变化
    SelectHubPort( index, 0 );  // 选择操作指定的ROOT-HUB端口,设置当前USB速度以及被操作设备的USB地址
    s = GetPortStatus( i );  // 获取端口状态
    if ( s != USB_INT_SUCCESS ) 
      return( s );  // 可能是该HUB断开了
    if ( (TempBuf[0]&0x01) && (TempBuf[2]&0x01) ) {  // 发现有设备连接
        DevOnHubPort[index][i-1].DeviceStatus = ROOT_DEV_CONNECTED;  // 有设备连接
        DevOnHubPort[index][i-1].DeviceAddress = 0;
        s = GetPortStatus( i );  // 获取端口状态
        if ( s != USB_INT_SUCCESS ) return( s );  // 可能是该HUB断开了
        DevOnHubPort[index][i-1].DeviceSpeed = TempBuf[1] & 0x02 ? 0 : 1;  // 低速还是全速
        if ( DevOnHubPort[index][i-1].DeviceSpeed ) printf( "Found full speed device on port %01d\n", (UINT16)i );
        else printf( "Found low speed device on port %01d\n", (UINT16)i );
        mDelaymS( 200 );  // 等待设备上电稳定	
        s = SetPortFeature( i, PORT_RESET );  // 对有设备连接的端口复位
        if ( s != USB_INT_SUCCESS ) return( s );  // 可能是该HUB断开了
        printf( "Reset port and then wait in\n" );
        do {  // 查询复位端口,直到复位完成,把完成后的状态显示出来
                mDelaymS( 1 );
                s = GetPortStatus( i );
                if ( s != USB_INT_SUCCESS ) return( s );  // 可能是该HUB断开了
        } while ( TempBuf[0] & 0x10 );  // 端口正在复位则等待
        mDelaymS( 100 );
        s = ClearPortFeature( i, C_PORT_CONNECTION ); // 清除连接或移除变化标志
        if ( s != USB_INT_SUCCESS ) return( s );
        s = GetPortStatus( i );  // 再读取状态,复查设备是否还在
        if ( s != USB_INT_SUCCESS ) return( s );
        if ( (TempBuf[0]&0x01) == 0 ) DevOnHubPort[index][i-1].DeviceStatus = ROOT_DEV_DISCONNECT;  // 设备不在了
        s = Level2DevEnum( index, i );
        DevOnHubPort[index][i-1].DeviceType = s;  // 保存设备类型
        SetUsbSpeed( TRUE );  // 默认为全速
    }
    else if ( (TempBuf[0]&0x01) == 0 ) {  // 设备已经断开
      DevOnHubPort[index][i-1].DeviceStatus = ROOT_DEV_DISCONNECT;  // 有设备连接
      if ( TempBuf[2]&0x01 ) ClearPortFeature( i, C_PORT_CONNECTION ); // 清除移除变化标志
    }
  }
  return( USB_INT_SUCCESS );  // 返回操作成功
}

UINT8	SearchRootHubPort( UINT8 type )  // 搜索指定类型的设备所在的端口号,输出端口号为0xFF则未搜索到
{
  // 当然也可以根据USB的厂商VID产品PID进行搜索(事先要记录各设备的VID和PID),以及指定搜索序号
  UINT8	i;
  for ( i = 0; i < 3; i ++ ) {
    // 现时搜索可以避免设备中途拔出而某些信息未及时更新的问题
    if (RootHubDev[i].DeviceType == type && 
        RootHubDev[i].DeviceStatus >= ROOT_DEV_SUCCESS )
      return( i );  // 类型匹配且枚举成功
  }
  return( 0xFF );
}



UINT8	GetHubDescriptor( void )  // 获取HUB描述符
{
	UINT8 s,len;
	CtrlBuf[0] = GET_HUB_DESCRIPTOR;
	CtrlBuf[1] = GET_DESCRIPTOR;
	CtrlBuf[2] = 0x00;
	CtrlBuf[3] = 0x29;
	CtrlBuf[4] = 0x00;
	CtrlBuf[5] = 0x00;
	CtrlBuf[6] = 0x01;
	CtrlBuf[7] = 0x00;
	s = HostCtrlTransfer374( CtrlBuf, TempBuf, &len );  // 执行控制传输
	if ( s == USB_INT_SUCCESS )
	{
		CtrlBuf[6] = TempBuf[0];
		CtrlBuf[0] = GET_HUB_DESCRIPTOR;
		CtrlBuf[1] = GET_DESCRIPTOR;
		CtrlBuf[2] = 0x00;
		CtrlBuf[3] = 0x29;
		CtrlBuf[4] = 0x00;
		CtrlBuf[5] = 0x00;
		CtrlBuf[7] = 0x00;
		s = HostCtrlTransfer374( CtrlBuf, TempBuf, &len );  // 执行控制传输
	}
	return s;
}

UINT8	GetPortStatus( UINT8 port )  // 查询HUB端口状态
{
	UINT8 s,len;
	CtrlBuf[0] = GET_PORT_STATUS;
	CtrlBuf[1] = GET_STATUS;
	CtrlBuf[2] = 0x00;
	CtrlBuf[3] = 0x00;
	CtrlBuf[4] = port;
	CtrlBuf[5] = 0x00;
	CtrlBuf[6] = 4;
	CtrlBuf[7] = 0x00;
	s = HostCtrlTransfer374( CtrlBuf, TempBuf, &len );  // 执行控制传输
	return s;
}

UINT8	SetPortFeature( UINT8 port, UINT8 select )
{
	UINT8 s,len;
	CtrlBuf[0] = SET_PORT_FEATURE;
	CtrlBuf[1] = SET_FEATURE;
	CtrlBuf[2] = select;
	CtrlBuf[3] = 0x00;
	CtrlBuf[4] = port;
	CtrlBuf[5] = 0x00;
	CtrlBuf[6] = 0x00;
	CtrlBuf[7] = 0x00;
	s = HostCtrlTransfer374( CtrlBuf, TempBuf, &len );  // 执行控制传输
	return s;
}

UINT8	ClearPortFeature( UINT8 port, UINT8 select )
{
	UINT8 s,len;
	CtrlBuf[0] = CLEAR_PORT_FEATURE;
	CtrlBuf[1] = CLEAR_FEATURE;
	CtrlBuf[2] = select;
	CtrlBuf[3] = 0x00;
	CtrlBuf[4] = port;
	CtrlBuf[5] = 0x00;
	CtrlBuf[6] = 0x00;
	CtrlBuf[7] = 0x00;
	s = HostCtrlTransfer374( CtrlBuf, TempBuf, &len );  // 执行控制传输
	return s;

}