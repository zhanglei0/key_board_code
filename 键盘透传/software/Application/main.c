
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
  Beep(500);
  Set_System();
  USB_Interrupts_Config();
  Set_USBClock();
  USB_Init();
  for (uint8_t  n = 0; n < 3; n ++ ) 
    RootHubDev[n].DeviceStatus = ROOT_DEV_DISCONNECT;  // ���
  HostSetBusFree();  // �趨USB��������
  while(1)
  {
    HandleUARTEvt();
    key = ScanKeyBoard();
    
    if(key > 0 && key < 5)
    {
      Beep(100);
      control_led(key - 1);
    }
    if ( Query374Interrupt())  {
      HostDetectInterrupt();  // �����USB�����ж�����
    }
    if ( NewDevCount ) {
      mDelaymS( 200 );
      NewDevCount=0;
      for ( n = 0; n < 3; n ++ ) {
        if ( RootHubDev[n].DeviceStatus == ROOT_DEV_CONNECTED ) {
          // �ղ����豸��δ��ʼ��
          s = InitDevice( n );  // ��ʼ��ö��ָ��HUB�˿ڵ�USB�豸
          RootHubDev[n].DeviceType = s;  // �����豸����
        }
      }
    }
    mDelaymS( 2 );
#if 0
    if ( count & 0x02 ) {  // ÿ��һ��ʱ����ⲿHUB�Ķ˿ڽ���һ��ö��,��Ƭ���п�ʱ��
      for ( n = 0; n < 3; n ++ ) {  // �����ⲿHUB�豸
        if ( RootHubDev[n].DeviceType == DEV_HUB && RootHubDev[n].DeviceStatus >= ROOT_DEV_SUCCESS ) 
        {
          // ��Ч���ⲿHUB
          SelectHubPort( n, 0 );  // ѡ�����ָ����ROOT-HUB�˿�,���õ�ǰUSB�ٶ��Լ��������豸��USB��ַ
          s = HubPortEnum( n );  // ö��ָ��ROOT-HUB�˿��ϵ��ⲿHUB�������ĸ����˿�,�����˿��������ӻ��Ƴ��¼�
          if ( s != USB_INT_SUCCESS ) {  // ������HUB�Ͽ���
            printf( "HubPortEnum error = %02X\n", (UINT16)s );
          }
          SetUsbSpeed( TRUE );  // Ĭ��Ϊȫ��
        }
      }
    }
#endif
    count++;
    if ( count > 2 ) 
      count = 0;
    switch( count ) {
        case 1:  // �ö�ʱģ����������,��Ҫ�������
          // ��ROOT-HUB�Լ��ⲿHUB���˿�������ָ�����͵��豸���ڵĶ˿ں�
          loc = SearchAllHubPort( DEV_MOUSE );
          if ( loc != 0xFFFF ) {
            // �ҵ���,���������MOUSE��δ���?
            n = loc >> 8;
            loc &= 0xFF;
            printf( "Query Mouse\n" );
            SelectHubPort( n, loc );  
            // ѡ�����ָ����ROOT-HUB�˿�,���õ�ǰUSB�ٶ��Լ��������豸��USB��ַ
            i = loc ? DevOnHubPort[n][loc-1].GpVar : RootHubDev[n].GpVar;  // �ж϶˵�ĵ�ַ,λ7����ͬ����־λ
            if ( i > 0 ) {
              // �˵���Ч
              s = HostTransact374( i | 0x80, DEF_USB_PID_IN, i & 0x80 );  // CH374��������,��ȡ����
              if ( s == USB_INT_SUCCESS ) {
                i ^= 0x80;  // ͬ����־��ת
                if ( loc )
                  DevOnHubPort[n][loc-1].GpVar = i;  // ����ͬ����־λ
                else RootHubDev[n].GpVar = i;
                i = Read374Byte( REG_USB_LENGTH );  // ���յ������ݳ���
                if ( i ) {
                  Read374Block( RAM_HOST_RECV, i, TempBuf );  // ȡ�����ݲ���ӡ
                  USB_SIL_Write(EP2_IN, (uint8_t*) TempBuf, i);  
                  SetEPTxValid(ENDP2);
                }
              }
              else if ( s != ( 0x20 | USB_INT_RET_NAK ) )
                printf("Mouse error %02x\n",(UINT16)s);  // �����ǶϿ���
            }
            else
              printf("Mouse no interrupt endpoint\n");
            SetUsbSpeed( TRUE );  // Ĭ��Ϊȫ��
        }
        break;
      case 2:   //search for keyboard port
        loc = SearchAllHubPort(DEV_KEYBOARD);
        if(loc != 0xFFFF){
          n = loc >> 8;
          loc &= 0xFF;
          printf( "Query Keyboard\n" );
          SelectHubPort( n, loc );
          in_endpoint = loc ? DevOnHubPort[n][loc-1].GpVar : RootHubDev[n].GpVar;
//          status = Set_Idle(); //����IDLE����������ǰ���HID���Э��������
          status = USB_INT_SUCCESS;
          if(status != USB_INT_SUCCESS) {
            printf("Set_Idle_Err=%02x\n",(unsigned short)status);
            if((status & 0x0f) == USB_INT_RET_STALL)  
              goto next_operate1; //����STALL���ܱ���֧��
          }
          else 
            printf("Set_idle success\n");
next_operate1:
//          printf("Get Hid Descriptors\n");
//          status = Get_Hid_Des(HidDescBuf);  // ��ȡ����������������
          status = USB_INT_SUCCESS;
          if(status == USB_INT_SUCCESS)
          {
            printf("HID_Desc: ");
            for(i=0;i < hid_des_leng;i++)
              printf("%02x ",(unsigned short)HidDescBuf[i]);
            printf("\n");
            printf("Set_Report \n"); //���ڼ��̷�Set_Report��������,�����������Ҫ��һ��
            
            status = Interrupt_Data_Trans(0x80|in_endpoint,HidDataBuf,&HIDCount);
next_operate2:
            if(status == USB_INT_SUCCESS)
            {
              USB_SIL_Write(EP1_IN, (uint8_t*) HidDataBuf, HIDCount);  
              SetEPTxValid(ENDP1);
              keyboard_key = HandleKeyBoardKeyVal(HidDataBuf);
              if(DUMMY_KEY != keyboard_key) {
                ProcessKeyBoardVal(keyboard_key,bootmode);
              }
            }
          }
          if(OutPacket_Len > 0) {
            Set_Report(Receive_Buffer);
            OutPacket_Len = 0;
          }
        }
        break;
      default:break;
    }
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