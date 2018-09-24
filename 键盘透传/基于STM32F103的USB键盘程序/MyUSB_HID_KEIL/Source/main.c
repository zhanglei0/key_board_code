/******************** (C) COPYRIGHT *********************************************
* Project Name       : MyUSB_hid
* Author             : DZ561
* Version            : V1.0
* Date               : Jun-17-2010
* Description        : 	
用于学习STM32 USB HID
源码已测试通s
欢迎联系：dz561@yahoo.cn
欢迎访问我的博客：http://blog.ednchina.com/itspy/  
PC上位机程序可以到我的博客去下载  
********************************************************************************/
#include "stm32f10x_lib.h"
#include "usb_lib.h"
#include "hw_config.h"
#include "stm32f10x_it.h"
#include "string.h"

#define nReportCnt 22

extern u8 Receive_Buffer[22];
extern u8 Transi_Buffer[22];
extern u8 USB_ReceiveFlg;

u8 Key_Table[10][8];
u8 USB_SBUF[8]={0,0,0x04,0,0,0,0,0};
u8 USB_SBUF_Zero[8]={0,0,0,0,0,0,0,0};

void DebugApp(void);
void NVIC_Configuration(void);
extern int RCC_Configuration(void);
extern void USB_SendString(u8 *str);
void Keyboard_Send(u8 *Key);
void Keyboard_Send_KeyString(u8 *Keys,u16 KeyLen,u16 SendEnterFlag);
void Keyboard_Send_Keyup(void);
void Delay(vu32 nCount);

vu8 MsgCmd;
extern u8 TimeCount;
int main(void)
{
     RCC_Configuration();
     NVIC_Configuration();
     Set_System();	 
//Control USB connecting via SW A
//enable USB for STM32F103RB
//#ifdef STM32F103RB
//     RCC->APB2ENR |= (1 << 5);                 // enable clock for GPIOD  
//     GPIOD->CRL &= ~0x00000F00;                // clear port PD2  
//     GPIOD->CRL |=  0x00000700;                // PD2 General purpose output open-drain, max speed 50 MHz  
//     GPIOD->BRR  = 0x0004;                     // reset PD2  (set to low)  
//#endif
     USB_Interrupts_Config();
     Set_USBClock();    
     USB_Init();     
//		 while(USB_ReceiveFlg == TRUE)
//     {
//       USB_SendString("Hi,PC! I'm STM32-ARM");
//     }
     while(1) //
     {       
				Delay(1100);
        Keyboard_Send(USB_SBUF);   
			  Keyboard_Send(USB_SBUF_Zero);
     }
}
void USB_SendString(u8 *str)
{
     u8 ii=0;   
     while(*str)
     {
         Transi_Buffer[ii++]=*(str++);
         if (ii == nReportCnt) break;
     }
     UserToPMABufferCopy(Transi_Buffer, ENDP2_TXADDR, nReportCnt);
     SetEPTxValid(ENDP2);
}

/* Function: **********************************
***	Send a byte data to PC as a keyboard *
***********************************************/
void Keyboard_Send(u8 *Key)
 {
  /*copy mouse position info in ENDP1 Tx Packet Memory Area*/
  UserToPMABufferCopy(Key, GetEPTxAddr(ENDP1), 8);
  /* enable endpoint for transmission */
  SetEPTxValid(ENDP1);
 }
 
/* Function: 
***	Send a bounch of data to PC as a keyboard 
 *  Keys  				: datas you need to send
 *  KeyLen				: length of the data
 *  SendEnterFlag : Weather you want to Send Enter message after you send this datas
**/
 void Keyboard_Send_KeyString(u8 *Keys,u16 KeyLen,u16 SendEnterFlag)
 {
  u8 Buffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  u8 loop,i;
  u16 nSendCouter;
  u16 nTableOffset;
  nSendCouter = 0 ;
 // i=2;
  for(loop=0;loop<KeyLen;loop++)
  {
   //memset(Buffer,0x00,sizeof(Buffer));
   //KEY down message
  //memcpy(Buffer+2,Keys+loop*6,6);
   nTableOffset = Keys[loop] - 0x20 ;
   if(nSendCouter)
   {
    if((Buffer[0] != Key_Table[nTableOffset][0]) || nSendCouter >= 6)//判断按键是否需要区分SHIFT
    {     
    //如果需要区分，则先把之前缓冲区里的数据发送出去
    //或者如果6个按键缓冲区已经满了，则直接发出去。
		  Keyboard_Send(Buffer);
      Buffer[0] = Key_Table[nTableOffset][0] ;
      Buffer[2] = Key_Table[nTableOffset][1] ;
      nSendCouter = 1 ;
    }
    else
    {
     for(i=0;i<nSendCouter;i++)
     {     
				if(Buffer[2+i] == Key_Table[nTableOffset][1])	//判断缓冲区里有没有重复的数据
				{																						 	//有，则把数据发出去
				 Keyboard_Send(Buffer);
				 nSendCouter = 0 ;
				 break;
				}
     }
     Buffer[0] = Key_Table[nTableOffset][0] ;
     Buffer[2+nSendCouter] = Key_Table[nTableOffset][1] ;
     nSendCouter ++ ;
    }
   }
   else
   {
    Buffer[0] = Key_Table[nTableOffset][0] ;
    Buffer[2] = Key_Table[nTableOffset][1] ;
    nSendCouter ++ ;
   }

 }
  if(nSendCouter)
  {
		Keyboard_Send(Buffer);
  }
  if(SendEnterFlag)
  {
		Buffer[0] = 0 ;
		Buffer[2] = 0x28 ;
		Keyboard_Send(Buffer);
  }
 }
 

void Keyboard_Send_Keyup(void)
{
  u8 Buffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	while(GetEPTxStatus(ENDP1)!=(0x02<<4));
  /*copy mouse position info in ENDP1 Tx Packet Memory Area*/
  UserToPMABufferCopy(Buffer, GetEPTxAddr(ENDP1), 8);
  /* enable endpoint for transmission */
  SetEPTxValid(ENDP1);
}


/*******************************************************************************
* Function Name  : NVIC_Configuration
* Description    : Configures Vector Table base location.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void NVIC_Configuration(void)
{    
  //NVIC_InitTypeDef NVIC_InitStructure;
#ifdef  VECT_TAB_RAM  
  /* Set the Vector Table base location at 0x20000000 */ 
     NVIC_SetVectorTable(NVIC_VectTab_RAM, 0x0); 
#else  /* VECT_TAB_FLASH  */
  /* Set the Vector Table base location at 0x08000000 */ 
     NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);   
#endif 
}
/*******************************************************************************
* Function Name  : Delay
* Description    : Inserts a delay time.
* Input          : nCount: specifies the delay time length.
* Output         : None
* Return         : None
*******************************************************************************/
void Delay(vu32 nCount)
{
  for(; nCount!= 0;nCount--);
}

#ifdef  DEBUG
/*******************************************************************************
* Function Name  : assert_failed
* Description    : Reports the name of the source file and the source line number
*                  where the assert_param error has occurred.
* Input          : - file: pointer to the source file name
*                  - line: assert_param error line source number
* Output         : None
* Return         : None
*******************************************************************************/
void assert_failed(u8* file, u32 line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* Infinite loop */
  while(1)
  {
  }
  
}
#endif
