#include "main.h"
#include "cmsis_os.h"
#include "usb_host.h"
#include "usbh_core.h"
#include "usbh_hid.h"
#include "usbh_audio.h"
#include "usbh_bluetooth.h"
#include "stm32h7b3i_discovery_conf.h"

extern USBH_StatusTypeDef  USBH_Start(USBH_HandleTypeDef *phost);
extern USBH_StatusTypeDef USBH_RegisterClass(USBH_HandleTypeDef *phost, USBH_ClassTypeDef *pclass);

SECTION_AHBSRAM USBH_HandleTypeDef hUsbHostHS;

#ifdef SIGNAL_BOOST
/* ULPI PHY */
#define USBULPI_PHYCR     ((uint32_t)(USB1_OTG_HS_PERIPH_BASE + 0x034))
#define USBULPI_D07       ((uint32_t)0x000000FF)
#define USBULPI_New       ((uint32_t)0x02000000)
#define USBULPI_RW        ((uint32_t)0x00400000)
#define USBULPI_S_BUSY    ((uint32_t)0x04000000)
#define USBULPI_S_DONE    ((uint32_t)0x08000000)

#define Pattern_55        ((uint32_t)0x00000055)
#define Pattern_AA        ((uint32_t)0x000000AA)

#define PHY_PWR_DOWN       (1<<11)
#define PHY_ADDRESS        0x00 /* default ADDR for PHY: LAN8742 */

#define USB_OTG_READ_REG32(reg)  (*(__IO uint32_t *)(reg))
#define USB_OTG_WRITE_REG32(reg,value) (*(__IO uint32_t *)(reg) = (value))

/**
  * @brief  Read CR value
  * @param  Addr the Address of the ULPI Register
  * @retval Returns value of PHY CR register
  */
static uint32_t USB_ULPI_Read(uint32_t Addr)
{
   __IO uint32_t val = 0;
   __IO uint32_t timeout = 100; /* Can be tuned based on the Clock or etc... */

   USB_OTG_WRITE_REG32(USBULPI_PHYCR, USBULPI_New | (Addr << 16));
   val = USB_OTG_READ_REG32(USBULPI_PHYCR);
   while (((val & USBULPI_S_DONE) == 0) && (timeout--))
   {
       val = USB_OTG_READ_REG32(USBULPI_PHYCR);
   }
   val = USB_OTG_READ_REG32(USBULPI_PHYCR);
   return  (val & 0x000000ff);
}

/**
  * @brief  Write CR value
  * @param  Addr the Address of the ULPI Register
  * @param  Data Data to write
  * @retval Returns value of PHY CR register
  */
static uint32_t USB_ULPI_Write(uint32_t Addr, uint32_t Data)   /* Parameter is the Address of the ULPI Register & Date to write */
{
  __IO uint32_t val;
  __IO uint32_t timeout = 10;   /* Can be tuned based on the Clock or etc... */

  USB_OTG_WRITE_REG32(USBULPI_PHYCR, USBULPI_New | USBULPI_RW | (Addr << 16) | (Data & 0x000000ff));
  val = USB_OTG_READ_REG32(USBULPI_PHYCR);
  while (((val & USBULPI_S_DONE) == 0) && (timeout--))
  {
           val = USB_OTG_READ_REG32(USBULPI_PHYCR);
  }

  val = USB_OTG_READ_REG32(USBULPI_PHYCR);
  return 0;
}
#endif


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
#if 1
    if (state == URB_NOTREADY)
    {
      //if (channel == 3) debug_printf("BK not ready\n");
      if (hhcd->hc[channel].state != HC_HALTED)
        return;
    }
#endif
    if (osMessageQueuePut(evqueue, &evdata, 0, 0) != osOK)
debug_printf("pipe_ev overflow. %d, %x\n", channel, state);
    return;
  }

  if (phost->urb_thread)
  {
     osThreadFlagsSet(phost->urb_thread, 1);
  }
}

#define	USB3320_ID	0x00070424	/* Vendor & Product ID */

void StartUsb()
{
  USBH_HandleTypeDef *phost;
#ifdef SIGNAL_BOOST
  uint32_t regval;
  uint32_t addr;
  uint32_t idval;
  uint8_t *bp;
#endif

  phost =  &hUsbHostHS;
  memset(&hUsbHostHS, 0, sizeof(hUsbHostHS));

  /* Init host Library, add supported class and start the library. */
  if (USBH_Init(phost, HOST_HS) != USBH_OK)
  {
    Error_Handler();
  }

#ifdef SIGNAL_BOOST
  bp = (uint8_t *)&idval;
  for (addr = 0; addr < 4; addr++)
  {
    *bp++ = USB_ULPI_Read(addr) & 0xff;
  }
  if (idval == USB3320_ID)
  {
    debug_printf("USB3320 detected.\n");
    regval = USB_ULPI_Write(0x31, 0x01);
    regval = USB_ULPI_Read(0x31);
    debug_printf("Boost = %x\n", regval);
  }
#endif

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
