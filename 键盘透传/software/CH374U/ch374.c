
#include <rthw.h>
#include "port.h"



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

_RootHubDev RootHubDev[3];
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
UINT8	HostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog )
{  // 本子程序着重于易理解,而在实际应用中,为了提供运行速度,应该对本子程序代码进行优化
	UINT8	retry;
	UINT8	s, r, u;
	for ( retry = 0; retry < 3; retry ++ ) {
		Write374Byte( REG_USB_H_PID, M_MK_HOST_PID_ENDP( pid, endp_addr ) );  // 指定令牌PID和目的端点号
//		Write374Byte( REG_USB_H_CTRL, BIT_HOST_START | ( tog ? ( BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : 0x00 ) );  // 设置同步标志并启动传输
		Write374Byte( REG_USB_H_CTRL, ( tog ? ( BIT_HOST_START | BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : BIT_HOST_START ) );  // 设置同步标志并启动传输
//		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE );  // 取消暂停
		s = Wait374Interrupt( );
		if ( s == ERR_USB_UNKNOWN ) return( s );  // 中断超时,可能是硬件异常
		s = Read374Byte( REG_INTER_FLAG );  // 获取中断状态
		if ( s & BIT_IF_DEV_DETECT ) {  // USB设备插拔事件
			mDelayuS( 200 );  // 等待传输完成
			Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_TRANSFER );  // 清中断标志
			if ( s & BIT_IF_DEV_ATTACH ) {  // USB设备连接事件
				u = Read374Byte( REG_USB_SETUP );
				if ( s & BIT_IF_USB_DX_IN ) {  // 速度匹配，不需要切换速度
					if ( u & BIT_SETP_USB_SPEED ) return( USB_INT_CONNECT_LS );  // 低速USB设备
					return( USB_INT_CONNECT );  // 全速USB设备
				}
				else {  // 速度失配，需要切换速度
					if ( u & BIT_SETP_USB_SPEED ) return( USB_INT_CONNECT );  // 全速USB设备
					return( USB_INT_CONNECT_LS );  // 低速USB设备
				}
			}
			else return( USB_INT_DISCONNECT );  // USB设备断开事件
		}
		else if ( s & BIT_IF_TRANSFER ) {  // 传输完成
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
			if ( s != ( DEF_USB_PID_NAK | 0x20 ) || timeout == 0 ) return( s );
			mDelayuS( 20 );
		}
		if ( timeout < 0xFFFF ) timeout --;
	}
}

UINT8	HostCtrlTransfer374( PUINT8 ReqBuf, PUINT8 DatBuf, PUINT8 RetLen )  // 执行控制传输,ReqBuf指向8字节请求码,DatBuf为收发缓冲区
// 如果需要接收和发送数据，那么DatBuf需指向有效缓冲区用于存放后续数据，实际成功收发的总长度保存在ReqLen指向的字节变量中
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

void	HostDetectInterrupt( void )  // 处理USB设备插拔事件中断
{
  UINT8	s, u;
  s = Read374Byte( REG_INTER_FLAG );  // 获取中断状态
  if (s & BIT_IF_DEV_DETECT )
  {
    // USB设备插拔事件
    Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT );  // 清中断标志
    if ( s & BIT_IF_DEV_ATTACH ) 
    {  // USB设备连接事件
      u = Read374Byte( REG_USB_SETUP );
      if ( s & BIT_IF_USB_DX_IN ) 
      {  // 速度匹配，不需要切换速度
        if ( u & BIT_SETP_USB_SPEED ) 
          FlagDeviceStatus = USB_INT_CONNECT_LS;  // 低速USB设备
        else 
          FlagDeviceStatus = USB_INT_CONNECT;  // 全速USB设备
      }
      else
      {  // 速度失配，需要切换速度
        if ( u & BIT_SETP_USB_SPEED )
          FlagDeviceStatus = USB_INT_CONNECT;  // 全速USB设备
        else 
          FlagDeviceStatus = USB_INT_CONNECT_LS;  // 低速USB设备
      }
    }
    else 
      FlagDeviceStatus = USB_INT_DISCONNECT;  // USB设备断开事件
  }
  else 
  {  // 意外的中断
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

void	Init374Host( void )  // 初始化USB主机
{
        Read374Byte(REG_SYS_INFO);
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
	s = HostCtrlTransfer374( (PUINT8)SetupGetDevDescr, buf, &len );  // 执行控制传输
	if ( s == USB_INT_SUCCESS ) {
		UsbDevEndpSize = ( (PUSB_DEV_DESCR)buf ) -> bMaxPacketSize0;  // 端点0最大包长度,这是简化处理,正常应该先获取前8字节后立即更新UsbDevEndpSize再继续
		if ( len < ( (PUSB_SETUP_REQ)SetupGetDevDescr ) -> wLengthL ) s = USB_INT_BUF_OVER;  // 描述符长度错误
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
        }
      }
    }
}
return( s );
}

UINT8	SetUsbAddress( UINT8 addr )  // 设置USB设备地址
{
	UINT8	s;
	UINT8	BufSetAddr[ sizeof( SetupSetUsbAddr ) ] ;
	memcpy ( BufSetAddr, SetupSetUsbAddr, sizeof( SetupSetUsbAddr ) );
	( (PUSB_SETUP_REQ)BufSetAddr ) -> wValueL = addr;  // USB设备地址
	s = HostCtrlTransfer374( BufSetAddr, NULL, NULL );  // 执行控制传输
	if ( s == USB_INT_SUCCESS ) {
		SetHostUsbAddr( addr );  // 设置USB主机当前操作的USB设备地址
	}
	mDelaymS( 3 );  // 等待USB设备完成操作
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
unsigned char Interrupt_Data_Trans(UINT8 endp_in_addr,PUINT8 p,PUINT8 counter)
{
  UINT8  s,count;
//  static UINT8 tog1;
  s = WaitHostTransact374( endp_in_addr, DEF_USB_PID_IN, tog1, 10 );  // IN数据
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