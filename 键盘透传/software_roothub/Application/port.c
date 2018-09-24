#include "port.h"
#include "EX_HUB.H"

volatile uint32_t gtick = 0;


/* SET IDLE */
const unsigned char  SetupSetidle[]={0x21,0x0a,0x00,0x00,0x00,0x00,0x00,0x00};        
/* 获取HID 报告描述符 */
unsigned char  SetupGetHidDes[]={0x81,0x06,0x00,0x22,0x00,0x00,0x81,0x00};    
/* SET REPORT */
unsigned char  SetupSetReport[]={0x21,0x09,0x00,0x02,0x00,0x00,0x01,0x00};     
UINT8	 tog1;              //读取数据时的同步标志

static void shortDelay(uint32_t ms)
{
  while(ms--);
}
void InitSPI(void)
{
  SPI_InitTypeDef   SPI_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1,ENABLE);
  /* SCK MOSI */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  /* MISO */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  /* CS */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  /* INT */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  /* SPIy Config -------------------------------------------------------------*/
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_Init(SPI1, &SPI_InitStructure);
  SPI_Cmd(SPI1, ENABLE);
  GPIO_SetBits(GPIOA,GPIO_Pin_4);
}

static inline uint8_t SPI_Transmit(uint8_t data)
{
  /* Wait for SPIy Tx buffer empty */
  while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
  /* Send SPIz data */
  SPI_I2S_SendData(SPI1,data);
  while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
  data = SPI_I2S_ReceiveData(SPI1);
  return data;
}

void SpiTransceiver(uint8_t *buff,uint16_t len) 
{
  uint16_t i;
  Spi374Start();
  shortDelay(32);
  for(i = 0; i < len;i++)
  {
    *(buff + i) = SPI_Transmit(*(buff + i));
  }
  shortDelay(32);
  Spi374Stop();
}
uint8_t	Read374Byte(uint8_t mAddr )
{
    uint8_t	d;
    Spi374Start();
    shortDelay(32);
    SPI_Transmit(mAddr);
    SPI_Transmit(CMD_SPI_374READ);
    d = SPI_Transmit(0xFF);
    shortDelay(32);
    Spi374Stop();
    return( d );
}
void Write374Byte( uint8_t mAddr, uint8_t mData )  /* 向指定寄存器写入数据 */
{
  Spi374Start();
  shortDelay(32);
  SPI_Transmit(mAddr);
  SPI_Transmit(CMD_SPI_374WRITE);
  SPI_Transmit(mData);
  shortDelay(32);
  Spi374Stop();
}

void Read374Block( uint8_t mAddr, uint8_t mLen, uint8_t *mBuf )  /* 从指定起始地址读出数据块 */
{
  Spi374Start();
  shortDelay(32);
  SPI_Transmit(mAddr);
  SPI_Transmit(CMD_SPI_374READ);
  while ( mLen -- ) *mBuf++ = SPI_Transmit(0xFF);
  shortDelay(32);
  Spi374Stop();
}

void Write374Block( uint8_t mAddr, uint8_t mLen, uint8_t *mBuf )  /* 向指定起始地址写入数据块 */
{
  Spi374Start();
  shortDelay(32);
  SPI_Transmit(mAddr);
  SPI_Transmit(CMD_SPI_374WRITE);
  while ( mLen -- ) SPI_Transmit(*mBuf++ );
  shortDelay(32);
  Spi374Stop();
}

void InitSystick(void)
{
  RCC_ClocksTypeDef clock;
  RCC_GetClocksFreq(&clock);
  SystemCoreClock = clock.HCLK_Frequency;
  SysTick_Config(SystemCoreClock / 1000);
}

uint32_t GetTick(void)
{
  return gtick;
}

void Delay(uint32_t ms)
{
  uint32_t start_tick = GetTick();
  while(start_tick + ms > GetTick());
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