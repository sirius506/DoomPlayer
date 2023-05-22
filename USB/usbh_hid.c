#include "DoomPlayer.h"
#include "usbh_hid.h"
#include "usbh_def.h"
#include "usbh_hid_parser.h"
#include "usbh_ctlreq.h"
#include "usbh_ioreq.h"
#include "usbh_pipes.h"
#include "usbh_hid_keybd.h"
#include "app_task.h"
#include "gamepad.h"

//#define	FLAG_TIMEOUT	5
#define	FLAG_TIMEOUT	osWaitForever

#define	KBDQ_SIZE	20

static KBDEVENT kbdqBuffer[KBDQ_SIZE];
MESSAGEQ_DEF(kbdq, kbdqBuffer, sizeof(kbdqBuffer))
osMessageQueueId_t kbdqId;

#define EVMSGQ_DEPTH     8
static uint8_t hidevmsgqBuffer[EVMSGQ_DEPTH * sizeof(PIPE_EVENT)];

MESSAGEQ_DEF(hidevmsgq, hidevmsgqBuffer, sizeof(hidevmsgqBuffer))

static USBH_StatusTypeDef USBH_HID_InterfaceInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_InterfaceDeInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_ClassRequest(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_Process(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_SOFProcess(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
static void  USBH_HID_ParseHIDDesc(HID_DescTypeDef *desc, uint8_t *buf);

USBH_StatusTypeDef USBH_HID_SetIdle(USBH_HandleTypeDef *phost, uint8_t duration, uint8_t reportId);

uint16_t USBH_HID_FifoWrite(FIFO_TypeDef *f, void *buf, uint16_t  nbytes);

SECTION_USBSRAM HID_HandleTypeDef HidHandleBuffer;

#define HID_STACK_SIZE  650

osThreadId_t hidtaskId;

TASK_DEF(hidtask, HID_STACK_SIZE, osPriorityHigh)

USBH_ClassTypeDef  HID_Class =
{
  "HID",
  USB_HID_CLASS,
  USBH_HID_InterfaceInit,
  USBH_HID_InterfaceDeInit,
  USBH_HID_ClassRequest,
  USBH_HID_Process,
  USBH_HID_SOFProcess,
  NULL,
};

static USBH_ClassTypeDef *pclass;

void set_hid_class(USBH_ClassTypeDef *class)
{
  pclass = class;
}

void StartHIDTask(void *arg)
{
  HID_HandleTypeDef *HID_Handle;
  USBH_StatusTypeDef st;
  uint8_t sofc;
  struct sGamePadDriver *padDriver;

  debug_printf("HID task started\n");
  pclass  = (USBH_ClassTypeDef *)arg;

  HID_Handle = (HID_HandleTypeDef *) pclass->pData;
  HID_Handle->state = HID_INIT;
  HID_Handle->report.hid_mode = HID_MODE_LVGL;
  USBH_HID_ParseHIDDesc(&HID_Handle->HID_Desc, pclass->phost->device.CfgDesc_Raw);

  HID_Handle->kbdqId = kbdqId = osMessageQueueNew(KBDQ_SIZE, sizeof(KBDEVENT), &attributes_kbdq);

  GrabUrb(pclass->phost);

//  pclass->phost->RequestState = CMD_SEND;

  st = USBH_HID_GetHIDReportDescriptor(pclass->phost, HID_Handle->HID_Desc.wItemLength);

  if (st == USBH_OK)
  {
     /* The descriptor is available in phost->device.Data */
     HID_Handle->ctl_state = HID_REQ_SET_IDLE;
     st = USBH_HID_SetIdle(pclass->phost, 0U, 0U);

     HID_Handle->Init(pclass, pclass->phost);

     HID_Handle->state = HID_IDLE;

#ifdef USE_GETREPORT
  st = USBH_HID_GetReport(pclass->phost, 0x01U, 0U, HID_Handle->pData, (uint8_t)HID_Handle->length);
#else
     st = USBH_OK;
#endif
     if (st == USBH_OK || st == USBH_NOT_SUPPORTED)
     {
       pclass->cState = 2;
debug_printf("HID poll = %d\n", HID_Handle->poll);
       HID_Handle->timer = HID_Handle->poll;
     }

     postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_SET | ICON_USB, NULL, NULL);
  }
  else if (st == USBH_NOT_SUPPORTED)
  {
    USBH_ErrLog("Control error: HID: Device Get Report Descriptor request failed");
    st = USBH_FAIL;
  }

debug_printf("HID EP = %d, %d, Pipe = %d, %d\n", HID_Handle->InEp, HID_Handle->OutEp, HID_Handle->InPipe, HID_Handle->OutPipe);
  pclass->classEventQueue = osMessageQueueNew(EVMSGQ_DEPTH, sizeof(PIPE_EVENT), &attributes_hidevmsgq);
//    pclass->phost->PipeEvq[0] = pclass->classEventQueue;
    pclass->phost->PipeEvq[HID_Handle->InPipe & 0x0F] = pclass->classEventQueue;
    pclass->phost->PipeEvq[HID_Handle->OutPipe & 0x0F] = pclass->classEventQueue;
osDelay(4);

  padDriver = (struct sGamePadDriver *)HID_Handle->gData;
  if (HID_Handle->devType == DEVTYPE_GAMEPAD)
  {
    (*padDriver->Setup)(pclass);
  }

  UngrabUrb(pclass->phost);


  /* Discard any pending events */
  osThreadFlagsWait (0xff, osFlagsWaitAny, 0);
  osThreadFlagsWait (0xff, osFlagsWaitAny, 0);

  sofc = 0;

  while (1)
  {
    PIPE_EVENT pev;
    uint32_t XferSize;
uint8_t *orp;
int orp_len;

    osMessageQueueGet(pclass->classEventQueue, &pev, NULL, osWaitForever);
    switch (pev.state)
    {
    case HID_SOF_FLAG:
      if (HID_Handle->devType == DEVTYPE_GAMEPAD)
      {
        (*padDriver->GetOutputReport)(&orp, &orp_len, HID_Handle->report.hid_mode, sofc++);
        USBH_InterruptSendData(pclass->phost, orp, orp_len, HID_Handle->OutPipe);
      }
      (void)USBH_InterruptReceiveData(pclass->phost, HID_Handle->pData,
                                      (uint8_t)HID_Handle->length,
                                      HID_Handle->InPipe);
      break;
    case USBH_URB_DONE:
      if (pev.channel == HID_Handle->InPipe)
      {
        XferSize = USBH_LL_GetLastXferSize(pclass->phost, HID_Handle->InPipe);
        if (XferSize > 0)
        {
          if (HID_Handle->devType == DEVTYPE_GAMEPAD)
          {
            HID_Handle->report.len = XferSize;
            (*padDriver->DecodeInputReport)(&HID_Handle->report);
          }
          else
            USBH_HID_KeybdDecode(HID_Handle);
        }
      }
      break;
    case HID_LVGL_BIT:
      HID_Handle->report.hid_mode = HID_MODE_LVGL;
      break;
    case HID_DOOM_BIT:
      HID_Handle->report.hid_mode = HID_MODE_DOOM;
      break;
    case HID_TEST_BIT:
      HID_Handle->report.hid_mode = HID_MODE_TEST;
      break;
    default:
      break;
    }
  }
}

/**
  * @brief  USBH_HID_InterfaceInit
  *         The function init the HID class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_InterfaceInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status;
  HID_HandleTypeDef *HID_Handle;
  uint8_t max_ep;
  uint8_t num = 0U;
  uint8_t interface;

  if (pclass->cState)
    return USBH_FAIL;

  num = USBH_FindInterface(phost, pclass->ClassCode, 0xFFU, 0xFFU);
  interface = USBH_FindInterfaceIndex(phost, num, 0);

  if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_IFDESC)) /* No Valid Interface */
  {
    USBH_DbgLog("Cannot Find the interface for %s class.", pclass->Name);
    return USBH_FAIL;
  }
  status = USBH_SelectInterface(phost, interface);

  if (status != USBH_OK)
  {
    return USBH_FAIL;
  }

  memset(&HidHandleBuffer, 0, sizeof(HidHandleBuffer));
  pclass->pData = &HidHandleBuffer;
  HID_Handle = (HID_HandleTypeDef *) pclass->pData;

  if (HID_Handle == NULL)
  {
    USBH_DbgLog("Cannot allocate memory for HID Handle");
    return USBH_FAIL;
  }

  /* Initialize hid handler */
  (void)USBH_memset(HID_Handle, 0, sizeof(HID_HandleTypeDef));

  HID_Handle->state = HID_ERROR;
  /*Decode Bootclass Protocol: Mouse or Keyboard*/
  if (phost->device.CfgDesc.Itf_Desc[interface].bInterfaceProtocol == HID_KEYBRD_BOOT_CODE)
  {
    USBH_UsrLog("KeyBoard device found!");
    HID_Handle->Init = USBH_HID_KeybdInit;
    HID_Handle->devType = DEVTYPE_KEYBOARD;
  }
#if 0
  else if (phost->device.CfgDesc.Itf_Desc[interface].bInterfaceProtocol  == HID_MOUSE_BOOT_CODE)
  {
    USBH_UsrLog("Mouse device found!");
    HID_Handle->Init = USBH_HID_MouseInit;
  }
#endif
  else
  {
    GAMEPAD_INFO *padInfo;
    const struct sGamePadDriver *pd;

    padInfo = IsSupportedGamePad(phost->device.DevDesc.idVendor, phost->device.DevDesc.idProduct);
    if (padInfo)
    {
      padInfo->pclass = pclass;
      pd = padInfo->padDriver;
      HID_Handle->gData = (void *)pd;
      HID_Handle->Init = pd->Init;
      HID_Handle->devType = DEVTYPE_GAMEPAD;
      postGuiEventMessage(GUIEV_GAMEPAD_READY, 0, padInfo, NULL);
    }
    else
    {
      HID_Handle->gData = NULL;
      USBH_UsrLog("Protocol not supported.");
      return USBH_FAIL;
    }
  }

  HID_Handle->state     = HID_INIT;
  HID_Handle->ctl_state = HID_REQ_INIT;
  HID_Handle->ep_addr   = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
  HID_Handle->length    = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
  HID_Handle->poll      = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bInterval;

  if (HID_Handle->poll  < HID_MIN_POLL)
  {
    HID_Handle->poll = HID_MIN_POLL;
  }

  /* Check of available number of endpoints */
  /* Find the number of EPs in the Interface Descriptor */
  /* Choose the lower number in order not to overrun the buffer allocated */
  max_ep = ((phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints <= USBH_MAX_NUM_ENDPOINTS) ?
            phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints : USBH_MAX_NUM_ENDPOINTS);


  /* Decode endpoint IN and OUT address from interface descriptor */
  for (num = 0U; num < max_ep; num++)
  {
    if ((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].bEndpointAddress & 0x80U) != 0U)
    {
      HID_Handle->InEp = (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].bEndpointAddress);
      HID_Handle->InPipe = USBH_AllocPipe(phost, HID_Handle->InEp);

      /* Open pipe for IN endpoint */
      (void)USBH_OpenPipe(phost, HID_Handle->InPipe, HID_Handle->InEp, phost->device.address,
                          phost->device.speed, USB_EP_TYPE_INTR, HID_Handle->length);

      (void)USBH_LL_SetToggle(phost, HID_Handle->InPipe, 0U);
    }
    else
    {
      HID_Handle->OutEp = (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].bEndpointAddress);
      HID_Handle->OutPipe  = USBH_AllocPipe(phost, HID_Handle->OutEp);

      /* Open pipe for OUT endpoint */
      (void)USBH_OpenPipe(phost, HID_Handle->OutPipe, HID_Handle->OutEp, phost->device.address,
                          phost->device.speed, USB_EP_TYPE_INTR, HID_Handle->length);

      (void)USBH_LL_SetToggle(phost, HID_Handle->OutPipe, 0U);
    }
  }
  pclass->cState = 1;
  pclass->phost = phost;
  hidtaskId =osThreadNew(StartHIDTask, pclass, &attributes_hidtask);

  return USBH_OK;
}

/**
  * @brief  USBH_ParseHIDDesc
  *         This function Parse the HID descriptor
  * @param  desc: HID Descriptor
  * @param  buf: Buffer where the source descriptor is available
  * @retval None
  */
static void  USBH_HID_ParseHIDDesc(HID_DescTypeDef *desc, uint8_t *buf)
{
  USBH_DescHeader_t *pdesc = (USBH_DescHeader_t *)buf;
  uint16_t CfgDescLen;
  uint16_t ptr;

  CfgDescLen = LE16(buf + 2U);

  if (CfgDescLen > USB_CONFIGURATION_DESC_SIZE)
  {
    ptr = USB_LEN_CFG_DESC;

    while (ptr < CfgDescLen)
    {
      pdesc = USBH_GetNextDesc((uint8_t *)pdesc, &ptr);

      if (pdesc->bDescriptorType == USB_DESC_TYPE_HID)
      {
        desc->bLength = *(uint8_t *)((uint8_t *)pdesc + 0U);
        desc->bDescriptorType = *(uint8_t *)((uint8_t *)pdesc + 1U);
        desc->bcdHID = LE16((uint8_t *)pdesc + 2U);
        desc->bCountryCode = *(uint8_t *)((uint8_t *)pdesc + 4U);
        desc->bNumDescriptors = *(uint8_t *)((uint8_t *)pdesc + 5U);
        desc->bReportDescriptorType = *(uint8_t *)((uint8_t *)pdesc + 6U);
        desc->wItemLength = LE16((uint8_t *)pdesc + 7U);
        break;
      }
    }
  }
}

static USBH_StatusTypeDef USBH_HID_InterfaceDeInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  return USBH_OK;
}

static USBH_StatusTypeDef USBH_HID_ClassRequest(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  return USBH_OK;
}
static USBH_StatusTypeDef USBH_HID_Process(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  return USBH_OK;
}
static USBH_StatusTypeDef USBH_HID_SOFProcess(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle;
  PIPE_EVENT pev;

  switch (pclass->cState)
  {
  case 2:
    pclass->cState++;
    break;
  case 3:
    HID_Handle = (HID_HandleTypeDef *) pclass->pData;
    if (HID_Handle->timer > 0)
    {
      HID_Handle->timer--;
    }
    else
    {
      HID_Handle->timer = HID_Handle->poll;
      pev.channel = 0;
      pev.state = HID_SOF_FLAG;
      osMessageQueuePut(pclass->classEventQueue, &pev, 0, 0);
    }
    break;
  default:
    break;
  }
  return USBH_OK;
}

/**
  * @brief  USBH_Get_HID_ReportDescriptor
  *         Issue report Descriptor command to the device. Once the response
  *         received, parse the report descriptor and update the status.
  * @param  phost: Host handle
  * @param  Length : HID Report Descriptor Length
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_GetHIDReportDescriptor(USBH_HandleTypeDef *phost,
                                                   uint16_t length)
{

  USBH_StatusTypeDef status;

  status = USBH_GetDescriptor(phost,
                              USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_STANDARD,
                              USB_DESC_HID_REPORT,
                              phost->device.Data,
                              length);

  /* HID report descriptor is available in phost->device.Data.
  In case of USB Boot Mode devices for In report handling ,
  HID report descriptor parsing is not required.
  In case, for supporting Non-Boot Protocol devices and output reports,
  user may parse the report descriptor*/

  do
  {
    osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
    status = USBH_CtlReq(phost, phost->device.Data, length);
  } while (status == USBH_BUSY);


  if ((status == USBH_OK) || (status == USBH_NOT_SUPPORTED))
  {
    /* Transaction completed, move control state to idle */
    phost->RequestState = CMD_SEND;
    phost->Control.state = CTRL_IDLE;
  }
  else if (status == USBH_FAIL)
  {
    /* Failure Mode */
    phost->RequestState = CMD_SEND;
  }

  return status;
}

/**
  * @brief  USBH_Set_Idle
  *         Set Idle State.
  * @param  phost: Host handle
  * @param  duration: Duration for HID Idle request
  * @param  reportId : Targeted report ID for Set Idle request
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_SetIdle(USBH_HandleTypeDef *phost,
                                    uint8_t duration,
                                    uint8_t reportId)
{
  USBH_StatusTypeDef status;

  phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | \
                                         USB_REQ_TYPE_CLASS;


  phost->Control.setup.b.bRequest = USB_HID_SET_IDLE;
  phost->Control.setup.b.wValue.w = (uint16_t)(((uint32_t)duration << 8U) | (uint32_t)reportId);

  phost->Control.setup.b.wIndex.w = 0U;
  phost->Control.setup.b.wLength.w = 0U;

  status = USBH_CtlReq(phost, NULL, 0);

  do
  {
    osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
    status = USBH_CtlReq(phost, NULL, 0);
  } while (status == USBH_BUSY);

  return status;
}

/**
  * @brief  USBH_HID_FifoInit
  *         Initialize FIFO.
  * @param  f: Fifo address
  * @param  buf: Fifo buffer
  * @param  size: Fifo Size
  * @retval none
  */
void USBH_HID_FifoInit(FIFO_TypeDef *f, uint8_t *buf, uint16_t size)
{
  f->head = 0U;
  f->tail = 0U;
  f->lock = 0U;
  f->size = size;
  f->buf = buf;
}

/**
  * @brief  USBH_HID_FifoRead
  *         Read from FIFO.
  * @param  f: Fifo address
  * @param  buf: read buffer
  * @param  nbytes: number of item to read
  * @retval number of read items
  */
uint16_t USBH_HID_FifoRead(FIFO_TypeDef *f, void *buf, uint16_t nbytes)
{
  uint16_t i;
  uint8_t *p;

  p = (uint8_t *) buf;

  if (f->lock == 0U)
  {
    f->lock = 1U;

    for (i = 0U; i < nbytes; i++)
    {
      if (f->tail != f->head)
      {
        *p++ = f->buf[f->tail];
        f->tail++;

        if (f->tail == f->size)
        {
          f->tail = 0U;
        }
      }
      else
      {
        f->lock = 0U;
        return i;
      }
    }
  }

  f->lock = 0U;

  return nbytes;
}

/**
  * @brief  USBH_HID_Set_Report
  *         Issues Set Report
  * @param  phost: Host handle
  * @param  reportType  : Report type to be sent
  * @param  reportId    : Targeted report ID for Set Report request
  * @param  reportBuff  : Report Buffer
  * @param  reportLen   : Length of data report to be send
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_SetReport(USBH_HandleTypeDef *phost,
                                      uint8_t reportType,
                                      uint8_t reportId,
                                      uint8_t *reportBuff,
                                      uint8_t reportLen)
{
  USBH_StatusTypeDef status;

  phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | \
                                         USB_REQ_TYPE_CLASS;


  phost->Control.setup.b.bRequest = USB_HID_SET_REPORT;
  phost->Control.setup.b.wValue.w = (uint16_t)(((uint32_t)reportType << 8U) | (uint32_t)reportId);

  phost->Control.setup.b.wIndex.w = 0U;
  phost->Control.setup.b.wLength.w = reportLen;

  status = USBH_CtlReq(phost, reportBuff, (uint16_t)reportLen);

  do
  {
    osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
    status = USBH_CtlReq(phost, reportBuff, (uint16_t)reportLen);
  } while (status == USBH_BUSY);

  return status;
}

/**
  * @brief  USBH_HID_GetReport
  *         retrieve Set Report
  * @param  phost: Host handle
  * @param  reportType  : Report type to be sent
  * @param  reportId    : Targeted report ID for Set Report request
  * @param  reportBuff  : Report Buffer
  * @param  reportLen   : Length of data report to be send
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_GetReport(USBH_HandleTypeDef *phost,
                                      uint8_t reportType,
                                      uint8_t reportId,
                                      uint8_t *reportBuff,
                                      uint8_t reportLen)
{
  USBH_StatusTypeDef status;

  phost->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_INTERFACE | \
                                         USB_REQ_TYPE_CLASS;


  phost->Control.setup.b.bRequest = USB_HID_GET_REPORT;
  phost->Control.setup.b.wValue.w = (uint16_t)(((uint32_t)reportType << 8U) | (uint32_t)reportId);

  phost->Control.setup.b.wIndex.w = 0U;
  phost->Control.setup.b.wLength.w = reportLen;

  status = USBH_CtlReq(phost, reportBuff, (uint16_t)reportLen);

  do
  {
    osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
    status = USBH_CtlReq(phost, reportBuff, (uint16_t)reportLen);
  } while (status == USBH_BUSY);

  return status;
}
