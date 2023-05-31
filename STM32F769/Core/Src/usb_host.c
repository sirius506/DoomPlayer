#include "main.h"
#include "cmsis_os.h"
#include "usb_host.h"
#include "usbh_core.h"
#include "usbh_hid.h"
#include "usbh_audio.h"
#include "usbh_bluetooth.h"
#include "stm32f769i_discovery_conf.h"

extern USBH_StatusTypeDef  USBH_Start(USBH_HandleTypeDef *phost);
extern USBH_StatusTypeDef USBH_RegisterClass(USBH_HandleTypeDef *phost, USBH_ClassTypeDef *pclass);

SECTION_AHBSRAM USBH_HandleTypeDef hUsbHostHS;

void URBChangeCallback(HCD_HandleTypeDef *hhcd, uint8_t channel, USB_OTG_URBStateTypeDef state)
{
  USBH_HandleTypeDef *phost;
  osMessageQueueId_t evqueue;

  phost = (USBH_HandleTypeDef *)hhcd->pData;
  evqueue = phost->PipeEvq[channel & 0x0F];
  if (evqueue)
  {  
    PIPE_EVENT evdata;

    evdata.channel = channel;
    evdata.state = state;

    if (state == URB_NOTREADY) 
    {  
      //if (channel == 3) debug_printf("BK not ready\n");
      if (hhcd->hc[channel].state != HC_HALTED)
        return;
    }  

    if (osMessageQueuePut(evqueue, &evdata, 0, 0) != osOK)
debug_printf("pipe_ev overflow. %d, %x\n", channel, state);
    return;
  }
  if (phost->urb_thread)
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

  if (USBH_RegisterClass(phost, USBH_BLUETOOTH_CLASS) != USBH_OK)
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
