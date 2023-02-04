#ifndef __USB_HID_DUALSENSE_H__
#define __USB_HID_DUALSENSE_H__

#include "usbh_hid.h"
#include "dualsense_report.h"

typedef enum {
  HID_SOF_FLAG = 1,
  HID_LVGL_BIT = 1<<1,
  HID_DOOM_BIT = 1<<2,
  HID_TEST_BIT = 1<<3,
} HID_EV_FLAG;

USBH_StatusTypeDef USBH_HID_DualSenseInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
HID_DualSense_Info_TypeDef *USBH_HID_GetDualSenseInfo(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
void USBH_HID_SetDualSenseLightbar(USBH_ClassTypeDef *pclass);
void USBH_HID_DualSenseDecode(HID_HandleTypeDef *HID_Handle);

extern void UpdateBarColor(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost, uint8_t uc);
extern void DualSenseSetup(USBH_ClassTypeDef *pclass);
void SetupTrigger(USBH_ClassTypeDef *pclass);

extern void Generate_LVGL_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton);
extern void Generate_DOOM_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton);
extern void Display_DualSense_Info(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton);
extern void DualSenseResetFusion();

#endif
