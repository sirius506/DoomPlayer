#ifndef _USB_HID_DUALSENSE_H
#define _USB_HID_DUALSENSE_H

#include "usbh_hid.h"
#include "dualsense_report.h"

typedef enum {
  HID_SOF_FLAG = 1<<8,
  HID_LVGL_BIT = 1<<9,
  HID_DOOM_BIT = 1<<10,
  HID_TEST_BIT = 1<<11,
} HID_EV_FLAG;

#endif
