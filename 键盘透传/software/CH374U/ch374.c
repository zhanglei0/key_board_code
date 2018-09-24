
#include <rthw.h>
#include "port.h"



// ��ȡ�豸������
const	UINT8	SetupGetDevDescr[] = { 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00 };
// ��ȡ����������
const	UINT8	SetupGetCfgDescr[] = { 0x80, 0x06, 0x00, 0x02, 0x00, 0x00, 0x04, 0x00 };
// ����USB��ַ
const	UINT8	SetupSetUsbAddr[] = { 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
// ����USB����
const	UINT8	SetupSetUsbConfig[] = { 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* SET IDLE */
const unsigned char  SetupSetidle[]={0x21,0x0a,0x00,0x00,0x00,0x00,0x00,0x00};        
/* ��ȡHID ���������� */
unsigned char  SetupGetHidDes[]={0x81,0x06,0x00,0x22,0x00,0x00,0x81,0x00};    
/* SET REPORT */
unsigned char  SetupSetReport[]={0x21,0x09,0x00,0x02,0x00,0x00,0x01,0x00};     

UINT8	UsbDevEndpSize = DEFAULT_ENDP0_SIZE;	/* USB�豸�Ķ˵�0�������ߴ� */

UINT8	FlagDeviceStatus;						/* ��ǰUSB�豸״̬��ͨ�������жϷ�ʽ��ȫ�ֱ�������������δʹ�� */


UINT8	GetDeviceDescr( PUINT8 buf );  // ��ȡ�豸������

UINT8	GetConfigDescr( PUINT8 buf );  // ��ȡ����������

UINT8	SetUsbAddress( UINT8 addr );  // ����USB�豸��ַ

UINT8	SetUsbConfig( UINT8 cfg );  // ����USB�豸����




UINT8    LOW_SPEED_BIT;
UINT8	 tog1;              //��ȡ����ʱ��ͬ����־
UINT8    endp_out_addr;	    // out�˵��ַ,����һ�������̲�֧��out�˵�,һ���ò��� 
UINT8    endp_in_addr;		// in �˵��ַ 
UINT8    hid_des_leng;      // HID�౨���������ĳ���
UINT8    endp_num;          // ���� hid ����̡����Ķ˵���Ŀ
UINT8	 FlagDeviceStatus;	// ��ǰUSB�豸״̬��ͨ�������жϷ�ʽ��ȫ�ֱ�������������δʹ�� 

_RootHubDev RootHubDev[3];
/* ��ʱָ��΢��ʱ��,���ݵ�Ƭ����Ƶ����,����ȷ */
void	mDelayuS( UINT8 us )
{
  for(uint8_t i = 0; i < 36;i++) {
    for(uint8_t j = 0; j < us;j++)
    {}
  }
}

/* ��ʱָ������ʱ�� */
void	mDelaymS( UINT8 ms )
{
	Delay(ms);
}

// ��ѯCH374�ж�(INT#�͵�ƽ)
BOOL	Query374Interrupt( void )
{
  return( CH374_INT_WIRE ? FALSE : TRUE );  /* ���������CH374���ж�������ֱ�Ӳ�ѯ�ж����� */
}
// �ȴ�CH374�ж�(INT#�͵�ƽ)����ʱ�򷵻�ERR_USB_UNKNOWN
UINT8	Wait374Interrupt( void )
{
	UINT16	i;
	for ( i = 0; i < 10000; i ++ ) {  // ������ֹ��ʱ
		if ( Query374Interrupt( ) ) return( 0 );
	}
	return( ERR_USB_UNKNOWN );  // ��Ӧ�÷��������
}

// CH374������������Ŀ�Ķ˵��ַ/PID����/ͬ����־������ͬCH375��NAK�����ԣ���ʱ/��������
UINT8	HostTransact374( UINT8 endp_addr, UINT8 pid, BOOL tog )
{  // ���ӳ��������������,����ʵ��Ӧ����,Ϊ���ṩ�����ٶ�,Ӧ�öԱ��ӳ����������Ż�
	UINT8	retry;
	UINT8	s, r, u;
	for ( retry = 0; retry < 3; retry ++ ) {
		Write374Byte( REG_USB_H_PID, M_MK_HOST_PID_ENDP( pid, endp_addr ) );  // ָ������PID��Ŀ�Ķ˵��
//		Write374Byte( REG_USB_H_CTRL, BIT_HOST_START | ( tog ? ( BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : 0x00 ) );  // ����ͬ����־����������
		Write374Byte( REG_USB_H_CTRL, ( tog ? ( BIT_HOST_START | BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : BIT_HOST_START ) );  // ����ͬ����־����������
//		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE );  // ȡ����ͣ
		s = Wait374Interrupt( );
		if ( s == ERR_USB_UNKNOWN ) return( s );  // �жϳ�ʱ,������Ӳ���쳣
		s = Read374Byte( REG_INTER_FLAG );  // ��ȡ�ж�״̬
		if ( s & BIT_IF_DEV_DETECT ) {  // USB�豸����¼�
			mDelayuS( 200 );  // �ȴ��������
			Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_TRANSFER );  // ���жϱ�־
			if ( s & BIT_IF_DEV_ATTACH ) {  // USB�豸�����¼�
				u = Read374Byte( REG_USB_SETUP );
				if ( s & BIT_IF_USB_DX_IN ) {  // �ٶ�ƥ�䣬����Ҫ�л��ٶ�
					if ( u & BIT_SETP_USB_SPEED ) return( USB_INT_CONNECT_LS );  // ����USB�豸
					return( USB_INT_CONNECT );  // ȫ��USB�豸
				}
				else {  // �ٶ�ʧ�䣬��Ҫ�л��ٶ�
					if ( u & BIT_SETP_USB_SPEED ) return( USB_INT_CONNECT );  // ȫ��USB�豸
					return( USB_INT_CONNECT_LS );  // ����USB�豸
				}
			}
			else return( USB_INT_DISCONNECT );  // USB�豸�Ͽ��¼�
		}
		else if ( s & BIT_IF_TRANSFER ) {  // �������
			Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_TRANSFER );  // ���жϱ�־
			s = Read374Byte( REG_USB_STATUS );  // USB״̬
			r = s & BIT_STAT_DEV_RESP;  // USB�豸Ӧ��״̬
			switch ( pid ) {
				case DEF_USB_PID_SETUP:
				case DEF_USB_PID_OUT:
					if ( r == DEF_USB_PID_ACK ) return( USB_INT_SUCCESS );
					else if ( r == DEF_USB_PID_STALL || r == DEF_USB_PID_NAK ) return( r | 0x20 );
					else if ( ! M_IS_HOST_TIMEOUT( s ) ) return( r | 0x20 );  // ���ǳ�ʱ/��������Ӧ��
					break;
				case DEF_USB_PID_IN:
					if ( M_IS_HOST_IN_DATA( s ) ) {  // DEF_USB_PID_DATA0 or DEF_USB_PID_DATA1
						if ( s & BIT_STAT_TOG_MATCH ) return( USB_INT_SUCCESS );  // ��ͬ�����趪��������
					}
					else if ( r == DEF_USB_PID_STALL || r == DEF_USB_PID_NAK ) return( r | 0x20 );
					else if ( ! M_IS_HOST_TIMEOUT( s ) ) return( r | 0x20 );  // ���ǳ�ʱ/��������Ӧ��
					break;
				default:
					return( ERR_USB_UNKNOWN );  // �����ܵ����
					break;
			}
		}
		else {  // �����ж�,��Ӧ�÷��������
			mDelayuS( 200 );  // �ȴ��������
			Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  /* ���жϱ�־ */
			if ( retry ) return( ERR_USB_UNKNOWN );  /* ���ǵ�һ�μ�⵽�򷵻ش��� */
		}
	}
	return( 0x20 );  // Ӧ��ʱ
}

// CH374������������Ŀ�Ķ˵��ַ/PID����/ͬ����־/��mSΪ��λ��NAK������ʱ��(0xFFFF��������)������ͬCH375��NAK���ԣ���ʱ��������
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

UINT8	HostCtrlTransfer374( PUINT8 ReqBuf, PUINT8 DatBuf, PUINT8 RetLen )  // ִ�п��ƴ���,ReqBufָ��8�ֽ�������,DatBufΪ�շ�������
// �����Ҫ���պͷ������ݣ���ôDatBuf��ָ����Ч���������ڴ�ź������ݣ�ʵ�ʳɹ��շ����ܳ��ȱ�����ReqLenָ����ֽڱ�����
{
	UINT8	s, len, count, total;
	BOOL	tog;
	Write374Block( RAM_HOST_TRAN, 8, ReqBuf );
	Write374Byte( REG_USB_LENGTH, 8 );
	mDelayuS( 100 );
	s = WaitHostTransact374( 0, DEF_USB_PID_SETUP, FALSE, 200 );  // SETUP�׶Σ�200mS��ʱ
	if ( s == USB_INT_SUCCESS ) {  // SETUP�ɹ�
		tog = TRUE;  // Ĭ��DATA1,Ĭ�������ݹ�״̬�׶�ΪIN
		total = *( ReqBuf + 6 );
		if ( total && DatBuf ) {  // ��Ҫ�շ�����
			len = total;
			if ( *ReqBuf & 0x80 ) {  // ��
				while ( len ) {
					mDelayuS( 100 );
					s = WaitHostTransact374( 0, DEF_USB_PID_IN, tog, 200 );  // IN����
					if ( s != USB_INT_SUCCESS ) break;
					count = Read374Byte( REG_USB_LENGTH );
					Read374Block( RAM_HOST_RECV, count, DatBuf );
					DatBuf += count;
					if ( count <= len ) len -= count;
					else len = 0;
					if ( count == 0 || ( count & ( UsbDevEndpSize - 1 ) ) ) break;  // �̰�
					tog = tog ? FALSE : TRUE;
				}
				tog = FALSE;  // ״̬�׶�ΪOUT
			}
			else {  // ��
				while ( len ) {
					mDelayuS( 100 );
					count = len >= UsbDevEndpSize ? UsbDevEndpSize : len;
					Write374Block( RAM_HOST_TRAN, count, DatBuf );
					Write374Byte( REG_USB_LENGTH, count );
					s = WaitHostTransact374( 0, DEF_USB_PID_OUT, tog, 200 );  // OUT����
					if ( s != USB_INT_SUCCESS ) break;
					DatBuf += count;
					len -= count;
					tog = tog ? FALSE : TRUE;
				}
				tog = TRUE;  // ״̬�׶�ΪIN
			}
			total -= len;  // ��ȥʣ�೤�ȵ�ʵ�ʴ��䳤��
		}
		if ( s == USB_INT_SUCCESS ) {  // ���ݽ׶γɹ�
			Write374Byte( REG_USB_LENGTH, 0 );
			mDelayuS( 100 );
			s = WaitHostTransact374( 0, ( tog ? DEF_USB_PID_IN : DEF_USB_PID_OUT ), TRUE, 200 );  // STATUS�׶�
			if ( tog && s == USB_INT_SUCCESS ) {  // ���IN״̬�������ݳ���
				if ( Read374Byte( REG_USB_LENGTH ) ) s = USB_INT_BUF_OVER;  // ״̬�׶δ���
			}
		}
	}
	if ( RetLen ) *RetLen = total;  // ʵ�ʳɹ��շ����ܳ���
	return( s );
}

// ��ѯ��ǰ�Ƿ����USB�豸
//BOOL	Query374DeviceIn( void )
//#define	Query374DeviceIn( )	( ( Read374Byte( REG_HUB_SETUP ) & BIT_HUB0_ATTACH ) ? TRUE : FALSE )

// ��ѯ��ǰ��USB�豸��ȫ�ٻ��ǵ���, ����TRUEΪȫ��
//BOOL	Query374DevFullSpeed( void )
//#define	Query374DevFullSpeed( )	( ( Read374Byte( REG_SYS_INFO ) & BIT_INFO_USB_DP ) ? TRUE : FALSE )

void	HostDetectInterrupt( void )  // ����USB�豸����¼��ж�
{
  UINT8	s, u;
  s = Read374Byte( REG_INTER_FLAG );  // ��ȡ�ж�״̬
  if (s & BIT_IF_DEV_DETECT )
  {
    // USB�豸����¼�
    Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT );  // ���жϱ�־
    if ( s & BIT_IF_DEV_ATTACH ) 
    {  // USB�豸�����¼�
      u = Read374Byte( REG_USB_SETUP );
      if ( s & BIT_IF_USB_DX_IN ) 
      {  // �ٶ�ƥ�䣬����Ҫ�л��ٶ�
        if ( u & BIT_SETP_USB_SPEED ) 
          FlagDeviceStatus = USB_INT_CONNECT_LS;  // ����USB�豸
        else 
          FlagDeviceStatus = USB_INT_CONNECT;  // ȫ��USB�豸
      }
      else
      {  // �ٶ�ʧ�䣬��Ҫ�л��ٶ�
        if ( u & BIT_SETP_USB_SPEED )
          FlagDeviceStatus = USB_INT_CONNECT;  // ȫ��USB�豸
        else 
          FlagDeviceStatus = USB_INT_CONNECT_LS;  // ����USB�豸
      }
    }
    else 
      FlagDeviceStatus = USB_INT_DISCONNECT;  // USB�豸�Ͽ��¼�
  }
  else 
  {  // ������ж�
    Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_TRANSFER | BIT_IF_USB_SUSPEND | BIT_IF_WAKE_UP );  // ���жϱ�־
  }
}

void	SetHostUsbAddr( UINT8 addr )  // ����USB������ǰ������USB�豸��ַ
{
  Write374Byte( REG_USB_ADDR, addr );
}

void	HostSetBusFree( void )  // USB���߿���
{
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE );  // USB������ʽ
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // USB������ʽ,����SOF
	Write374Byte( REG_HUB_SETUP, 0x00 );  // ��BIT_HUB_DISABLE,�������õ�ROOT-HUB
	Write374Byte( REG_HUB_CTRL, 0x00 );  // ���ROOT-HUB��Ϣ
}

void	HostSetBusReset( void )  // USB���߸�λ
{
	UsbDevEndpSize = DEFAULT_ENDP0_SIZE;  /* USB�豸�Ķ˵�0�������ߴ� */
	SetHostUsbAddr( 0x00 );
	Write374Byte( REG_USB_H_CTRL, 0x00 );
	Write374Byte( REG_HUB_SETUP, BIT_HUB0_RESET );  // Ĭ��Ϊȫ��,��ʼ��λ
	mDelaymS( 15 );  // ��λʱ��10mS��20mS
	Write374Byte( REG_HUB_SETUP, 0x00 );  // ������λ
	mDelaymS( 1 );
	Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_USB_SUSPEND );  // ���жϱ�־
}

void	HostSetFullSpeed( void )  // �趨ȫ��USB�豸���л���
{
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // ȫ���ҷ�SOF
	Write374Byte( REG_HUB_SETUP, BIT_HUB0_EN );  // ʹ��HUB0�˿�
}

void	HostSetLowSpeed( void )  // �趨����USB�豸���л���
{
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF | BIT_SETP_LOW_SPEED );  // �����ҷ�SOF
	Write374Byte( REG_HUB_SETUP, BIT_HUB0_EN | BIT_HUB0_POLAR );  // ʹ��HUB0�˿�
}

void	Init374Host( void )  // ��ʼ��USB����
{
        Read374Byte(REG_SYS_INFO);
        Read374Byte(REG_USB_SETUP);
	SetHostUsbAddr( 0x00 );
        Write374Byte( REG_HUB_SETUP, 0x00 );   //Enable HUB
	Write374Byte( REG_USB_H_CTRL, 0x00 );
	Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  // �������жϱ�־
//	Write374Byte( REG_INTER_EN, BIT_IE_TRANSFER );  // ����������ж�,��Ϊ������ʹ�ò�ѯ��ʽ���USB�豸���,��������USB�豸����ж�
	Write374Byte( REG_INTER_EN, BIT_IE_TRANSFER | BIT_IE_DEV_DETECT );  // ����������жϺ�USB�豸����ж�
	Write374Byte( REG_SYS_CTRL, BIT_CTRL_OE_POLAR );  // ����CH374T����UEN�������յ�CH374S������BIT_CTRL_OE_POLARΪ1
	Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // ȫ���ҷ�SOF
        HostSetBusFree();  // USB���߿���
}

UINT8	GetDeviceDescr( PUINT8 buf )  // ��ȡ�豸������
{
	UINT8	s, len;
	s = HostCtrlTransfer374( (PUINT8)SetupGetDevDescr, buf, &len );  // ִ�п��ƴ���
	if ( s == USB_INT_SUCCESS ) {
		UsbDevEndpSize = ( (PUSB_DEV_DESCR)buf ) -> bMaxPacketSize0;  // �˵�0��������,���Ǽ򻯴���,����Ӧ���Ȼ�ȡǰ8�ֽں���������UsbDevEndpSize�ټ���
		if ( len < ( (PUSB_SETUP_REQ)SetupGetDevDescr ) -> wLengthL ) s = USB_INT_BUF_OVER;  // ���������ȴ���
	}
	return( s );
}

UINT8 GetConfigDescr( PUINT8 buf )  // ��ȡ����������
{
  UINT8	s, len,i,c,j;
  UINT8	BufLogDescr[ sizeof( SetupGetCfgDescr ) ] ;
  s = HostCtrlTransfer374( (PUINT8)SetupGetCfgDescr, buf, &len );  // ִ�п��ƴ���
  if ( s == USB_INT_SUCCESS ) 
  {
    if ( len < ( (PUSB_SETUP_REQ)SetupGetCfgDescr )->wLengthL )
      s = USB_INT_BUF_OVER;  // ���س��ȴ���
    else 
    {
      memcpy ( BufLogDescr, SetupGetCfgDescr, sizeof( SetupGetCfgDescr ) );
      ( (PUSB_SETUP_REQ)BufLogDescr )->wLengthL = ( (PUSB_CFG_DESCR)buf )->wTotalLengthL;  // �����������������ܳ���
      s = HostCtrlTransfer374( BufLogDescr, buf, &len );  // ִ�п��ƴ���
      if ( s == USB_INT_SUCCESS ) 
      {
        if ( len < ( (PUSB_CFG_DESCR)buf ) -> wTotalLengthL )
          s = USB_INT_BUF_OVER;  // ���������ȴ���
        else 
        {
          for(i=0;i<( (PUSB_CFG_DESCR)buf ) -> wTotalLengthL;i++) 
          {
              if((buf[i]==0x09)&&(buf[i+1]==0x21)&&(buf[i+6]==0x22))  
                hid_des_leng = buf[i+7];    //��ȡ�����������ĳ���
          }
          printf("hid_des_leng=%02x\n",(unsigned short)hid_des_leng);
          endp_out_addr = endp_in_addr = 0;
          endp_num = 0 ;
          for(i=0;i<( (PUSB_CFG_DESCR)buf )->wTotalLengthL;i++) 
          {
            if((buf[i]==0x09) && (buf[i+1]==0x04) &&(buf[i+5]==0x03)
               &&(buf[i+7]==0x01) || (buf[i+7] ==0x02 ))  //�ӿ�������ΪHID����ꡢ����   
            {
              for(j=0;j<( (PUSB_CFG_DESCR)buf ) -> wTotalLengthL-i;j++) 
              {
                if((buf[i+j] == 0x07) && (buf[i+j+1] == 0x05) && (buf[i+j+3] == 0x03)) {
                    c = buf[i+j+2];        //�ж��Ƿ�Ϊ�ж϶˵�
                  if ( c & 0x80 )  
                    endp_in_addr = c & 0x0f;         // IN�˵�ĵ�ַ                                
                  else
                  {
                    endp_out_addr = c & 0x0f;         // OUT�˵� 
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

UINT8	SetUsbAddress( UINT8 addr )  // ����USB�豸��ַ
{
	UINT8	s;
	UINT8	BufSetAddr[ sizeof( SetupSetUsbAddr ) ] ;
	memcpy ( BufSetAddr, SetupSetUsbAddr, sizeof( SetupSetUsbAddr ) );
	( (PUSB_SETUP_REQ)BufSetAddr ) -> wValueL = addr;  // USB�豸��ַ
	s = HostCtrlTransfer374( BufSetAddr, NULL, NULL );  // ִ�п��ƴ���
	if ( s == USB_INT_SUCCESS ) {
		SetHostUsbAddr( addr );  // ����USB������ǰ������USB�豸��ַ
	}
	mDelaymS( 3 );  // �ȴ�USB�豸��ɲ���
	return( s );
}

UINT8	SetUsbConfig( UINT8 cfg )  // ����USB�豸����
{
  UINT8	BufSetCfg[ sizeof( SetupSetUsbConfig ) ] ;
  memcpy ( BufSetCfg, SetupSetUsbConfig, sizeof( SetupSetUsbConfig ) );
  ( (PUSB_SETUP_REQ)BufSetCfg ) -> wValueL = cfg;  // USB�豸����
  return( HostCtrlTransfer374( BufSetCfg, NULL, NULL ) );  // ִ�п��ƴ���
  }



/* ����Idle */
UINT8  Set_Idle(void)
{
  UINT8  s;
  s=HostCtrlTransfer374((PUINT8)SetupSetidle,NULL,NULL);
  return s;
}

/* ��ȡ���������� */
UINT8  Get_Hid_Des(unsigned char *p)
{
  UINT8  s;
  //leng=SetupGetHidDes[0x06]-0x40;   //�����������ĳ����ڷ������ݳ��ȵĻ����ϼ�ȥ0X40
  s=HostCtrlTransfer374((PUINT8)SetupGetHidDes,p,(PUINT8)&SetupGetHidDes[0x06]);
  return s;
}


/* ���ñ��� */
UINT8  Set_Report(unsigned char *p)
{
  UINT8  s,l=1;
  s=HostCtrlTransfer374((PUINT8)SetupSetReport,p,&l); //ʵ�ʵ����ݿ���д�������
  return s;
}

/*ͨ���ж϶˵��ȡ��ꡢ�����ϴ������� */
unsigned char Interrupt_Data_Trans(UINT8 endp_in_addr,PUINT8 p,PUINT8 counter)
{
  UINT8  s,count;
//  static UINT8 tog1;
  s = WaitHostTransact374( endp_in_addr, DEF_USB_PID_IN, tog1, 10 );  // IN����
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
void HostEnableRootHub( void )  // �������õ�Root-HUB
{
  Write374Byte( REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // USB������ʽ,����SOF
  Write374Byte( REG_HUB_SETUP, 0x00 );  // ��BIT_HUB_DISABLE,�������õ�ROOT-HUB
  Write374Byte( REG_HUB_CTRL, 0x00 );  // ���ROOT-HUB��Ϣ
}