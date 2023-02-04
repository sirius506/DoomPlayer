/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include "SEGGER_RTT.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SAI1_FSA_Pin GPIO_PIN_4
#define SAI1_FSA_GPIO_Port GPIOE
#define SAI1_SDB_Pin GPIO_PIN_3
#define SAI1_SDB_GPIO_Port GPIOE
#define QSPI_D2_Pin GPIO_PIN_2
#define QSPI_D2_GPIO_Port GPIOE
#define RMII_TXD1_Pin GPIO_PIN_14
#define RMII_TXD1_GPIO_Port GPIOG
#define FMC_NBL1_Pin GPIO_PIN_1
#define FMC_NBL1_GPIO_Port GPIOE
#define FMC_NBL0_Pin GPIO_PIN_0
#define FMC_NBL0_GPIO_Port GPIOE
#define ARDUINO_SCL_D15_Pin GPIO_PIN_8
#define ARDUINO_SCL_D15_GPIO_Port GPIOB
#define ULPI_D7_Pin GPIO_PIN_5
#define ULPI_D7_GPIO_Port GPIOB
#define uSD_D3_Pin GPIO_PIN_4
#define uSD_D3_GPIO_Port GPIOB
#define uSD_D2_Pin GPIO_PIN_3
#define uSD_D2_GPIO_Port GPIOB
#define uSD_CMD_Pin GPIO_PIN_7
#define uSD_CMD_GPIO_Port GPIOD
#define WIFI_RX_Pin GPIO_PIN_12
#define WIFI_RX_GPIO_Port GPIOC
#define CEC_Pin GPIO_PIN_15
#define CEC_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SAI1_SCKA_Pin GPIO_PIN_5
#define SAI1_SCKA_GPIO_Port GPIOE
#define SAI1_SDA_Pin GPIO_PIN_6
#define SAI1_SDA_GPIO_Port GPIOE
#define RMII_TXD0_Pin GPIO_PIN_13
#define RMII_TXD0_GPIO_Port GPIOG
#define ARDUINO_SDA_D14_Pin GPIO_PIN_9
#define ARDUINO_SDA_D14_GPIO_Port GPIOB
#define AUDIO_SDA_Pin GPIO_PIN_7
#define AUDIO_SDA_GPIO_Port GPIOB
#define QSPI_NCS_Pin GPIO_PIN_6
#define QSPI_NCS_GPIO_Port GPIOB
#define FMC_SDNCAS_Pin GPIO_PIN_15
#define FMC_SDNCAS_GPIO_Port GPIOG
#define RMII_TX_EN_Pin GPIO_PIN_11
#define RMII_TX_EN_GPIO_Port GPIOG
#define LD_USER1_Pin GPIO_PIN_13
#define LD_USER1_GPIO_Port GPIOJ
#define Audio_INT_Pin GPIO_PIN_12
#define Audio_INT_GPIO_Port GPIOJ
#define uSD_CLK_Pin GPIO_PIN_6
#define uSD_CLK_GPIO_Port GPIOD
#define FMC_D2_Pin GPIO_PIN_0
#define FMC_D2_GPIO_Port GPIOD
#define DFSDM_DATIN5_Pin GPIO_PIN_11
#define DFSDM_DATIN5_GPIO_Port GPIOC
#define QSPI_D1_Pin GPIO_PIN_10
#define QSPI_D1_GPIO_Port GPIOC
#define ARD_D13_SCK_Pin GPIO_PIN_12
#define ARD_D13_SCK_GPIO_Port GPIOA
#define NC4_Pin GPIO_PIN_8
#define NC4_GPIO_Port GPIOI
#define FMC_NBL2_Pin GPIO_PIN_4
#define FMC_NBL2_GPIO_Port GPIOI
#define NC3_Pin GPIO_PIN_7
#define NC3_GPIO_Port GPIOK
#define NC2_Pin GPIO_PIN_6
#define NC2_GPIO_Port GPIOK
#define NC1_Pin GPIO_PIN_5
#define NC1_GPIO_Port GPIOK
#define SPDIF_RX_Pin GPIO_PIN_12
#define SPDIF_RX_GPIO_Port GPIOG
#define uSD_D1_Pin GPIO_PIN_10
#define uSD_D1_GPIO_Port GPIOG
#define WIFI_RST_Pin GPIO_PIN_14
#define WIFI_RST_GPIO_Port GPIOJ
#define RMII_RXER_Pin GPIO_PIN_5
#define RMII_RXER_GPIO_Port GPIOD
#define DFSDM_CKOUT_Pin GPIO_PIN_3
#define DFSDM_CKOUT_GPIO_Port GPIOD
#define FMC_D3_Pin GPIO_PIN_1
#define FMC_D3_GPIO_Port GPIOD
#define D27_Pin GPIO_PIN_3
#define D27_GPIO_Port GPIOI
#define D26_Pin GPIO_PIN_2
#define D26_GPIO_Port GPIOI
#define SPI2_NSS_Pin GPIO_PIN_11
#define SPI2_NSS_GPIO_Port GPIOA
#define FMC_A0_Pin GPIO_PIN_0
#define FMC_A0_GPIO_Port GPIOF
#define FMC_NBL3_Pin GPIO_PIN_5
#define FMC_NBL3_GPIO_Port GPIOI
#define D29_Pin GPIO_PIN_7
#define D29_GPIO_Port GPIOI
#define D31_Pin GPIO_PIN_10
#define D31_GPIO_Port GPIOI
#define D28_Pin GPIO_PIN_6
#define D28_GPIO_Port GPIOI
#define NC8_Pin GPIO_PIN_4
#define NC8_GPIO_Port GPIOK
#define NC7_Pin GPIO_PIN_3
#define NC7_GPIO_Port GPIOK
#define uSD_D0_Pin GPIO_PIN_9
#define uSD_D0_GPIO_Port GPIOG
#define DSI_RESET_Pin GPIO_PIN_15
#define DSI_RESET_GPIO_Port GPIOJ
#define OTG_FS_OverCurrent_Pin GPIO_PIN_4
#define OTG_FS_OverCurrent_GPIO_Port GPIOD
#define WIFI_TX_Pin GPIO_PIN_2
#define WIFI_TX_GPIO_Port GPIOD
#define D23_Pin GPIO_PIN_15
#define D23_GPIO_Port GPIOH
#define D25_Pin GPIO_PIN_1
#define D25_GPIO_Port GPIOI
#define VCP_RX_Pin GPIO_PIN_10
#define VCP_RX_GPIO_Port GPIOA
#define RCC_OSC32_IN_Pin GPIO_PIN_14
#define RCC_OSC32_IN_GPIO_Port GPIOC
#define FMC_A1_Pin GPIO_PIN_1
#define FMC_A1_GPIO_Port GPIOF
#define NC5_Pin GPIO_PIN_12
#define NC5_GPIO_Port GPIOI
#define D30_Pin GPIO_PIN_9
#define D30_GPIO_Port GPIOI
#define D21_Pin GPIO_PIN_13
#define D21_GPIO_Port GPIOH
#define D22_Pin GPIO_PIN_14
#define D22_GPIO_Port GPIOH
#define D24_Pin GPIO_PIN_0
#define D24_GPIO_Port GPIOI
#define VCP_TX_Pin GPIO_PIN_9
#define VCP_TX_GPIO_Port GPIOA
#define RCC_OSC32_OUT_Pin GPIO_PIN_15
#define RCC_OSC32_OUT_GPIO_Port GPIOC
#define ULPI_DIR_Pin GPIO_PIN_11
#define ULPI_DIR_GPIO_Port GPIOI
#define QSPI_D0_Pin GPIO_PIN_9
#define QSPI_D0_GPIO_Port GPIOC
#define CEC_CLK_Pin GPIO_PIN_8
#define CEC_CLK_GPIO_Port GPIOA
#define OSC_25M_Pin GPIO_PIN_0
#define OSC_25M_GPIO_Port GPIOH
#define FMC_A2_Pin GPIO_PIN_2
#define FMC_A2_GPIO_Port GPIOF
#define LCD_INT_Pin GPIO_PIN_13
#define LCD_INT_GPIO_Port GPIOI
#define uSD_Detect_Pin GPIO_PIN_15
#define uSD_Detect_GPIO_Port GPIOI
#define ARD_D5_PWM_Pin GPIO_PIN_8
#define ARD_D5_PWM_GPIO_Port GPIOC
#define ARD_D0_RX_Pin GPIO_PIN_7
#define ARD_D0_RX_GPIO_Port GPIOC
#define FMC_A3_Pin GPIO_PIN_3
#define FMC_A3_GPIO_Port GPIOF
#define LCD_BL_CTRL_Pin GPIO_PIN_14
#define LCD_BL_CTRL_GPIO_Port GPIOI
#define ULPI_NXT_Pin GPIO_PIN_4
#define ULPI_NXT_GPIO_Port GPIOH
#define FMC_SDCLK_Pin GPIO_PIN_8
#define FMC_SDCLK_GPIO_Port GPIOG
#define ARDUINO_TX_D1_Pin GPIO_PIN_6
#define ARDUINO_TX_D1_GPIO_Port GPIOC
#define FMC_A4_Pin GPIO_PIN_4
#define FMC_A4_GPIO_Port GPIOF
#define FMC_SDNME_Pin GPIO_PIN_5
#define FMC_SDNME_GPIO_Port GPIOH
#define FMC_SDNE0_Pin GPIO_PIN_3
#define FMC_SDNE0_GPIO_Port GPIOH
#define SAI1_MCLKA_Pin GPIO_PIN_7
#define SAI1_MCLKA_GPIO_Port GPIOG
#define EXT_SDA_Pin GPIO_PIN_6
#define EXT_SDA_GPIO_Port GPIOG
#define ARD_D6_PWM_Pin GPIO_PIN_7
#define ARD_D6_PWM_GPIO_Port GPIOF
#define ARD_D3_PWM_Pin GPIO_PIN_6
#define ARD_D3_PWM_GPIO_Port GPIOF
#define FMC_A5_Pin GPIO_PIN_5
#define FMC_A5_GPIO_Port GPIOF
#define FMC_SDCKE0_Pin GPIO_PIN_2
#define FMC_SDCKE0_GPIO_Port GPIOH
#define FMC_D1_Pin GPIO_PIN_15
#define FMC_D1_GPIO_Port GPIOD
#define ULPI_D6_Pin GPIO_PIN_13
#define ULPI_D6_GPIO_Port GPIOB
#define FMC_D15_Pin GPIO_PIN_10
#define FMC_D15_GPIO_Port GPIOD
#define ARDUINO_A1_Pin GPIO_PIN_10
#define ARDUINO_A1_GPIO_Port GPIOF
#define ARDUINO_A2_Pin GPIO_PIN_9
#define ARDUINO_A2_GPIO_Port GPIOF
#define ARDUINO_A3_Pin GPIO_PIN_8
#define ARDUINO_A3_GPIO_Port GPIOF
#define FMC_D0_Pin GPIO_PIN_14
#define FMC_D0_GPIO_Port GPIOD
#define ULPI_D5_Pin GPIO_PIN_12
#define ULPI_D5_GPIO_Port GPIOB
#define FMC_D14_Pin GPIO_PIN_9
#define FMC_D14_GPIO_Port GPIOD
#define FMC_D13_Pin GPIO_PIN_8
#define FMC_D13_GPIO_Port GPIOD
#define ULPI_STP_Pin GPIO_PIN_0
#define ULPI_STP_GPIO_Port GPIOC
#define RMII_MDC_Pin GPIO_PIN_1
#define RMII_MDC_GPIO_Port GPIOC
#define ARD_A2_Pin GPIO_PIN_2
#define ARD_A2_GPIO_Port GPIOC
#define FMC_A6_Pin GPIO_PIN_12
#define FMC_A6_GPIO_Port GPIOF
#define FMC_A11_Pin GPIO_PIN_1
#define FMC_A11_GPIO_Port GPIOG
#define FMC_A9_Pin GPIO_PIN_15
#define FMC_A9_GPIO_Port GPIOF
#define ARD_D8_Pin GPIO_PIN_4
#define ARD_D8_GPIO_Port GPIOJ
#define AUDIO_SCL_Pin GPIO_PIN_12
#define AUDIO_SCL_GPIO_Port GPIOD
#define QSPI_D3_Pin GPIO_PIN_13
#define QSPI_D3_GPIO_Port GPIOD
#define EXT_SCL_Pin GPIO_PIN_3
#define EXT_SCL_GPIO_Port GPIOG
#define FMC_A12_Pin GPIO_PIN_2
#define FMC_A12_GPIO_Port GPIOG
#define LD_USER2_Pin GPIO_PIN_5
#define LD_USER2_GPIO_Port GPIOJ
#define D20_Pin GPIO_PIN_12
#define D20_GPIO_Port GPIOH
#define RMII_REF_CLK_Pin GPIO_PIN_1
#define RMII_REF_CLK_GPIO_Port GPIOA
#define B_USER_Pin GPIO_PIN_0
#define B_USER_GPIO_Port GPIOA
#define ARD_A1_Pin GPIO_PIN_4
#define ARD_A1_GPIO_Port GPIOA
#define RMII_RXD0_Pin GPIO_PIN_4
#define RMII_RXD0_GPIO_Port GPIOC
#define FMC_A7_Pin GPIO_PIN_13
#define FMC_A7_GPIO_Port GPIOF
#define FMC_A10_Pin GPIO_PIN_0
#define FMC_A10_GPIO_Port GPIOG
#define ARD_D7_Pin GPIO_PIN_3
#define ARD_D7_GPIO_Port GPIOJ
#define FMC_D5_Pin GPIO_PIN_8
#define FMC_D5_GPIO_Port GPIOE
#define SPDIF_TX_Pin GPIO_PIN_11
#define SPDIF_TX_GPIO_Port GPIOD
#define FMC_BA1_Pin GPIO_PIN_5
#define FMC_BA1_GPIO_Port GPIOG
#define FMC_BA0_Pin GPIO_PIN_4
#define FMC_BA0_GPIO_Port GPIOG
#define FMC_D_7_Pin GPIO_PIN_9
#define FMC_D_7_GPIO_Port GPIOH
#define FMC_D19_Pin GPIO_PIN_11
#define FMC_D19_GPIO_Port GPIOH
#define RMII_MDIO_Pin GPIO_PIN_2
#define RMII_MDIO_GPIO_Port GPIOA
#define ARD_A0_Pin GPIO_PIN_6
#define ARD_A0_GPIO_Port GPIOA
#define ULPI_CLK_Pin GPIO_PIN_5
#define ULPI_CLK_GPIO_Port GPIOA
#define RMII_RXD1_Pin GPIO_PIN_5
#define RMII_RXD1_GPIO_Port GPIOC
#define FMC_A8_Pin GPIO_PIN_14
#define FMC_A8_GPIO_Port GPIOF
#define DSIHOST_TE_Pin GPIO_PIN_2
#define DSIHOST_TE_GPIO_Port GPIOJ
#define FMC_SDNRAS_Pin GPIO_PIN_11
#define FMC_SDNRAS_GPIO_Port GPIOF
#define FMC_D6_Pin GPIO_PIN_9
#define FMC_D6_GPIO_Port GPIOE
#define FMC_D8_Pin GPIO_PIN_11
#define FMC_D8_GPIO_Port GPIOE
#define FMC_D11_Pin GPIO_PIN_14
#define FMC_D11_GPIO_Port GPIOE
#define ULPI_D3_Pin GPIO_PIN_10
#define ULPI_D3_GPIO_Port GPIOB
#define ARDUINO_PWM_D6_Pin GPIO_PIN_6
#define ARDUINO_PWM_D6_GPIO_Port GPIOH
#define FMC_D16_Pin GPIO_PIN_8
#define FMC_D16_GPIO_Port GPIOH
#define FMC_D18_Pin GPIO_PIN_10
#define FMC_D18_GPIO_Port GPIOH
#define ULPI_D0_Pin GPIO_PIN_3
#define ULPI_D0_GPIO_Port GPIOA
#define RMII_CRS_DV_Pin GPIO_PIN_7
#define RMII_CRS_DV_GPIO_Port GPIOA
#define ULPI_D2_Pin GPIO_PIN_1
#define ULPI_D2_GPIO_Port GPIOB
#define ULPI_D1_Pin GPIO_PIN_0
#define ULPI_D1_GPIO_Port GPIOB
#define ARD_D4_Pin GPIO_PIN_0
#define ARD_D4_GPIO_Port GPIOJ
#define ARD_D2_Pin GPIO_PIN_1
#define ARD_D2_GPIO_Port GPIOJ
#define FMC_D4_Pin GPIO_PIN_7
#define FMC_D4_GPIO_Port GPIOE
#define FMC_D7_Pin GPIO_PIN_10
#define FMC_D7_GPIO_Port GPIOE
#define FMC_D9_Pin GPIO_PIN_12
#define FMC_D9_GPIO_Port GPIOE
#define FMC_D12_Pin GPIO_PIN_15
#define FMC_D12_GPIO_Port GPIOE
#define FMC_D10_Pin GPIO_PIN_13
#define FMC_D10_GPIO_Port GPIOE
#define ULPI_D4_Pin GPIO_PIN_11
#define ULPI_D4_GPIO_Port GPIOB
#define ARDUINO_MISO_D12_Pin GPIO_PIN_14
#define ARDUINO_MISO_D12_GPIO_Port GPIOB
#define ARDUINO_MOSI_PWM_D11_Pin GPIO_PIN_15
#define ARDUINO_MOSI_PWM_D11_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

#define SECTION_DTCMRAM  __attribute__((section(".DTCMSection")))
#define SECTION_AHBSRAM  __attribute__((section(".AHBSRAMSection")))
#define SECTION_AHBSRAM2 __attribute__((section(".AHBSRAM2Section")))
#define SECTION_SRDSRAM  __attribute__((section(".SRDSRAMSection")))
#define SECTION_FBRAM    __attribute__((section(".FBRAMSection")))
#define	SECTION_SDRAM    __attribute__((section(".SDRAMSection")))

#define debug_printf(...)       SEGGER_RTT_printf(0, __VA_ARGS__)

#define	TEST0_Pin	GPIO_PIN_7
#define	TEST0_GPIO_Port	GPIOC
#define	TEST1_Pin	GPIO_PIN_6
#define	TEST1_GPIO_Port	GPIOC
#define	TEST2_Pin	GPIO_PIN_1
#define	TEST2_GPIO_Port	GPIOJ
#define	TEST3_Pin	GPIO_PIN_6
#define	TEST3_GPIO_Port	GPIOF

#define	SOF_Pin		TEST0_Pin
#define	SOF_Port	TEST0_GPIO_Port
#define	AUDIO_REQ_Pin	TEST1_Pin
#define	AUDIO_REQ_Port	TEST1_GPIO_Port
#define	HID_REQ_Pin	TEST2_Pin
#define	HID_REQ_Port	TEST2_GPIO_Port
#define	SLEEP_Pin	TEST3_Pin
#define	SLEEP_Port	TEST3_GPIO_Port

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
