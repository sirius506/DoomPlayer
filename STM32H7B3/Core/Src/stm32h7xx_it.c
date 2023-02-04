/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
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
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32h7b3i_discovery_audio.h"
#include "stm32h7b3i_discovery_ts.h"
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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern HCD_HandleTypeDef hhcd_USB_OTG_HS;
extern SD_HandleTypeDef hsd1;
extern TIM_HandleTypeDef htim17;
extern UART_HandleTypeDef huart1;
extern LTDC_HandleTypeDef hlcd_ltdc;
extern HASH_HandleTypeDef hhash;
extern DMA_HandleTypeDef hdma_hash_in;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA2D_HandleTypeDef hlcd_dma2d;
extern JPEG_HandleTypeDef JPEG_Handle;

/* USER CODE BEGIN EV */
extern DMA_HandleTypeDef hdma_memtomem_dma2_stream1;
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
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
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles SDMMC1 global interrupt.
  */
void SDMMC1_IRQHandler(void)
{
  /* USER CODE BEGIN SDMMC1_IRQn 0 */

  /* USER CODE END SDMMC1_IRQn 0 */
  HAL_SD_IRQHandler(&hsd1);
  /* USER CODE BEGIN SDMMC1_IRQn 1 */

  /* USER CODE END SDMMC1_IRQn 1 */
}

/**
  * @brief This function handles EXTI line[15:10] interrupts.
  */
void EXTI15_10_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */

  /* USER CODE END EXTI15_10_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(WAKEUP_Pin);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

/**
  * @brief This function handles USB On The Go HS End Point 1 Out global interrupt.
  */
void OTG_HS_EP1_OUT_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_HS_EP1_OUT_IRQn 0 */

  /* USER CODE END OTG_HS_EP1_OUT_IRQn 0 */
  HAL_HCD_IRQHandler(&hhcd_USB_OTG_HS);
  /* USER CODE BEGIN OTG_HS_EP1_OUT_IRQn 1 */

  /* USER CODE END OTG_HS_EP1_OUT_IRQn 1 */
}

/**
  * @brief This function handles USB On The Go HS End Point 1 In global interrupt.
  */
void OTG_HS_EP1_IN_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_HS_EP1_IN_IRQn 0 */

  /* USER CODE END OTG_HS_EP1_IN_IRQn 0 */
  HAL_HCD_IRQHandler(&hhcd_USB_OTG_HS);
  /* USER CODE BEGIN OTG_HS_EP1_IN_IRQn 1 */

  /* USER CODE END OTG_HS_EP1_IN_IRQn 1 */
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
  * @brief This function handles TIM17 global interrupt.
  */
void TIM17_IRQHandler(void)
{
  /* USER CODE BEGIN TIM17_IRQn 0 */

  /* USER CODE END TIM17_IRQn 0 */
  HAL_TIM_IRQHandler(&htim17);
  /* USER CODE BEGIN TIM17_IRQn 1 */

  /* USER CODE END TIM17_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/**
  * @brief This function handles DMA1 stream1 global interrupt.
  */
void DMA1_Stream0_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream0_IRQn 0 */

  /* USER CODE END DMA1_Stream0_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_hash_in);
  /* USER CODE BEGIN DMA1_Stream0_IRQn 1 */

  /* USER CODE END DMA1_Stream0_IRQn 1 */
}

void DMA1_Stream1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

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
  * @brief This function handles DMA2 stream1 global interrupt.
  */
void DMA2_Stream1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Stream1_IRQn 0 */

  /* USER CODE END DMA2_Stream1_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_memtomem_dma2_stream1);
  /* USER CODE BEGIN DMA2_Stream1_IRQn 1 */

  /* USER CODE END DMA2_Stream1_IRQn 1 */
}

#ifdef USE_MIC
/**
  * @brief  This function handles DMA2 Stream0 interrupt request.
  * @retval None
  */
void DMA2_Stream0_IRQHandler(void)
{
  BSP_AUDIO_IN_IRQHandler(1, AUDIO_IN_DEVICE_DIGITAL_MIC2);
}

/**
  * @brief  This function handles DMA2 Stream2 interrupt request.
  * @retval None
  */
void DMA2_Stream2_IRQHandler(void)
{
  BSP_AUDIO_IN_IRQHandler(1, AUDIO_IN_DEVICE_DIGITAL_MIC4);
}

/**
  * @brief  This function handles DMA2 Stream3 interrupt request.
  * @retval None
  */
void DMA2_Stream3_IRQHandler(void)
{
  BSP_AUDIO_IN_IRQHandler(1, AUDIO_IN_DEVICE_DIGITAL_MIC3);
}

/**
  * @brief  This function handles DMA2 Stream4 interrupt request.
  * @retval None
  */
void DMA2_Stream4_IRQHandler(void)
{
  BSP_AUDIO_IN_IRQHandler(0, AUDIO_IN_DEVICE_ANALOG_MIC);
}
#endif

/**
  * @brief  This function handles DMA2 Stream6 interrupt request.
  * @param  None
  * @retval None
  */
void DMA2_Stream6_IRQHandler(void)
{
  BSP_AUDIO_OUT_IRQHandler(0);
}

#ifdef USE_MIC
/**
  * @brief  This function handles DMA2 Stream7 interrupt request.
  * @retval None
  */
void DMA2_Stream7_IRQHandler(void)
{
  BSP_AUDIO_IN_IRQHandler(1, AUDIO_IN_DEVICE_DIGITAL_MIC1);
}
#endif

/**
  * @brief  This function BDMA Channel0 interrupt request.
  * @retval None
  */
void BDMA_Channel0_IRQHandler(void)
{
  BSP_AUDIO_OUT_IRQHandler(1);
}

#ifdef USE_MIC
/**
  * @brief  This function handles BDMA Channel6 interrupt request.
  * @retval None
  */
void BDMA2_Channel6_IRQHandler(void)
{
  BSP_AUDIO_IN_IRQHandler(1, AUDIO_IN_DEVICE_DIGITAL_MIC5);
}
#endif

void EXTI2_IRQHandler(void)
{
  BSP_TS_IRQHandler(0);
}

/**
  * @brief This function handles LTDC global interrupt.
  */
void LTDC_IRQHandler(void)
{
  /* USER CODE BEGIN LTDC_IRQn 0 */

  /* USER CODE END LTDC_IRQn 0 */
  HAL_LTDC_IRQHandler(&hlcd_ltdc);
  /* USER CODE BEGIN LTDC_IRQn 1 */

  /* USER CODE END LTDC_IRQn 1 */
}

/**
  * @brief This function handles HASH and RNG global interrupts.
  */
void HASH_RNG_IRQHandler(void)
{
  /* USER CODE BEGIN HASH_RNG_IRQn 0 */

  /* USER CODE END HASH_RNG_IRQn 0 */
  HAL_HASH_IRQHandler(&hhash);
  /* USER CODE BEGIN HASH_RNG_IRQn 1 */

  /* USER CODE END HASH_RNG_IRQn 1 */
}

/**
  * @brief This function handles DMA2D global interrupt.
  */
void DMA2D_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2D_IRQn 0 */

  /* USER CODE END DMA2D_IRQn 0 */
  HAL_DMA2D_IRQHandler(&hlcd_dma2d);
  /* USER CODE BEGIN DMA2D_IRQn 1 */

  /* USER CODE END DMA2D_IRQn 1 */
}

void JPEG_IRQHandler(void)
{
  HAL_JPEG_IRQHandler(&JPEG_Handle);
}

/**
  * @brief  This function handles MDMA interrupt request.
  * @param  None
  * @retval None
  */

void MDMA_IRQHandler()
{
  /* Check the interrupt and clear flag */
  HAL_MDMA_IRQHandler(JPEG_Handle.hdmain);
  HAL_MDMA_IRQHandler(JPEG_Handle.hdmaout);
}

/* USER CODE END 1 */
