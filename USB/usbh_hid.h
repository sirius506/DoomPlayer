#ifndef __USBH_HID_H
#define __USBH_HID_H

#include "usbh_core.h"

#define USB_HID_CLASS                                 0x03U
extern USBH_ClassTypeDef  HID_Class;
#define USBH_HID_CLASS    &HID_Class

#define HID_MIN_POLL                                3U
#define HID_REPORT_SIZE                             16U
#define HID_MAX_USAGE                               10U
#define HID_MAX_NBR_REPORT_FMT                      10U
#define HID_QUEUE_SIZE                              10U

/* States for HID State Machine */
typedef enum
{
  HID_INIT = 0,
  HID_IDLE,
  HID_SEND_DATA,
  HID_BUSY,
  HID_GET_DATA,
  HID_SYNC,
  HID_POLL,
  HID_ERROR,
}
HID_StateTypeDef;

typedef enum
{
  HID_REQ_INIT = 0,
  HID_REQ_IDLE,
  HID_REQ_GET_REPORT_DESC,
  HID_REQ_GET_HID_DESC,
  HID_REQ_SET_IDLE,
  HID_REQ_SET_PROTOCOL,
  HID_REQ_SET_REPORT,

}
HID_CtlStateTypeDef;

typedef enum
{
  HID_MOUSE    = 0x01,
  HID_KEYBOARD = 0x02,
  HID_UNKNOWN = 0xFF,
}
HID_TypeTypeDef;

typedef struct _HIDDescriptor
{
  uint8_t   bLength;
  uint8_t   bDescriptorType;
  uint16_t  bcdHID;               /* indicates what endpoint this descriptor is describing */
  uint8_t   bCountryCode;        /* specifies the transfer type. */
  uint8_t   bNumDescriptors;     /* specifies the transfer type. */
  uint8_t   bReportDescriptorType;    /* Maximum Packet Size this endpoint is capable of sending or receiving */
  uint16_t  wItemLength;          /* is used to specify the polling interval of certain transfers. */
}
HID_DescTypeDef;

typedef struct
{
  uint8_t  *buf;
  uint16_t  head;
  uint16_t tail;
  uint16_t size;
  uint8_t  lock;
} FIFO_TypeDef;


#define	HID_MODE_LVGL	0
#define	HID_MODE_TEST	1
#define	HID_MODE_DOOM	2

/* Structure for HID process */
typedef struct _HID_Process
{
  uint8_t              OutPipe;
  uint8_t              InPipe;
  HID_StateTypeDef     state;
  uint8_t              OutEp;
  uint8_t              InEp;
  HID_CtlStateTypeDef  ctl_state;
  FIFO_TypeDef         fifo;
  uint8_t              devType;
  uint8_t              *pData;
  uint16_t             length;
  uint8_t              ep_addr;
  uint16_t             poll;
  uint32_t             timer;
  uint8_t              DataReady;
  uint8_t              hid_mode;
  HID_DescTypeDef      HID_Desc;
  osMessageQueueId_t   kbdqId;
  USBH_StatusTypeDef(* Init)(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
}
HID_HandleTypeDef;

#define	DEVTYPE_KEYBOARD	0
#define	DEVTYPE_DUALSENSE	1

#define USB_HID_GET_REPORT                            0x01U
#define USB_HID_GET_IDLE                              0x02U
#define USB_HID_GET_PROTOCOL                          0x03U
#define USB_HID_SET_REPORT                            0x09U
#define USB_HID_SET_IDLE                              0x0AU
#define USB_HID_SET_PROTOCOL                          0x0BU

/* Interface Descriptor field values for HID Boot Protocol */
#define HID_BOOT_CODE                                 0x01U
#define HID_KEYBRD_BOOT_CODE                          0x01U
#define HID_MOUSE_BOOT_CODE                           0x02U

USBH_StatusTypeDef USBH_HID_GetHIDReportDescriptor(USBH_HandleTypeDef *phost, uint16_t length);

void HID_Set_DoomMode();
void HID_Set_TestMode();
void HID_Set_LVGLMode();

void USBH_HID_FifoInit(FIFO_TypeDef *f, uint8_t *buf, uint16_t size);
USBH_StatusTypeDef USBH_HID_GetReport(USBH_HandleTypeDef *phost, uint8_t reportType,
                                      uint8_t reportId, uint8_t *reportBuff, uint8_t reportLen);
#endif
