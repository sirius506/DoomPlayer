/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f7xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f769i_discovery_sd.h"
#include "stm32f769i_discovery_audio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern JPEG_HandleTypeDef    JPEG_Handle;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern HCD_HandleTypeDef  hhcd_USB_OTG_HS;
extern DMA2D_HandleTypeDef hdma2d_discovery;
extern DSI_HandleTypeDef  hdsi_discovery;
extern LTDC_HandleTypeDef hltdc_discovery;
extern SD_HandleTypeDef   uSdHandle;
extern TIM_HandleTypeDef  htim6;
extern SAI_HandleTypeDef  haudio_out_sai;;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern UART_HandleTypeDef huart1;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M7 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32F7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles EXTI line[15:10] interrupts.
  */
void EXTI15_10_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */

  /* USER CODE END EXTI15_10_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(LCD_INT_Pin);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

/**
  * @brief This function handles TIM6 global interrupt, DAC1 and DAC2 underrun error interrupts.
  */
void TIM6_DAC_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_DAC_IRQn 0 */

  /* USER CODE END TIM6_DAC_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_DAC_IRQn 1 */

  /* USER CODE END TIM6_DAC_IRQn 1 */
}

/**
  * @brief This function handles DMA2 stream7 global interrupt.
  */
void DMA2_Stream7_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Stream7_IRQn 0 */

  /* USER CODE END DMA2_Stream7_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_tx);
  /* USER CODE BEGIN DMA2_Stream7_IRQn 1 */

  /* USER CODE END DMA2_Stream7_IRQn 1 */
}

/**
  * @brief This function handles USB On The Go HS global interrupt.
  */
void OTG_HS_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_HS_IRQn 0 */

  /* USER CODE END OTG_HS_IRQn 0 */
  HAL_HCD_IRQHandler(&hhcd_USB_OTG_HS);
  /* USER CODE BEGIN OTG_HS_IRQn 1 */

  /* USER CODE END OTG_HS_IRQn 1 */
}

/**
  * @brief This function handles LTDC global interrupt.
  */
void LTDC_IRQHandler(void)
{
  /* USER CODE BEGIN LTDC_IRQn 0 */

  /* USER CODE END LTDC_IRQn 0 */
  HAL_LTDC_IRQHandler(&hltdc_discovery);
  /* USER CODE BEGIN LTDC_IRQn 1 */

  /* USER CODE END LTDC_IRQn 1 */
}

/**
  * @brief This function handles DMA2D global interrupt.
  */
void DMA2D_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2D_IRQn 0 */

  /* USER CODE END DMA2D_IRQn 0 */
  HAL_DMA2D_IRQHandler(&hdma2d_discovery);
  /* USER CODE BEGIN DMA2D_IRQn 1 */

  /* USER CODE END DMA2D_IRQn 1 */
}

/**
  * @brief This function handles DSI global interrupt.
  */
void DSI_IRQHandler(void)
{
  /* USER CODE BEGIN DSI_IRQn 0 */

  /* USER CODE END DSI_IRQn 0 */
  HAL_DSI_IRQHandler(&hdsi_discovery);
  /* USER CODE BEGIN DSI_IRQn 1 */

  /* USER CODE END DSI_IRQn 1 */
}

/**
  * @brief This function handles SDMMC2 global interrupt.
  */
void SDMMC2_IRQHandler(void)
{
  /* USER CODE BEGIN SDMMC2_IRQn 0 */

  /* USER CODE END SDMMC2_IRQn 0 */
  HAL_SD_IRQHandler(&uSdHandle);
  /* USER CODE BEGIN SDMMC2_IRQn 1 */

  /* USER CODE END SDMMC2_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/**
 * @brief Handles SDMMC2 DMA Rx transfer interrupt request.
 * @retval None
 */
void BSP_SDMMC_DMA_Rx_IRQHandler(void)
{
  HAL_DMA_IRQHandler(uSdHandle.hdmarx);
}

/**
 * @brief Handles SDMMC2 DMA Tx transfer interrupt request.
 * @retval None
 */
void BSP_SDMMC_DMA_Tx_IRQHandler(void)
{
  HAL_DMA_IRQHandler(uSdHandle.hdmatx);
}

void AUDIO_OUT_SAIx_DMAx_IRQHandler(void)
{
  HAL_DMA_IRQHandler(haudio_out_sai.hdmatx);
}

void WWDG_IRQHandler()
{
  debug_printf("%s!\n", __FUNCTION__);
  while (1) ;
}

void PVD_IRQHandler()
{
  debug_printf("%s!\n", __FUNCTION__);
  while (1) ;
}

void TAMP_STAMP_IRQHandler()
{
  debug_printf("%s!\n", __FUNCTION__);
  while (1) ;
}

void RTC_WKUP_IRQHandler()
{
  debug_printf("%s!\n", __FUNCTION__);
  while (1) ;
}

void FLASH_IRQHandler()
{
  debug_printf("%s!\n", __FUNCTION__);
  while (1) ;
}

void RCC_IRQHandler()
{
  debug_printf("%s!\n", __FUNCTION__);
  while (1) ;
}

void EXTI0_IRQHandler()
{
  debug_printf("%s!\n", __FUNCTION__);
  HAL_GPIO_EXTI_IRQHandler(WAKEUP_BUTTON_PIN);
}

void EXTI1_IRQHandler()
{
  debug_printf("%s!\n", __FUNCTION__);
  while (1) ;
}

void JPEG_IRQHandler(void)
{
  HAL_JPEG_IRQHandler(&JPEG_Handle);
}

void DMA2_Stream3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(JPEG_Handle.hdmain);
}

void DMA2_Stream4_IRQHandler(void)
{
  HAL_DMA_IRQHandler(JPEG_Handle.hdmaout);
}

#if 0
   .weak      EXTI2_IRQHandler         
   .weak      EXTI3_IRQHandler         
   .weak      EXTI4_IRQHandler         
   .weak      DMA1_Stream0_IRQHandler               
   .weak      DMA1_Stream1_IRQHandler               
   .weak      DMA1_Stream2_IRQHandler               
   .weak      DMA1_Stream3_IRQHandler               
   .weak      DMA1_Stream4_IRQHandler              
   .weak      DMA1_Stream5_IRQHandler               
   .weak      DMA1_Stream6_IRQHandler               
   .weak      ADC_IRQHandler      
   .weak      CAN1_TX_IRQHandler   
   .weak      CAN1_RX0_IRQHandler                  
   .weak      CAN1_RX1_IRQHandler                  
   .weak      CAN1_SCE_IRQHandler                  
   .weak      EXTI9_5_IRQHandler   
   .weak      TIM1_BRK_TIM9_IRQHandler            
   .weak      TIM1_UP_TIM10_IRQHandler            
   .weak      TIM1_TRG_COM_TIM11_IRQHandler      
   .weak      TIM1_CC_IRQHandler   
   .weak      TIM2_IRQHandler            
   .weak      TIM3_IRQHandler            
   .weak      TIM4_IRQHandler            
   .weak      I2C1_EV_IRQHandler   
   .weak      I2C1_ER_IRQHandler   
   .weak      I2C2_EV_IRQHandler   
   .weak      I2C2_ER_IRQHandler   
   .weak      SPI1_IRQHandler            
   .weak      SPI2_IRQHandler            
   .weak      USART1_IRQHandler      
   .weak      USART2_IRQHandler      
   .weak      USART3_IRQHandler      
   .weak      EXTI15_10_IRQHandler               
   .weak      RTC_Alarm_IRQHandler               
   .weak      OTG_FS_WKUP_IRQHandler         
   .weak      TIM8_BRK_TIM12_IRQHandler         
   .weak      TIM8_UP_TIM13_IRQHandler            
   .weak      TIM8_TRG_COM_TIM14_IRQHandler      
   .weak      TIM8_CC_IRQHandler   
   .weak      DMA1_Stream7_IRQHandler               
   .weak      FMC_IRQHandler            
   .weak      SDMMC1_IRQHandler            
   .weak      TIM5_IRQHandler            
   .weak      SPI3_IRQHandler            
   .weak      UART4_IRQHandler         
   .weak      UART5_IRQHandler         
   .weak      TIM6_DAC_IRQHandler                  
   .weak      TIM7_IRQHandler            
   .weak      DMA2_Stream0_IRQHandler               
   .weak      DMA2_Stream1_IRQHandler               
   .weak      DMA2_Stream2_IRQHandler               
   .weak      DMA2_Stream3_IRQHandler               
   .weak      DMA2_Stream4_IRQHandler               
   .weak      ETH_IRQHandler   
   .weak      ETH_WKUP_IRQHandler   
   .weak      CAN2_TX_IRQHandler   
   .weak      CAN2_RX0_IRQHandler                  
   .weak      CAN2_RX1_IRQHandler                  
   .weak      CAN2_SCE_IRQHandler                  
   .weak      OTG_FS_IRQHandler      
   .weak      DMA2_Stream5_IRQHandler               
   .weak      DMA2_Stream6_IRQHandler               
   .weak      DMA2_Stream7_IRQHandler               
   .weak      USART6_IRQHandler      
   .weak      I2C3_EV_IRQHandler   
   .weak      I2C3_ER_IRQHandler   
   .weak      OTG_HS_EP1_OUT_IRQHandler         
   .weak      OTG_HS_EP1_IN_IRQHandler            
   .weak      OTG_HS_WKUP_IRQHandler         
   .weak      OTG_HS_IRQHandler      
   .weak      DCMI_IRQHandler            
   .weak      RNG_IRQHandler            
   .weak      FPU_IRQHandler                  
   .weak      UART7_IRQHandler                  
   .weak      UART8_IRQHandler                  
   .weak      SPI4_IRQHandler            
   .weak      SPI5_IRQHandler            
   .weak      SPI6_IRQHandler            
   .weak      SAI1_IRQHandler            
   .weak      LTDC_IRQHandler            
   .weak      LTDC_ER_IRQHandler            
   .weak      DMA2D_IRQHandler            
   .weak      SAI2_IRQHandler            
   .weak      QUADSPI_IRQHandler            
   .weak      LPTIM1_IRQHandler            
   .weak      CEC_IRQHandler            
   .weak      I2C4_EV_IRQHandler            
   .weak      I2C4_ER_IRQHandler            
   .weak      SPDIF_RX_IRQHandler            
   .weak      DSI_IRQHandler            
   .weak      DFSDM1_FLT0_IRQHandler            
   .weak      DFSDM1_FLT1_IRQHandler            
   .weak      DFSDM1_FLT2_IRQHandler            
   .weak      DFSDM1_FLT3_IRQHandler            
   .weak      SDMMC2_IRQHandler            
   .weak      CAN3_TX_IRQHandler            
   .weak      CAN3_RX0_IRQHandler            
   .weak      CAN3_RX1_IRQHandler            
   .weak      CAN3_SCE_IRQHandler            
   .weak      JPEG_IRQHandler            
   .weak      MDIOS_IRQHandler            
#endif
/* USER CODE END 1 */
