/**
  ******************************************************************************
  * @file    usbh_hid_keybd.c
  * @author  MCD Application Team
  * @brief   This file is the application layer for USB Host HID Keyboard handling
  *          QWERTY and AZERTY Keyboard are supported as per the selection in
  *          usbh_hid_keybd.h
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* BSPDependencies
- "stm32xxxxx_{eval}{discovery}{nucleo_144}.c"
- "stm32xxxxx_{eval}{discovery}_io.c"
- "stm32xxxxx_{eval}{discovery}{adafruit}_lcd.c"
- "stm32xxxxx_{eval}{discovery}_sdram.c"
EndBSPDependencies */

/* Includes ------------------------------------------------------------------*/
#include "usbh_hid_keybd.h"
#include "usbh_hid_parser.h"
#include "app_task.h"

/** @addtogroup USBH_LIB
  * @{
  */

/** @addtogroup USBH_CLASS
  * @{
  */

/** @addtogroup USBH_HID_CLASS
  * @{
  */

/** @defgroup USBH_HID_KEYBD
  * @brief    This file includes HID Layer Handlers for USB Host HID class.
  * @{
  */

/** @defgroup USBH_HID_KEYBD_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */


/** @defgroup USBH_HID_KEYBD_Private_Defines
  * @{
  */

/**
  * @}
  */

/** @defgroup USBH_HID_KEYBD_Private_Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup USBH_HID_KEYBD_Private_FunctionPrototypes
  * @{
  */
/**
  * @}
  */

/** @defgroup USBH_HID_KEYBD_Private_Variables
  * @{
  */

HID_KEYBD_Info_TypeDef    keybd_info;
uint8_t                   keybd_rx_report_buf[USBH_HID_KEYBD_REPORT_SIZE];

/**
  * @brief  USBH_HID_KeybdInit
  *         The function init the HID keyboard.
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_KeybdInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) pclass->pData;

  memset(&keybd_info, 0, sizeof(keybd_info));

  HID_Handle->pData = keybd_rx_report_buf;

  return USBH_OK;
}

#define	NUM_KEYS	6
static uint8_t lastkeys[NUM_KEYS];
static uint8_t lastmod;

extern void send_key_event(HID_HandleTypeDef *HID_Handle, int evcode, int kbdcode, int modkey);
extern void send_modkey_event(HID_HandleTypeDef *HID_Handle, int evcode, int bpos);

/**
  * @brief  USBH_HID_KeybdDecode
  *         The function decode keyboard data.
  * @param  phost: Host handle
  * @retval USBH Status
  */
void USBH_HID_KeybdDecode(HID_HandleTypeDef *HID_Handle)
{
  HID_KEYBD_Info_TypeDef kbdinfo;
  int lpos, cpos;

  if (HID_Handle->length == sizeof(kbdinfo))
  {
    SCB_InvalidateDCache_by_Addr((uint32_t *)HID_Handle->pData, sizeof(kbdinfo));
    memcpy(&kbdinfo, HID_Handle->pData, sizeof(kbdinfo));
#ifdef KBD_DEBUG
    debug_printf("Key %x: %02x %02x %02x\n", kbdinfo.modkey, kbdinfo.keys[0], kbdinfo.keys[1], kbdinfo.keys[2]);
#endif

    lpos = 0;
    for (cpos = 0; cpos < NUM_KEYS; cpos++)
    {
      if (kbdinfo.keys[cpos] != lastkeys[lpos])
      {
        if (lastkeys[lpos] == 0)
        {
          do
          {
#ifdef KBD_DEBUG
            debug_printf("Press %02x\n", kbdinfo.keys[cpos]);
#endif
            send_key_event(HID_Handle, KBDEVENT_DOWN, kbdinfo.keys[cpos], kbdinfo.modkey);
            cpos++;
          }
          while ((cpos < NUM_KEYS) && (kbdinfo.keys[cpos]));
          goto lcopy;
        }
        else if (kbdinfo.keys[cpos] == 0)
        {
          do
          {
#ifdef KBD_DEBUG
            debug_printf("Release %02x\n", lastkeys[lpos]);
#endif
            send_key_event(HID_Handle, KBDEVENT_UP, lastkeys[lpos], kbdinfo.modkey);
            lpos++;
          }
          while ((lpos < NUM_KEYS) && (lastkeys[lpos]));
          goto lcopy;
        }
        else
        {
          do
          {
#ifdef KBD_DEBUG
            debug_printf("Release2 %02x\n", lastkeys[lpos]);
#endif
            send_key_event(HID_Handle, KBDEVENT_UP, lastkeys[lpos], kbdinfo.modkey);
            lpos++;
          }
          while ((lpos < NUM_KEYS) && (lastkeys[lpos] != kbdinfo.keys[cpos]));
          goto lcopy;
        }
      }
      else
      {
        lpos++;
      }
    }
lcopy:
    memcpy(lastkeys, kbdinfo.keys, NUM_KEYS);
    if (lastmod != kbdinfo.modkey)
    {
      uint8_t cmask, bpos;

      cmask = lastmod ^ kbdinfo.modkey;
      if (cmask)
      {
        for (bpos = 0; bpos < 8; bpos++)
        {
          if (cmask & (1 << bpos))
          {
            send_modkey_event(HID_Handle, (lastmod & (1<<bpos))? KBDEVENT_UP : KBDEVENT_DOWN, bpos);
          }
        }
      }
      lastmod = kbdinfo.modkey;
    }
  }
}
