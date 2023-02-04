#include "main.h"
#include "cmsis_os.h"
#include "usb_host.h"
#include "usbh_core.h"
#include "usbh_hid.h"
#include "usbh_audio.h"
#include "stm32f769i_discovery_conf.h"

extern SPI_HandleTypeDef hspi5;
extern USBH_StatusTypeDef  USBH_Start(USBH_HandleTypeDef *phost);
extern USBH_StatusTypeDef USBH_RegisterClass(USBH_HandleTypeDef *phost, USBH_ClassTypeDef *pclass);

SECTION_AHBSRAM USBH_HandleTypeDef hUsbHostHS;

void URBChangeCallback(HCD_HandleTypeDef *hhcd, uint8_t channel, USB_OTG_URBStateTypeDef state)
{
  USBH_HandleTypeDef *phost;
  osEventFlagsId_t evflag;

  phost = (USBH_HandleTypeDef *)hhcd->pData;
  evflag = phost->PipeFlags[channel & 0x0F];
  if (evflag)
  {
     switch (state)
     {
     case URB_IDLE:
       //osEventFlagsSet(evflag, EVF_URB_IDLE);
       //break;
     case URB_DONE:
       osEventFlagsSet(evflag, EVF_URB_DONE);
       break;
     case URB_NOTREADY:
       osEventFlagsSet(evflag, EVF_URB_NOTREADY);
debug_printf("URB_NOTREADY\n");
       break;
     case URB_NYET:
       osEventFlagsSet(evflag, EVF_URB_NYET);
debug_printf("URB_NOT_YET\n");
       break;
     case URB_ERROR:
       osEventFlagsSet(evflag, EVF_URB_ERROR);
debug_printf("URB_ERROR\n");
       break;
     case URB_STALL:
debug_printf("URB_STALL\n");
       osEventFlagsSet(evflag, EVF_URB_STALL);
       break;
     default:
debug_printf("state = %x\n", state);
       break;
     }
  }
  else if (phost->urb_thread)
  {
     osThreadFlagsSet(phost->urb_thread, 1);
  }
}

void StartUsb()
{
  USBH_HandleTypeDef *phost;

  phost =  &hUsbHostHS;
  memset(&hUsbHostHS, 0, sizeof(hUsbHostHS));

  /* Init host Library, add supported class and start the library. */
  if (USBH_Init(phost, HOST_HS) != USBH_OK)
  {
    Error_Handler();
  }

  if (USBH_RegisterClass(phost, USBH_AUDIO_CLASS) != USBH_OK)
  {
    Error_Handler();
  }
  if (USBH_RegisterClass(phost, USBH_HID_CLASS) != USBH_OK)
  {
    Error_Handler();
  }
  HAL_HCD_RegisterHC_NotifyURBChangeCallback((HCD_HandleTypeDef *)phost->pData, URBChangeCallback);
  if (USBH_Start(phost) != USBH_OK)
  {
    Error_Handler();
  }
}
