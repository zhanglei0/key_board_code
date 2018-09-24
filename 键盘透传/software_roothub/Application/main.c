
#include "hw_config.h"
#include "port.h"
#include "usb_regs.h"
#include "usb_lib.h"
#include "usb_prop.h"
#include "usb_pwr.h"

static UINT8 DeviceDescBuf[DEVICE_DESC_LEN];
static UINT8 ConfigDescBuf[CONFIG_DESC_LEN];
static UINT8 HidDescBuf[HID_DESC_LEN];
static UINT8 HidDataBuf[64];


extern const	UINT8	SetupGetDevDescr[] ;
extern UINT8    LOW_SPEED_BIT;
extern UINT8	 tog1;              //读取数据时的同步标志
extern UINT8    endp_out_addr;	    // out端点地址,由于一般鼠标键盘不支持out端点,一般用不到 
extern UINT8    endp_in_addr;		// in 端点地址 
extern UINT8    hid_des_leng;      // HID类报告描述符的长度
extern UINT8    endp_num;          // 数据 hid 类键盘、鼠标的端点数目

PUSB_DEV_DESCR device_desc;
/* used for STM32 USB */
__IO uint8_t PrevXferComplete;
extern uint8_t      Receive_Buffer[20];
extern uint8_t Report_Buf[2];  
extern __IO uint8_t OutPacket_Len;

uint8_t SetReportBuf[64];
UINT8 HIDCount = 0;


#define CTRL_MODE       0x00
#define ALT_MODE        0x01

static uint8_t gMode = CTRL_MODE;

static uint8_t ctrl_buf[8]     = {0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static uint8_t alt_buf[8]      =  {0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static uint8_t ctrl_alt_buf[8] =  {0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static uint8_t num1_alt_buf[8] =  {0x00,0x00,0x1E,0x00,0x00,0x00,0x00,0x00};
static uint8_t num2_alt_buf[8] =  {0x00,0x00,0x1F,0x00,0x00,0x00,0x00,0x00};
static uint8_t num3_alt_buf[8] =  {0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00};
static uint8_t num4_alt_buf[8] =  {0x00,0x00,0x21,0x00,0x00,0x00,0x00,0x00};

static uint8_t alt_ctrl_num1_alt_buf[8] =  {0x05,0x00,0x1E,0x00,0x00,0x00,0x00,0x00};
static uint8_t alt_ctrl_num2_alt_buf[8] =  {0x05,0x00,0x1F,0x00,0x00,0x00,0x00,0x00};
static uint8_t alt_ctrl_num3_alt_buf[8] =  {0x05,0x00,0x20,0x00,0x00,0x00,0x00,0x00};
static uint8_t alt_ctrl_num4_alt_buf[8] =  {0x05,0x00,0x21,0x00,0x00,0x00,0x00,0x00};

static uint8_t alt_ctrl_ctrl_alt_buf[8] =  {0x15,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static uint8_t USB_SBUF_Zero[8]={0,0,0,0,0,0,0,0};
static uint8_t USB_SBUF[8]={0,0,0x04,0,0,0,0,0};

static uint8_t ctrl_times = 0;
static uint8_t alt_times = 0;

static void InitLED(void)
{
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  
  GPIO_ResetBits(GPIOB,GPIO_Pin_8|GPIO_Pin_9);
}

void control_led(uint8_t state)
{
  if(state == 0x00) {
    GPIO_ResetBits(GPIOB,GPIO_Pin_8|GPIO_Pin_9);
  }
  else if(state == 0x01) {
    GPIO_ResetBits(GPIOB,GPIO_Pin_8);
    GPIO_SetBits(GPIOB,GPIO_Pin_9);
  }
  else if(state == 0x02) {
    GPIO_ResetBits(GPIOB,GPIO_Pin_9);
    GPIO_SetBits(GPIOB,GPIO_Pin_8);
  }
  else if(state == 0x03) {
    GPIO_SetBits(GPIOB,GPIO_Pin_9|GPIO_Pin_8);
  }
}

static UINT8 WaitCTRL(void)
{
  uint32_t start_tick = GetTick();
  uint8_t status;
  memset(HidDataBuf,0,8);
  while(start_tick + 3000 > GetTick()) {
    status = Interrupt_Data_Trans(0x80|endp_in_addr,HidDataBuf,&HIDCount);
    if(status == USB_INT_SUCCESS) {
      if(memcmp(HidDataBuf,ctrl_buf,8) == 0) {
        return 0;
      }
    }
  }
  return 1;
}
static UINT8 WaitALT(void)
{
  uint32_t start_tick = GetTick();
  uint8_t status;
  memset(HidDataBuf,0,8);
  while(start_tick + 3000 > GetTick()) {
    status = Interrupt_Data_Trans(0x80|endp_in_addr,HidDataBuf,&HIDCount);
    if(status == USB_INT_SUCCESS) {
      if(memcmp(HidDataBuf,alt_buf,8) == 0) {
        return 0;
      }
    }
  }
  return 1;
}
extern void UserToPMABufferCopy(uint8_t *pbUsrBuf, uint16_t wPMABufAddr, uint16_t wNBytes);
extern void USB_Init(void);
extern uint32_t USB_SIL_Write(uint8_t bEpAddr, uint8_t* pBufferPointer, uint32_t wBufferSize);
static void Keyboard_Send(u8 *Key)
{
    /* Write the descriptor through the endpoint */
  USB_SIL_Write(EP1_IN, (uint8_t*) Key, 8);  
  
  SetEPTxValid(ENDP1);
}


static void HandleUSB_Upload(uint8_t *key)
{
//  if(PrevXferComplete == 1) {
  if(1) {
    PrevXferComplete = 0;
    Keyboard_Send(key);
 //   Keyboard_Send(USB_SBUF_Zero);
  }
}
extern __IO uint8_t OutPacket;
extern  __IO uint32_t bDeviceState; /* USB device status */
void main(void)
{
  uint16_t i;
  UINT8 status;
  InitSystick();
  InitSPI();
  Delay(200);
  Init374Host();
  InitLED();
  Delay(500);
  Set_System();
  USB_Interrupts_Config();
  Set_USBClock();
  USB_Init();
  while(1)
  {
    HostSetBusFree();  // 设定USB主机空闲
    while (1) 
    {
      if (Query374Interrupt())
        HostDetectInterrupt();  // 如果有USB主机中断则处理
      if (Query374DeviceIn())
        break;  // 有USB设备插入
    }
    Delay( 250 );  // 由于USB设备刚插入尚未稳定，故等待USB设备数百毫秒，消除插拔抖动
    if (Query374Interrupt())
      HostDetectInterrupt();  // 如果有USB主机中断则处理
    HostSetBusReset();  // USB总线复位
    for ( i = 0; i < 100; i++ )
    {
      // 等待USB设备复位后重新连接
        if ( Query374DeviceIn())
          break;  // 有USB设备
        Delay(1);
    }
    if (Query374Interrupt())
      HostDetectInterrupt();  // 如果有USB主机中断则处理
    if ( Query374DeviceIn())
    {
      // 有USB设备
      if ( Query374DevFullSpeed()) 
      {
        HostSetFullSpeed( );  // 检测到全速USB设备
      }
      else 
      {  // 建议参考EMB_HUB中的低速例子处理
        HostSetLowSpeed( );  // 检测到低速USB设备,建议参考EMB_HUB中的低速例子处理
      }
    }
    else 
    {
        continue;  // 设备已经断开,继续等待
    }
    Delay(50);
    status = GetDeviceDescr(DeviceDescBuf);  // 获取设备描述符
    if ( status != USB_INT_SUCCESS ) {
       goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
    }
    device_desc = (PUSB_DEV_DESCR)DeviceDescBuf;
    for ( i = 0; i < ( (PUSB_SETUP_REQ)SetupGetDevDescr)->wLengthL; i++ )
      printf( "%02X ", (UINT16)( DeviceDescBuf[i]));
    status = SetUsbAddress( 0x02 );  // 设置USB设备地址
    if ( status != USB_INT_SUCCESS ) {
            goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
    }

    status = GetConfigDescr(ConfigDescBuf);  // 获取配置描述符
    if ( status != USB_INT_SUCCESS ) {
            goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
    }
    for ( i = 0; i < ( (PUSB_CFG_DESCR)ConfigDescBuf)->wTotalLengthL; i++ ) 
      printf( "%02X ", (UINT16)( ConfigDescBuf[i]));
/* 分析配置描述符，获取端点数据/各端点地址/各端点大小等，更新变量endp_addr和endp_size等 */
    
    printf( "SetUsbConfig: " );
    status = SetUsbConfig(((PUSB_CFG_DESCR)ConfigDescBuf) ->bConfigurationValue );  // 设置USB设备配置
    if ( status != USB_INT_SUCCESS ) {
        printf( "ERROR = %02X\n", (UINT16)status);
        goto WaitDeviceOut;  // 终止操作,等待USB设备拔出
    }
    //-------------以下进行HID类的简单操作-----------------------//
    status = Set_Idle( ); //设置IDLE，这个步骤是按照HID类的协议来做的
    if(status != USB_INT_SUCCESS)
    {
      printf("Set_Idle_Err=%02x\n",(unsigned short)status);
      if((status & 0x0f) == USB_INT_RET_STALL)  
        goto next_operate1; //返回STALL可能本身不支持
    }
    else 
      printf("Set_idle success\n");
next_operate1:
    printf("Get Hid Descriptors\n");
    status = Get_Hid_Des(HidDescBuf);  // 获取报告描述符描述符
    if(status == USB_INT_SUCCESS)
    {
      printf("HID_Desc: ");
      for(i=0;i < hid_des_leng;i++)
        printf("%02x ",(unsigned short)HidDescBuf[i]);
      printf("\n");
    }
    else
    {
      goto WaitDeviceOut;                             //出错退出
    }
    printf("Set_Report \n"); //对于键盘发Set_Report来点亮灯,对于鼠标则不需要这一步
//    SetReportBuf[0]=0x01;
//    status =Set_Report(SetReportBuf); //设置报表
//    if(status == USB_INT_SUCCESS)   
//    {
//      printf("Set_Report success\n");
//    }
//    else
//    {
//      printf("Set_Report Err=%02x\n",(unsigned short)status);      //设置报告出错
//      if((status & 0x0f) == USB_INT_RET_STALL)  
//        goto next_operate2;      //返回STALL可能本身不支持		  
//    }
next_operate2:
// 下面开始读取数据 ( 实际在读取数据的时候，要先发送中断端点的令牌来读取数据，接着才能获取到数据 )
    tog1=FALSE;                                  //开始取DATA0
    uint32_t start_tick = GetTick();
    control_led(3);
    while(1)
    {
      if(OutPacket_Len > 0) {
        Set_Report(Receive_Buffer);
        OutPacket_Len = 0;
      }
      status = Interrupt_Data_Trans(0x80|endp_in_addr,HidDataBuf,&HIDCount);
      if(status == USB_INT_SUCCESS)
      {
        HandleUSB_Upload(HidDataBuf);
#if 1
        if(memcmp(USB_SBUF_Zero,HidDataBuf,8) == 0) {
        }
        else {
          if(gMode == CTRL_MODE) {
            if(ctrl_times == 0) {
              if(memcmp(HidDataBuf,ctrl_buf,8) == 0)
                ctrl_times = 1;
            }
            else if(ctrl_times == 1) {
              if(memcmp(HidDataBuf,ctrl_buf,8) == 0) 
                ctrl_times = 2;
              else 
                ctrl_times = 0;
            }
            else if(ctrl_times == 2) {
              if(memcmp(HidDataBuf,num1_alt_buf,8) == 0) {
                control_led(0);
              }
              else if(memcmp(HidDataBuf,num2_alt_buf,8) == 0) {
                control_led(1);
              }
              else if(memcmp(HidDataBuf,num3_alt_buf,8) == 0) {
                control_led(2);
              }
              else if(memcmp(HidDataBuf,num4_alt_buf,8) == 0) {
                control_led(3);
              }
              else if(memcmp(HidDataBuf,alt_buf,8) == 0) {
                alt_times  = 0;
                ctrl_times = 0;
                gMode = ALT_MODE;
              }
              if(memcmp(HidDataBuf,ctrl_buf,8) == 0) {
                ctrl_times = 1;
              }
              else 
                ctrl_times = 0;
            }
          }
          else if(gMode == ALT_MODE) {
            if(memcmp(HidDataBuf,alt_ctrl_num1_alt_buf,8) == 0) {
              control_led(0);
            }
            if(memcmp(HidDataBuf,alt_ctrl_num2_alt_buf,8) == 0) {
              control_led(1);
            }
            if(memcmp(HidDataBuf,alt_ctrl_num3_alt_buf,8) == 0) {
              control_led(2);
            }
            if(memcmp(HidDataBuf,alt_ctrl_num4_alt_buf,8) == 0) {
              control_led(3);
            }
            if(memcmp(HidDataBuf,alt_ctrl_ctrl_alt_buf,8) == 0) {
              ctrl_times = 0;
              alt_times  = 0;
              gMode = CTRL_MODE;
            }
          }
        }
#endif
      }
      else if(status == USB_INT_DISCONNECT)            //  这个是为了知道设备拔出产生的中断状态
      {
        break;
      }
    }
    /* Now the device is ready */
WaitDeviceOut:  // 等待USB设备拔出
    printf("Wait Device Out\n");
    while (1) {
            if ( Query374Interrupt()) HostDetectInterrupt( );  // 如果有USB主机中断则处理
            if ( Query374DeviceIn( ) == FALSE ) break;  // 没有USB设备
    }
    Delay( 100 );  // 等待设备完全断开，消除插拔抖动
    if ( Query374DeviceIn( ) ) goto WaitDeviceOut;  // 没有完全断开
  }
}

/* if device is HID, then return 1 */
uint8_t handle_usbDeviceDesc(PUSB_DEV_DESCR pbuf)
{
  if(pbuf->bDeviceClass == 0x00 && pbuf->bDeviceSubClass == 0x00)
    return 1;
  else return 0;
}

UINT8 analysis_configDesc(PUINT8 buf)
{
  PUSB_CFG_DESCR desc = (PUSB_CFG_DESCR)buf ;
  UINT16 wTotalLength = (desc->wTotalLengthL) + ((UINT16)(desc->wTotalLengthH)) * 256;
  UINT16 i;
  UINT8 length,type;
  for(i = 0; i < wTotalLength;) {
    length = buf[i];
    type = buf[i + 1];
    if(type == HID_DESCRIPTOR_TYPE) {
      
    }
    else if(type == USB_ENDPOINT_DESCRIPTOR_TYPE) {
    }
    i = i + length;
  }
  return 0;
}

int fputc(int ch,FILE *f)
{
  return ch;
}