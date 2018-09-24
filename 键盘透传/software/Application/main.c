
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
extern UINT8	 tog1;              //��ȡ����ʱ��ͬ����־
extern UINT8    endp_out_addr;	    // out�˵��ַ,����һ�������̲�֧��out�˵�,һ���ò��� 
extern UINT8    endp_in_addr;		// in �˵��ַ 
extern UINT8    hid_des_leng;      // HID�౨���������ĳ���
extern UINT8    endp_num;          // ���� hid ����̡����Ķ˵���Ŀ

PUSB_DEV_DESCR device_desc;
/* used for STM32 USB */
__IO uint8_t PrevXferComplete;
extern uint8_t      Receive_Buffer[20];
extern uint8_t Report_Buf[2];  
extern __IO uint8_t OutPacket_Len;
extern UINT8	TempBuf[64];
extern _RootHubDev RootHubDev[3];
extern _DevOnHubPort DevOnHubPort[3][4];

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
  
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_8;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_ResetBits(GPIOA,GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_8);
}
void Light_SingleLED(uint8_t pos)
{
  if( pos <= 3)
  {
    GPIO_SetBits(GPIOA,GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_8);
    switch(pos)
    {
    case 0:
      GPIO_ResetBits(GPIOA,GPIO_Pin_0);
      break;
    case 1:
      GPIO_ResetBits(GPIOA,GPIO_Pin_1);
      break;
    case 2:
      GPIO_ResetBits(GPIOA,GPIO_Pin_2);
      break;
    case 3:
      GPIO_ResetBits(GPIOA,GPIO_Pin_8);
      break;
    }
  }
}
void control_led(uint8_t state)
{
  if(state == 0x00) {
    GPIO_ResetBits(GPIOB,GPIO_Pin_8|GPIO_Pin_9);
    Light_SingleLED(0);
  }
  else if(state == 0x01) {
    GPIO_ResetBits(GPIOB,GPIO_Pin_8);
    GPIO_SetBits(GPIOB,GPIO_Pin_9);
    Light_SingleLED(1);
  }
  else if(state == 0x02) {
    GPIO_ResetBits(GPIOB,GPIO_Pin_9);
    GPIO_SetBits(GPIOB,GPIO_Pin_8);
    Light_SingleLED(2);
  }
  else if(state == 0x03) {
    GPIO_SetBits(GPIOB,GPIO_Pin_9|GPIO_Pin_8);
    Light_SingleLED(3);
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
extern UINT8	NewDevCount;

void ProcessMiscDev(void)
{
  uint8_t key_press;
  key_press = ScanKeyBoard();
  HandleUARTEvt();
  if(key_press > 0 && key_press < 5)
  {
    control_led(key_press - 1);
    Beep(100);
  }
}


static void HandleMisc(void)
{
    HandleUARTEvt();
    uint8_t key = ScanKeyBoard();
    if(key > 0 && key < 5)
    {
      Beep(100);
      control_led(key - 1);
    }
}
void main(void)
{
  uint16_t i,n;
  UINT8 status,s;
  UINT8 count = 0;
  UINT16	loc;
  UINT8   in_endpoint;
  UINT8 bootmode = 0;
  uint8_t key;
  uint8_t keyboard_key;
  InitSystick();
  InitSPI();
  Delay(1000);
  Init374Host();
  HostEnableRootHub();  // �������õ�Root-HUB
  bootmode = GetFunctionSel();
  InitLED();
  InitDetectPin();
  InitKey();
  InitBeep();
  InitUart();
  Beep(100);
  Set_System();
  USB_Interrupts_Config();
  Set_USBClock();
  USB_Init();
  while(1)
  {
    HostSetBusFree();  // �趨USB��������
    while(1) {
      HandleMisc();
      if (Query374Interrupt())
        HostDetectInterrupt();  // �����USB�����ж�����
      if (Query374DeviceIn())
        break;  // ��USB�豸����
    }
    Delay( 250 );  // ����USB�豸�ղ�����δ�ȶ����ʵȴ�USB�豸���ٺ��룬������ζ���
    if (Query374Interrupt())
      HostDetectInterrupt();  // �����USB�����ж�����
    HostSetBusReset();  // USB���߸�λ
    for ( i = 0; i < 100; i++ )
    {
      // �ȴ�USB�豸��λ����������
        if ( Query374DeviceIn())
          break;  // ��USB�豸
        Delay(1);
    }
    if (Query374Interrupt())
      HostDetectInterrupt();  // �����USB�����ж�����
    if ( Query374DeviceIn())
    {
      // ��USB�豸
      if ( Query374DevFullSpeed()) 
      {
        HostSetFullSpeed( );  // ��⵽ȫ��USB�豸
      }
      else 
      {  // ����ο�EMB_HUB�еĵ������Ӵ���
        HostSetLowSpeed( );  // ��⵽����USB�豸,����ο�EMB_HUB�еĵ������Ӵ���
      }
    }
    else 
    {
        continue;  // �豸�Ѿ��Ͽ�,�����ȴ�
    }
        Delay(50);
    status = GetDeviceDescr(DeviceDescBuf);  // ��ȡ�豸������
    if ( status != USB_INT_SUCCESS ) {
       goto WaitDeviceOut;  // ��ֹ����,�ȴ�USB�豸�γ�
    }
    device_desc = (PUSB_DEV_DESCR)DeviceDescBuf;
    for ( i = 0; i < ( (PUSB_SETUP_REQ)SetupGetDevDescr)->wLengthL; i++ )
      printf( "%02X ", (UINT16)( DeviceDescBuf[i]));
    status = SetUsbAddress( 0x02 );  // ����USB�豸��ַ
    if ( status != USB_INT_SUCCESS ) {
            goto WaitDeviceOut;  // ��ֹ����,�ȴ�USB�豸�γ�
    }

    status = GetConfigDescr(ConfigDescBuf);  // ��ȡ����������
    if ( status != USB_INT_SUCCESS ) {
            goto WaitDeviceOut;  // ��ֹ����,�ȴ�USB�豸�γ�
    }
    for ( i = 0; i < ( (PUSB_CFG_DESCR)ConfigDescBuf)->wTotalLengthL; i++ ) 
      printf( "%02X ", (UINT16)( ConfigDescBuf[i]));
/* ������������������ȡ�˵�����/���˵��ַ/���˵��С�ȣ����±���endp_addr��endp_size�� */
    
    printf( "SetUsbConfig: " );
    status = SetUsbConfig(((PUSB_CFG_DESCR)ConfigDescBuf) ->bConfigurationValue );  // ����USB�豸����
    if ( status != USB_INT_SUCCESS ) {
        printf( "ERROR = %02X\n", (UINT16)status);
        goto WaitDeviceOut;  // ��ֹ����,�ȴ�USB�豸�γ�
    }
    //-------------���½���HID��ļ򵥲���-----------------------//
    status = Set_Idle( ); //����IDLE����������ǰ���HID���Э��������
    if(status != USB_INT_SUCCESS)
    {
      printf("Set_Idle_Err=%02x\n",(unsigned short)status);
      if((status & 0x0f) == USB_INT_RET_STALL)  
        goto next_operate1; //����STALL���ܱ���֧��
    }
    else 
      printf("Set_idle success\n");
next_operate1:
    printf("Get Hid Descriptors\n");
    status = Get_Hid_Des(HidDescBuf);  // ��ȡ����������������
    if(status == USB_INT_SUCCESS)
    {
      printf("HID_Desc: ");
      for(i=0;i < hid_des_leng;i++)
        printf("%02x ",(unsigned short)HidDescBuf[i]);
      printf("\n");
    }
    else
    {
      goto WaitDeviceOut;                             //�����˳�
    }
    printf("Set_Report \n"); //���ڼ��̷�Set_Report��������,�����������Ҫ��һ��
    SetReportBuf[0]=0x01;
    status =Set_Report(SetReportBuf); //���ñ���
    if(status == USB_INT_SUCCESS)   
    {
      printf("Set_Report success\n");
    }
    else
    {
      printf("Set_Report Err=%02x\n",(unsigned short)status);      //���ñ������
      if((status & 0x0f) == USB_INT_RET_STALL)  
        goto next_operate2;      //����STALL���ܱ���֧��		  
    }
next_operate2:
// ���濪ʼ��ȡ���� ( ʵ���ڶ�ȡ���ݵ�ʱ��Ҫ�ȷ����ж϶˵����������ȡ���ݣ����Ų��ܻ�ȡ������ )
    tog1=FALSE;                                  //��ʼȡDATA0
    while(1)
    {
      status = Interrupt_Data_Trans(0x80|endp_in_addr,HidDataBuf,&HIDCount);
      if(status == USB_INT_SUCCESS)
      {
        printf("%02x",HidDataBuf[0]);
      }
      HandleMisc();
    }
        /* Now the device is ready */
WaitDeviceOut:  // �ȴ�USB�豸�γ�
    printf("Wait Device Out\n");
    while (1) {
      ProcessMiscDev();
      if ( Query374Interrupt()) HostDetectInterrupt( );  // �����USB�����ж�����
      if ( Query374DeviceIn( ) == FALSE ) break;  // û��USB�豸
    }
    Delay( 100 );  // �ȴ��豸��ȫ�Ͽ���������ζ���
    if ( Query374DeviceIn( ) ) goto WaitDeviceOut;  // û����ȫ�Ͽ�
  }
}


int fputc(int ch,FILE *f)
{
//  USART_SendData(USART1, ch);
//    
//  /* Loop until USARTy DR register is empty */ 
//  while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
//  {
//  }
  return ch;
}