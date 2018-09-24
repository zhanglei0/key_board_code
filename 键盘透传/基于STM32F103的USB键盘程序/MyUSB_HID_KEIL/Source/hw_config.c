/******************** (C) COPYRIGHT 2008 STMicroelectronics ********************
* File Name          : hw_config.c
* Author             : MCD Application Team
* Version            : V2.2.1
* Date               : 09/22/2008
* Description        : Hardware Configuration & Setup
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_lib.h"
#include "stm32f10x_tim.h"
#include "platform_config.h"
#include "stm32f10x_map.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_systick.h"

#include "hw_config.h"
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#define BOOL bool
ErrorStatus HSEStartUpStatus;
void RTC_Configuration(void);
void Timer2Configuration(void);

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define ADC1_DR_Address    ((u32)0x4001244C)
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/


/* Extern variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

void Set_System(void)
{ 
     GPIO_Configuration();
}
void ConfigurationGPIO(void)
{
  // Configure the used GPIOs 
     //EXTI_Configuration();
     GPIO_Configuration();
     //SPI_Configuration(); 
}
void RCC_Configuration(void)
{
    /* SYSCLK, HCLK, PCLK2 and PCLK1 configuration -----------------------------*/   
     
  /* RCC system reset(for debug purpose) */
	RCC_DeInit();

  /* Enable HSE */
	RCC_HSEConfig(RCC_HSE_ON);

  /* Wait till HSE is ready */
	HSEStartUpStatus = RCC_WaitForHSEStartUp();

	if(HSEStartUpStatus == SUCCESS)
	{
    /* Enable Prefetch Buffer */
		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);

    /* Flash 2 wait state */
		FLASH_SetLatency(FLASH_Latency_2);
 
    /* HCLK = SYSCLK */
		RCC_HCLKConfig(RCC_SYSCLK_Div1); 
  
    /* PCLK2 = HCLK */
		RCC_PCLK2Config(RCC_HCLK_Div1); 

    /* PCLK1 = HCLK/2 */
		RCC_PCLK1Config(RCC_HCLK_Div2);  // 36MZH

    /* On STICE the PLL output clock is fixed to 48 MHz */
		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_6);

    /* Enable PLL */ 
		RCC_PLLCmd(ENABLE);

    /* Wait till PLL is ready */
		while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
		{
		}

    /* Select PLL as system clock source */
		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    /* Wait till PLL is used as system clock source */
		while(RCC_GetSYSCLKSource() != 0x08)
		{
		}
	}
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP |
                               RCC_APB1Periph_TIM2, ENABLE);
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIO_DISCONNECT |
			       RCC_APB2Periph_SPI1 | RCC_APB2Periph_GPIOB, ENABLE); 
}
/*******************************************************************************
* Function Name  : Set_USBClock
* Description    : Configures USB Clock input (48MHz).
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Set_USBClock(void)
{
  // Select USBCLK source 
     RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5);  // 72MHZ sysclk
  //  RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div1);

  /* Enable USB clock */
     RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
}

/*******************************************************************************
* Function Name  : Enter_LowPowerMode.
* Description    : Power-off system clocks and power while entering suspend mode.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Enter_LowPowerMode(void)
{
  /* Set the device state to suspend */
  bDeviceState = SUSPENDED;
}

/*******************************************************************************
* Function Name  : Leave_LowPowerMode.
* Description    : Restores system clocks and power while exiting suspend mode.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Leave_LowPowerMode(void)
{
  DEVICE_INFO *pInfo = &Device_Info;

  /* Enable HSE */
  RCC_HSEConfig(RCC_HSE_ON);

  /* Wait till HSE is ready */
  HSEStartUpStatus = RCC_WaitForHSEStartUp();

  /* Enable HSE */
  RCC_HSEConfig(RCC_HSE_ON);

  /* Wait till HSE is ready */
  while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET)
  {}

  /* Enable PLL */
  RCC_PLLCmd(ENABLE);

  /* Wait till PLL is ready */
  while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
  {}

  /* Select PLL as system clock source */
  RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

  /* Wait till PLL is used as system clock source */
  while (RCC_GetSYSCLKSource() != 0x08)
  {}

  /* Set the device state to the correct state */
  if (pInfo->Current_Configuration != 0)
  {
    /* Device configured */
    bDeviceState = CONFIGURED;
  }
  else
  {
    bDeviceState = ATTACHED;
  }
}

/*******************************************************************************
* Function Name  : USB_Interrupts_Config.
* Description    : Configures the USB interrupts.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void USB_Interrupts_Config(void)
{
     NVIC_InitTypeDef NVIC_InitStructure; 
     NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN_RX0_IRQChannel;
     NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
     NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
     NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
     NVIC_Init(&NVIC_InitStructure);
   // enable timer2
//     NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQChannel;
//     NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
//     NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;  
//     NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
//     NVIC_Init(&NVIC_InitStructure);
    // 

}
/*******************************************************************************
* Function Name : Timer2Config.
* Description   : interrupt enable
* Input         : None.
* Output        : None.
* Return value  : The direction value.
*******************************************************************************/
void Timer2Configuration(void)
{//
     TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
     TIM_TimeBaseStructure.TIM_Period = 20*1000;  //  1s:20000  1ms*1000=1s
     TIM_TimeBaseStructure.TIM_Prescaler = (3600-1); //36MHZ/3600 =10Khz      
     TIM_TimeBaseStructure.TIM_ClockDivision = 0;   //36MHZ 
     TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //Mode;
     TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
     TIM_Cmd(TIM2, ENABLE);
     TIM_ITConfig(TIM2,TIM_IT_CC1,ENABLE);
     TIM_ClearITPendingBit(TIM2, TIM_IT_CC1 );	
}
/*******************************************************************************
* Function Name  : USB_Cable_Config.
* Description    : Software Connection/Disconnection of USB Cable.
* Input          : NewState: new state.
* Output         : None.
* Return         : None
*******************************************************************************/
void USB_Cable_Config (FunctionalState NewState)
{ 
	if (NewState != DISABLE)
	{
		GPIO_ResetBits(USB_DISCONNECT, USB_DISCONNECT_PIN);
	}
	else
	{
		GPIO_SetBits(USB_DISCONNECT, USB_DISCONNECT_PIN);
	}
}

/*******************************************************************************
* Function Name  : GPIO_Configuration
* Description    : Configures the different GPIO ports.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void GPIO_Configuration(void)
{
     GPIO_InitTypeDef GPIO_InitStructure;
	/* USB_DISCONNECT used as USB pull-up */
     GPIO_InitStructure.GPIO_Pin = USB_DISCONNECT_PIN;
     GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
     GPIO_Init(USB_DISCONNECT, &GPIO_InitStructure);
  
	/* Configure SPI1 pins: SCK, MISO and MOSI ---------------------------------*/
     GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 |GPIO_Pin_7;
     GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//AF_PP
     GPIO_Init(GPIOA, &GPIO_InitStructure);
	
     GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;//MISO
     GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
     GPIO_Init(GPIOA, &GPIO_InitStructure);
     //for LED test PA3,PA2
     GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_2;
     GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
     GPIO_Init(GPIOA, &GPIO_InitStructure);
 
}
void DisableClock(void)
{
     RCC_APB1PeriphClockCmd(RCC_APB1Periph_ALL,DISABLE);
     RCC_APB2PeriphClockCmd(RCC_APB2Periph_ALL,DISABLE);
}

/*******************************************************************************
* Function Name  : Get_SerialNum.
* Description    : Create the serial number string descriptor.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Get_SerialNum(void)
{
     u32 Device_Serial0, Device_Serial1, Device_Serial2;
     Device_Serial0 = *(u32*)(0x1FFFF7E8);
     Device_Serial1 = *(u32*)(0x1FFFF7EC);
     Device_Serial2 = *(u32*)(0x1FFFF7F0);
     if (Device_Serial0 != 0)
     {
        CustomHID_StringSerial[2] = (u8)(Device_Serial0 & 0x000000FF);  
				CustomHID_StringSerial[4] = (u8)((Device_Serial0 & 0x0000FF00) >> 8);
				CustomHID_StringSerial[6] = (u8)((Device_Serial0 & 0x00FF0000) >> 16);
			  CustomHID_StringSerial[8] = (u8)((Device_Serial0 & 0xFF000000) >> 24);
				 
			  CustomHID_StringSerial[10] = (u8)(Device_Serial1 & 0x000000FF);  
			  CustomHID_StringSerial[12] = (u8)((Device_Serial1 & 0x0000FF00) >> 8);
			  CustomHID_StringSerial[14] = (u8)((Device_Serial1 & 0x00FF0000) >> 16);
			  CustomHID_StringSerial[16] = (u8)((Device_Serial1 & 0xFF000000) >> 24);
				 
			  CustomHID_StringSerial[18] = (u8)(Device_Serial2 & 0x000000FF);  
			  CustomHID_StringSerial[20] = (u8)((Device_Serial2 & 0x0000FF00) >> 8);
			  CustomHID_StringSerial[22] = (u8)((Device_Serial2 & 0x00FF0000) >> 16);
			  CustomHID_StringSerial[24] = (u8)((Device_Serial2 & 0xFF000000) >> 24);
      }
}
void USB_Connect (BOOL con) 
{
     GPIO_InitTypeDef GPIO_InitStructure;
     if (con)
     {
        GPIO_InitStructure.GPIO_Pin = USB_DISCONNECT_PIN;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
        GPIO_Init(USB_DISCONNECT, &GPIO_InitStructure);
     }
     else
     {
        GPIO_InitStructure.GPIO_Pin = USB_DISCONNECT_PIN;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
        GPIO_Init(USB_DISCONNECT, &GPIO_InitStructure); 
     }
/*
     *CNTR = CNTR_FRES;                         // Force USB Reset  
     *ISTR = 0;                                 // Clear Interrupt Status  
     if (con) {
        *CNTR = CNTR_RESETM;                     // USB Reset Interrupt Mask  
     } else {
        *CNTR = CNTR_FRES | CNTR_PDWN;           // Switch Off USB Device  
     }
  */
}
/******************* (C) COPYRIGHT 2008 STMicroelectronics *****END OF FILE****/
