/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbh_platform.c

  * @brief          : This file implements the USB platform
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
#include "usbh_platform.h"

/* USER CODE BEGIN INCLUDE */

/* USER CODE END INCLUDE */

/**
  * @brief  Drive VBUS.
  * @param  state : VBUS state
  *          This parameter can be one of the these values:
  *          - 1 : VBUS Active
  *          - 0 : VBUS Inactive
  */
void MX_DriverVbusHS(uint8_t state)
{
  uint8_t data = state;
  /* USER CODE BEGIN PREPARE_GPIO_DATA_VBUS_HS */
  if(state == 0)
  {
    /* Drive high Charge pump */
    data = GPIO_PIN_SET;
  }
  else
  {
    /* Drive low Charge pump */
    data = GPIO_PIN_RESET;
  }
  /* USER CODE END PREPARE_GPIO_DATA_VBUS_HS */
  HAL_GPIO_WritePin(GPIOF,GPIO_PIN_10,(GPIO_PinState)data);
}

