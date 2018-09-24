#include "port.h"
#include "ch374inc.h"

volatile uint32_t gtick = 0;
#define UART_RECV_LEN           16

volatile uint8_t USART_RecvBuf[UART_RECV_LEN];
volatile uint8_t uart_recv_len  = 0;
volatile uint8_t uart_recv_done = 0;

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


uint8_t GetFunctionSel(void)
{
  uint16_t gpio;
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  Delay(100);
  gpio = GPIO_ReadInputData(GPIOB);
  return ((gpio >> 5) & 0x07);
}


void InitKey(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO,ENABLE);
  GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_3 | GPIO_Pin_4;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void InitBeep(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_ResetBits(GPIOB,GPIO_Pin_10);
}

uint8_t ScanKeyBoard(void)
{
#define KEY_MASK        0x1B
  uint16_t gpio_old;
  uint8_t ret = 0;
  uint16_t gpio = GPIO_ReadInputData(GPIOB);
  gpio &= KEY_MASK;
  gpio_old = gpio;
  if(gpio != KEY_MASK)
  {
    Delay(50);
    gpio = GPIO_ReadInputData(GPIOB) & KEY_MASK;
    if(gpio == gpio_old)
    {
      while(1)
      {
        if((GPIO_ReadInputData(GPIOB) & KEY_MASK) == KEY_MASK)
        {
          break;
        }
      }
      switch(gpio)
      {
      case 0x1A:
        ret = 1;
        break;
      case 0x19:
        ret = 2;
        break;
      case 0x13:
        ret = 3;
        break;
      case 0x0B:
        ret = 4;
        break;
      default:break;
      }
    }
  }
  return ret;
}

void Beep(uint32_t tick)
{
  GPIO_SetBits(GPIOB,GPIO_Pin_10);
  Delay(tick);
  GPIO_ResetBits(GPIOB,GPIO_Pin_10);
}

void InitUart(void)
{
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
  GPIO_InitTypeDef GPIO_InitStructure;
  /* Configure USARTy Rx as input floating */
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  /* Configure USARTz Rx as input floating */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  NVIC_InitTypeDef NVIC_InitStructure;

  /* Configure the NVIC Preemption Priority Bits */  
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
  
  /* Enable the USARTy Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
  USART_InitTypeDef USART_InitStructure;
  USART_InitStructure.USART_BaudRate   = 115200;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  /* Configure USARTy */
  USART_Init(USART1, &USART_InitStructure);
  /* Enable USARTy Receive and Transmit interrupts */
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  USART_Cmd(USART1, ENABLE);
}

void InitDetectPin(void)
{
  uint16_t state;
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  Delay(200);
  state = GPIO_ReadInputData(GPIOB) & 0xF000;
  if(state & 0x1000)
  {
    GPIO_ResetBits(GPIOB,GPIO_Pin_8|GPIO_Pin_9);
  }
  else 
  {
    if(0xE000 == (state & 0xE000))
    {
      GPIO_ResetBits(GPIOB,GPIO_Pin_8|GPIO_Pin_9);
    }
    else 
      GPIO_SetBits(GPIOB,GPIO_Pin_8|GPIO_Pin_9);
  }
}
void SysTick_Handler(void)
{
  gtick++;
}
void USART_SendString(uint8_t *str)
{
  uint16_t len = strlen(str);
  for(uint16_t i = 0; i < len ;i++) {
    /* Send one byte from USARTy to USARTz */
    USART_SendData(USART1, *(str + i));
    
    /* Loop until USARTy DR register is empty */ 
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
    {
    }
  }
}

void HandleUARTEvt(void)
{
  uint8_t string[5] = {'A','C','K',0,0};
  if(uart_recv_done == 1)
  {
    uart_recv_done = 0;
    if(memcmp("SW",(const uint8_t *)USART_RecvBuf,2) == 0)
    {
      uint8_t num = USART_RecvBuf[2] - '0';
      switch(num)
      {
      case 1:
      case 2:
      case 3:
      case 4:
        control_led(num - 1);
        string[3] = num + '0';
        USART_SendString(string);
        break;
      default:break;
      }
    }
  }
}

static const uint8_t dummy_key[8]   = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t ctrl_buf[8]     = {0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t key1_buf[8] =  {0x00,0x00,0x1E,0x00,0x00,0x00,0x00,0x00};
static const uint8_t key2_buf[8] =  {0x00,0x00,0x1F,0x00,0x00,0x00,0x00,0x00};
static const uint8_t key3_buf[8] =  {0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00};
static const uint8_t key4_buf[8] =  {0x00,0x00,0x21,0x00,0x00,0x00,0x00,0x00};

static const uint8_t alt_ctrl_key1_buf[8] =  {0x05,0x00,0x1E,0x00,0x00,0x00,0x00,0x00};
static const uint8_t alt_ctrl_key2_buf[8] =  {0x05,0x00,0x1F,0x00,0x00,0x00,0x00,0x00};
static const uint8_t alt_ctrl_key3_buf[8] =  {0x05,0x00,0x20,0x00,0x00,0x00,0x00,0x00};
static const uint8_t alt_ctrl_key4_buf[8] =  {0x05,0x00,0x21,0x00,0x00,0x00,0x00,0x00};

static const uint8_t ctrl_key1_buf[8] =  {0x01,0x00,0x1E,0x00,0x00,0x00,0x00,0x00};
static const uint8_t ctrl_key2_buf[8] =  {0x01,0x00,0x1F,0x00,0x00,0x00,0x00,0x00};
static const uint8_t ctrl_key3_buf[8] =  {0x01,0x00,0x20,0x00,0x00,0x00,0x00,0x00};
static const uint8_t ctrl_key4_buf[8] =  {0x01,0x00,0x21,0x00,0x00,0x00,0x00,0x00};

#if 0
#define CTRL_KEY                        0x0A

#define KEY_1                           0x0C
#define KEY_2                           0x0E
#define KEY_3                           0x0F
#define KEY_4                           0x10

#define CTRL_ALT_KEY_1                  0x11
#define CTRL_ALT_KEY_2                  0x12
#define CTRL_ALT_KEY_3                  0x13
#define CTRL_ALT_KEY_4                  0x14

#define CTRL_KEY_1                      0x20
#define CTRL_KEY_2                      0x21
#define CTRL_KEY_3                      0x22
#define CTRL_KEY_4                      0x23

#endif
uint8_t HandleKeyBoardKeyVal(uint8_t *input)
{
  if(memcmp(input,dummy_key,8) == 0)
  {
    return DUMMY_KEY;
  }
  if(memcmp(input,ctrl_buf,8) == 0)
  {
    return CTRL_KEY;
  }
  else if(memcmp(input,key1_buf,8) == 0)
  {
    return KEY_1;
  }
  else if(memcmp(input,key2_buf,8) == 0)
  {
    return KEY_2;
  }
  else if(memcmp(input,key3_buf,8) == 0)
  {
    return KEY_3;
  }
  else if(memcmp(input,key4_buf,8) == 0)
  {
    return KEY_4;
  }
  else if(memcmp(input,alt_ctrl_key1_buf,8) == 0)
  {
    return CTRL_ALT_KEY_1;
  }
  else if(memcmp(input,alt_ctrl_key2_buf,8) == 0)
  {
    return CTRL_ALT_KEY_2;
  }
  else if(memcmp(input,alt_ctrl_key3_buf,8) == 0)
  {
    return CTRL_ALT_KEY_3;
  }
  else if(memcmp(input,alt_ctrl_key4_buf,8) == 0)
  {
    return CTRL_ALT_KEY_4;
  }
  else if(memcmp(input,ctrl_key1_buf,8) == 0)
  {
    return CTRL_ALT_KEY_1;
  }
  else if(memcmp(input,ctrl_key2_buf,8) == 0)
  {
    return CTRL_ALT_KEY_2;
  }
  else if(memcmp(input,ctrl_key3_buf,8) == 0)
  {
    return CTRL_ALT_KEY_3;
  }
  else if(memcmp(input,ctrl_key4_buf,8) == 0)
  {
    return CTRL_ALT_KEY_4;
  }
  else 
    return 0;
}

static uint8_t toggle_times = 0;
void ProcessKeyBoardVal(uint8_t val,uint8_t bootmode)
{
  if(bootmode == 0x07)
  {
    if(val == CTRL_KEY)
    {
      if(toggle_times == 0)
        toggle_times = 1;
      else if(toggle_times == 1)
        toggle_times = 2;
      else 
        toggle_times = 0;
    }
    else if((val == KEY_1) || (val == KEY_2) || (val == KEY_3) || (val == KEY_4))
    {
      if(toggle_times == 2)
        control_led(val - KEY_1);
      toggle_times = 0;
    }
    else 
      toggle_times = 0;
  }
  else if(bootmode == 0x06)
  {
    if((val == CTRL_ALT_KEY_1) || (val == CTRL_ALT_KEY_2) || 
       (val == CTRL_ALT_KEY_3) || (val == CTRL_ALT_KEY_4))
    {
      control_led(val - CTRL_ALT_KEY_1);
    }
  }
  else if(bootmode == 0x05)
  {
    if((val == CTRL_KEY_1) || (val == CTRL_KEY_2) || 
       (val == CTRL_KEY_3) || (val == CTRL_KEY_4))
    {
      control_led(val - CTRL_KEY_1);
    }
  }
}

void USART1_IRQHandler(void)
{
  if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
  {
    USART_RecvBuf[uart_recv_len] = USART_ReceiveData(USART1);
    if(uart_recv_len >= UART_RECV_LEN)
      uart_recv_len = 0;
    else 
      uart_recv_len++;
    if(USART_RecvBuf[uart_recv_len - 1] == '\n')
    {
      uart_recv_done = 1;
      uart_recv_len = 0;
    }
  }
}