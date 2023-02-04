#include "DoomPlayer.h"
#include "usbh_core.h"
#include "usbh_ioreq.h"
#include "usbh_pipes.h"
#include "usbh_ctlreq.h"

#define USBH_ADDRESS_DEFAULT                     0x00U
#define USBH_ADDRESS_ASSIGNED                    0x01U
#define USBH_MPS_DEFAULT                         0x40U

static USBH_StatusTypeDef DeInitStateMachine(USBH_HandleTypeDef *phost);
static void  USBH_HandleSof(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HandleEnum(USBH_HandleTypeDef *phost);

TASK_DEF(hostTask, 600, osPriorityHigh)

void StartHostTask(void *arg)
{
  USBH_HandleTypeDef *phost = (USBH_HandleTypeDef *)arg;
  osStatus_t qstatus;
  __IO USBH_StatusTypeDef status = USBH_FAIL;
  uint8_t idx = 0U;
  USBH_ClassTypeDef *pcif;

  osDelay(800);

  phost->gState = HOST_IDLE;

  for (;;)
  {
    qstatus = osMessageQueueGet(phost->os_event,
                               &phost->os_msg, NULL, osWaitForever);
    if (qstatus == osOK)
    {
      /* check for Host pending port disconnect event */
      if (phost->device.is_disconnected == 1U)
      {
        phost->gState = HOST_DEV_DISCONNECTED;
      }

      switch (phost->gState)
      {
        case HOST_IDLE :
          if ((phost->device.is_connected) != 0U)
          {
            USBH_UsrLog("USB Device Connected");

            /* Wait for 200 ms after connection */
            phost->gState = HOST_DEV_WAIT_FOR_ATTACHMENT;
            osDelay(200U);
            (void)USBH_LL_ResetPort(phost);

            /* Make sure to start with Default address */
            phost->device.address = USBH_ADDRESS_DEFAULT;
            phost->Timeout = 0U;

            phost->os_msg = (uint32_t)USBH_PORT_EVENT;
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
          }
          break;
        case HOST_DEV_WAIT_FOR_ATTACHMENT: /* Wait for Port Enabled */

          if (phost->device.PortEnabled == 1U)
          {
            USBH_UsrLog("USB Device Reset Completed");
            phost->device.RstCnt = 0U;
            phost->gState = HOST_DEV_ATTACHED;
          }
          else
          {
            if (phost->Timeout > USBH_DEV_RESET_TIMEOUT)
            {
              phost->device.RstCnt++;
              if (phost->device.RstCnt > 3U)
              {
                /* Buggy Device can't complete reset */
                USBH_UsrLog("USB Reset Failed, Please unplug the Device.");
                phost->gState = HOST_ABORT_STATE;
              }
              else
              {
                phost->gState = HOST_IDLE;
              }
            }
            else
            {
              phost->Timeout += 10U;
              osDelay(10U);
            }
          }
          phost->os_msg = (uint32_t)USBH_PORT_EVENT;
          (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
          break;

        case HOST_DEV_ATTACHED :
#if 0
          if (phost->pUser != NULL)
          {
            phost->pUser(phost, HOST_USER_CONNECTION);
          }
#endif
          /* Wait for 100 ms after Reset */
          osDelay(100U);

          phost->device.speed = (uint8_t)USBH_LL_GetSpeed(phost);

          phost->gState = HOST_ENUMERATION;

          phost->Control.pipe_out = USBH_AllocPipe(phost, 0x00U);
          phost->Control.pipe_in  = USBH_AllocPipe(phost, 0x80U);

          /* Open Control pipes */
          (void)USBH_OpenPipe(phost, phost->Control.pipe_in, 0x80U,
                          phost->device.address, phost->device.speed,
                          USBH_EP_CONTROL, (uint16_t)phost->Control.pipe_size);

          /* Open Control pipes */
          (void)USBH_OpenPipe(phost, phost->Control.pipe_out, 0x00U,
                          phost->device.address, phost->device.speed,
                          USBH_EP_CONTROL, (uint16_t)phost->Control.pipe_size);
          phost->os_msg = (uint32_t)USBH_PORT_EVENT;
          (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
          break;

        case HOST_ENUMERATION:
          /* Check for enumeration status */
          status = USBH_HandleEnum(phost);
          if (status == USBH_OK)
          {
            /* The function shall return USBH_OK when full enumeration is complete */
            USBH_UsrLog("Enumeration done.");

            phost->device.current_interface = 0U;

            if (phost->device.DevDesc.bNumConfigurations == 1U)
            {
              USBH_UsrLog("This device has only 1 configuration.");
              phost->gState = HOST_SET_CONFIGURATION;
            }
            else
            {
              phost->gState = HOST_INPUT;
            }
            phost->os_msg = (uint32_t)USBH_STATE_CHANGED_EVENT;
            (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
          }
          break;

        case HOST_SET_CONFIGURATION:
          /* set configuration */
          if (USBH_SetCfg(phost, (uint16_t)phost->device.CfgDesc.bConfigurationValue) == USBH_OK)
          {
            phost->gState = HOST_SET_WAKEUP_FEATURE;
            USBH_UsrLog("Default configuration set.");
          }

          phost->os_msg = (uint32_t)USBH_PORT_EVENT;
          (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
          break;

        case  HOST_SET_WAKEUP_FEATURE:

          if (((phost->device.CfgDesc.bmAttributes) & (1U << 5)) != 0U)
          {
            status = USBH_SetFeature(phost, FEATURE_SELECTOR_REMOTEWAKEUP);

            if (status == USBH_OK)
            {
              USBH_UsrLog("Device remote wakeup enabled");
              phost->gState = HOST_CHECK_CLASS;
            }
            else if (status == USBH_NOT_SUPPORTED)
            {
              USBH_UsrLog("Remote wakeup not supported by the device");
              phost->gState = HOST_CHECK_CLASS;
            }
            else
            {
              /* .. */
            }
          }
          else
          {
            phost->gState = HOST_CHECK_CLASS;
          }

          phost->os_msg = (uint32_t)USBH_PORT_EVENT;
          (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
          break;

        case HOST_CHECK_CLASS:
          if (phost->ClassNumber == 0U)
          {
            USBH_UsrLog("No Class has been registered.");
            phost->gState = HOST_ABORT_STATE;
          }
          else
          {
            int ifidx;

            phost->pActiveClassList = NULL;
            for (idx = 0U; idx < USBH_MAX_NUM_SUPPORTED_CLASS; idx++)
            {
              pcif = NULL;
              for (ifidx = 0; (pcif == NULL) && (ifidx < USBH_MAX_NUM_IFDESC); ifidx++)
              {
                if (phost->pClass[idx]->ClassCode == phost->device.CfgDesc.Itf_Desc[ifidx].bInterfaceClass)
                {
                  //debug_printf("code match: %d\n", phost->pClass[idx]->ClassCode);
                  osThreadYield();
                  pcif = phost->pClass[idx];
                  break;
                }
              }
              if (pcif != NULL)
              {
                  pcif->pNext = phost->pActiveClassList;
                  phost->pActiveClassList = pcif;
              }
            }
            pcif = phost->pActiveClassList;

            if (pcif == NULL)
            {
              phost->gState = HOST_ABORT_STATE;
              USBH_UsrLog("No registered class for this device.");
            }
            else
            {
              do
              {
                if (pcif->Init(pcif, phost) == USBH_OK)
                {
                  //phost->gState = HOST_CLASS_REQUEST;
                  phost->gState = HOST_CLASS;
                  USBH_UsrLog("%s class started.", pcif->Name);

#if 0
                  /* Inform user that a class has been activated */
                  phost->pUser(phost, HOST_USER_CLASS_SELECTED);
#endif
                }
                else
                {
                  phost->gState = HOST_ABORT_STATE;
                  USBH_UsrLog("Device not supporting %s class.", pcif->Name);
                }
                pcif = pcif->pNext;
              }
              while (pcif != NULL);
            }
          }

          //phost->gState = HOST_ABORT_STATE;
          phost->os_msg = (uint32_t)USBH_STATE_CHANGED_EVENT;
          (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
          break;

        case HOST_CLASS:
          if (phost->urb_thread)
          {
//debug_printf("MSG = %x\n", phost->os_msg);
            osThreadFlagsSet(phost->urb_thread, 1);
          }
#if 0
          else
          {
debug_printf("no grab.\n");
          }
#endif
          break;
        case HOST_ABORT_STATE:
        default:  
          debug_printf("Abort STATE %d\n", phost->gState);
          break;
      }
    }
  }
}

/**
  * @brief  USBH_HandleEnum
  *         This function includes the complete enumeration process
  * @param  phost: Host Handle
  * @retval USBH_Status
  */
static USBH_StatusTypeDef USBH_HandleEnum(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef Status = USBH_BUSY;
  USBH_StatusTypeDef ReqStatus = USBH_BUSY;

  switch (phost->EnumState)
  {
    case ENUM_IDLE:
      /* Get Device Desc for only 1st 8 bytes : To get EP0 MaxPacketSize */
      ReqStatus = USBH_Get_DevDesc(phost, 8U);
      if (ReqStatus == USBH_OK)
      {
        phost->Control.pipe_size = phost->device.DevDesc.bMaxPacketSize;

        phost->EnumState = ENUM_GET_FULL_DEV_DESC;

        /* modify control channels configuration for MaxPacket size */
        (void)USBH_OpenPipe(phost, phost->Control.pipe_in, 0x80U, phost->device.address,
                            phost->device.speed, USBH_EP_CONTROL,
                            (uint16_t)phost->Control.pipe_size);

        /* Open Control pipes */
        (void)USBH_OpenPipe(phost, phost->Control.pipe_out, 0x00U, phost->device.address,
                            phost->device.speed, USBH_EP_CONTROL,
                            (uint16_t)phost->Control.pipe_size);
      }
      else if (ReqStatus == USBH_NOT_SUPPORTED)
      {
        USBH_ErrLog("Control error: Get Device Descriptor request failed");
        phost->device.EnumCnt++;
        if (phost->device.EnumCnt > 3U)
        {
          /* Buggy Device can't complete get device desc request */
          USBH_UsrLog("Control error, Device not Responding Please unplug the Device.");
          phost->gState = HOST_ABORT_STATE;
        }
        else
        {
          /* free control pipes */
          (void)USBH_FreePipe(phost, phost->Control.pipe_out);
          (void)USBH_FreePipe(phost, phost->Control.pipe_in);

          /* Reset the USB Device */
          phost->gState = HOST_IDLE;
        }
      }
      else
      {
        /* .. */
      }
      break;

    case ENUM_GET_FULL_DEV_DESC:
      /* Get FULL Device Desc  */
      ReqStatus = USBH_Get_DevDesc(phost, USB_DEVICE_DESC_SIZE);
      if (ReqStatus == USBH_OK)
      {
        USBH_UsrLog("PID: %xh", phost->device.DevDesc.idProduct);
        USBH_UsrLog("VID: %xh", phost->device.DevDesc.idVendor);

        phost->EnumState = ENUM_SET_ADDR;
      }
      else if (ReqStatus == USBH_NOT_SUPPORTED)
      {
        USBH_ErrLog("Control error: Get Full Device Descriptor request failed");
        phost->device.EnumCnt++;
        if (phost->device.EnumCnt > 3U)
        {
          /* Buggy Device can't complete get device desc request */
          USBH_UsrLog("Control error, Device not Responding Please unplug the Device.");
          phost->gState = HOST_ABORT_STATE;
        }
        else
        {
          /* Free control pipes */
          (void)USBH_FreePipe(phost, phost->Control.pipe_out);
          (void)USBH_FreePipe(phost, phost->Control.pipe_in);

          /* Reset the USB Device */
          phost->EnumState = ENUM_IDLE;
          phost->gState = HOST_IDLE;
        }
      }
      else
      {
        /* .. */
      }
      break;
    case ENUM_SET_ADDR:
      /* set address */
      ReqStatus = USBH_SetAddress(phost, USBH_DEVICE_ADDRESS);
      if (ReqStatus == USBH_OK)
      {
        osDelay(2U);
        phost->device.address = USBH_DEVICE_ADDRESS;

        /* user callback for device address assigned */
        USBH_UsrLog("Address (#%d) assigned.", phost->device.address);
        phost->EnumState = ENUM_GET_CFG_DESC;

        /* modify control channels to update device address */
        (void)USBH_OpenPipe(phost, phost->Control.pipe_in, 0x80U,  phost->device.address,
                            phost->device.speed, USBH_EP_CONTROL,
                            (uint16_t)phost->Control.pipe_size);

        /* Open Control pipes */
        (void)USBH_OpenPipe(phost, phost->Control.pipe_out, 0x00U, phost->device.address,
                            phost->device.speed, USBH_EP_CONTROL,
                            (uint16_t)phost->Control.pipe_size);
      }
      else if (ReqStatus == USBH_NOT_SUPPORTED)
      {
        USBH_ErrLog("Control error: Device Set Address request failed");

        /* Buggy Device can't complete get device desc request */
        USBH_UsrLog("Control error, Device not Responding Please unplug the Device.");
        phost->gState = HOST_ABORT_STATE;
        phost->EnumState = ENUM_IDLE;
      }
      else
      {
        /* .. */
      }
      break;

    case ENUM_GET_CFG_DESC:
      /* get standard configuration descriptor */
      ReqStatus = USBH_Get_CfgDesc(phost, USB_CONFIGURATION_DESC_SIZE);
      if (ReqStatus == USBH_OK)
      {
        phost->EnumState = ENUM_GET_FULL_CFG_DESC;
      }
      else if (ReqStatus == USBH_NOT_SUPPORTED)
      {
        USBH_ErrLog("Control error: Get Device configuration descriptor request failed");
        phost->device.EnumCnt++;
        if (phost->device.EnumCnt > 3U)
        {
          /* Buggy Device can't complete get device desc request */
          USBH_UsrLog("Control error, Device not Responding Please unplug the Device.");
          phost->gState = HOST_ABORT_STATE;
        }
        else
        {
          /* Free control pipes */
          (void)USBH_FreePipe(phost, phost->Control.pipe_out);
          (void)USBH_FreePipe(phost, phost->Control.pipe_in);

          /* Reset the USB Device */
          phost->EnumState = ENUM_IDLE;
          phost->gState = HOST_IDLE;
        }
      }
      else
      {
        /* .. */
      }
      break;
    case ENUM_GET_FULL_CFG_DESC:
      /* get FULL config descriptor (config, interface, endpoints) */
      ReqStatus = USBH_Get_CfgDesc(phost, phost->device.CfgDesc.wTotalLength);
      if (ReqStatus == USBH_OK)
      {
        phost->EnumState = ENUM_GET_MFC_STRING_DESC;
      }
      else if (ReqStatus == USBH_NOT_SUPPORTED)
      {
        USBH_ErrLog("Control error: Get Device configuration descriptor request failed");
        phost->device.EnumCnt++;
        if (phost->device.EnumCnt > 3U)
        {
          /* Buggy Device can't complete get device desc request */
          USBH_UsrLog("Control error, Device not Responding Please unplug the Device.");
          phost->gState = HOST_ABORT_STATE;
        }
        else
        {
          /* Free control pipes */
          (void)USBH_FreePipe(phost, phost->Control.pipe_out);
          (void)USBH_FreePipe(phost, phost->Control.pipe_in);

          /* Reset the USB Device */
          phost->EnumState = ENUM_IDLE;
          phost->gState = HOST_IDLE;
        }
      }
      else
      {
        /* .. */
      }
      break;

    case ENUM_GET_MFC_STRING_DESC:
      if (phost->device.DevDesc.iManufacturer != 0U)
      {
        /* Check that Manufacturer String is available */
        ReqStatus = USBH_Get_StringDesc(phost, phost->device.DevDesc.iManufacturer,
                                        phost->device.Data, 0xFFU);
        if (ReqStatus == USBH_OK)
        {
          /* User callback for Manufacturing string */
          USBH_UsrLog("Manufacturer : %s", (char *)(void *)phost->device.Data);
          phost->EnumState = ENUM_GET_PRODUCT_STRING_DESC;

          phost->os_msg = (uint32_t)USBH_STATE_CHANGED_EVENT;
          (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
        }
        else if (ReqStatus == USBH_NOT_SUPPORTED)
        {
          USBH_UsrLog("Manufacturer : N/A");
          phost->EnumState = ENUM_GET_PRODUCT_STRING_DESC;

          phost->os_msg = (uint32_t)USBH_STATE_CHANGED_EVENT;
          (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
        }
        else
        {
          /* .. */
        }
      }
      else
      {
        USBH_UsrLog("Manufacturer : N/A");
        phost->EnumState = ENUM_GET_PRODUCT_STRING_DESC;

        phost->os_msg = (uint32_t)USBH_STATE_CHANGED_EVENT;
        (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
      }
      break;
    case ENUM_GET_PRODUCT_STRING_DESC:
      if (phost->device.DevDesc.iProduct != 0U)
      {
        /* Check that Product string is available */
        ReqStatus = USBH_Get_StringDesc(phost, phost->device.DevDesc.iProduct,
                                        phost->device.Data, 0xFFU);
        if (ReqStatus == USBH_OK)
        {
          /* User callback for Product string */
          USBH_UsrLog("Product : %s", (char *)(void *)phost->device.Data);
          phost->EnumState = ENUM_GET_SERIALNUM_STRING_DESC;
        }
        else if (ReqStatus == USBH_NOT_SUPPORTED)
        {
          USBH_UsrLog("Product : N/A");
          phost->EnumState = ENUM_GET_SERIALNUM_STRING_DESC;

          phost->os_msg = (uint32_t)USBH_STATE_CHANGED_EVENT;
          (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
        }
        else
        {
          /* .. */
        }
      }
      else
      {
        USBH_UsrLog("Product : N/A");
        phost->EnumState = ENUM_GET_SERIALNUM_STRING_DESC;

        phost->os_msg = (uint32_t)USBH_STATE_CHANGED_EVENT;
        (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
      }
      break;

    case ENUM_GET_SERIALNUM_STRING_DESC:
      if (phost->device.DevDesc.iSerialNumber != 0U)
      {
        /* Check that Serial number string is available */
        ReqStatus = USBH_Get_StringDesc(phost, phost->device.DevDesc.iSerialNumber,
                                        phost->device.Data, 0xFFU);
        if (ReqStatus == USBH_OK)
        {
          /* User callback for Serial number string */
          USBH_UsrLog("Serial Number : %s", (char *)(void *)phost->device.Data);
          Status = USBH_OK;
        }
        else if (ReqStatus == USBH_NOT_SUPPORTED)
        {
          USBH_UsrLog("Serial Number : N/A");
          Status = USBH_OK;
        }
        else
        {
          /* .. */
        }
      }
      else
      {
        USBH_UsrLog("Serial Number : N/A");
        Status = USBH_OK;
      }
      break;

    default:
      break;
  }
  return Status;
}

USBH_StatusTypeDef  USBH_Init(USBH_HandleTypeDef *phost, uint8_t id)
{
  /* Check whether the USB Host handle is valid */
  if (phost == NULL)
  {
    USBH_ErrLog("Invalid Host handle");
    return USBH_FAIL;
  }

  /* Set DRiver ID */
  phost->id = id;

  /* Unlink class*/
  phost->pActiveClassList = NULL;
  phost->ClassNumber = 0U;
  phost->gState = 0;

  /* Restore default states and prepare EP0 */
  (void)DeInitStateMachine(phost);

  /* Restore default Device connection states */
  phost->device.PortEnabled = 0U;
  phost->device.is_connected = 0U;
  phost->device.is_disconnected = 0U;
  phost->device.is_ReEnumerated = 0U;

  /* Create USB Host Queue */
  phost->os_event = osMessageQueueNew(MSGQUEUE_OBJECTS, sizeof(uint32_t), NULL);
  phost->host_lock = osMutexNew(NULL);
#ifdef USE_URB_SEM
  phost->urb_sem = osSemaphoreNew(1, 1, NULL);
#endif
  phost->control_sem = osSemaphoreNew(1, 1, NULL);
  phost->thread = osThreadNew(StartHostTask, phost, &attributes_hostTask);

  /* Initialize low level driver */
  (void)USBH_LL_Init(phost);

  return USBH_OK;
}

/**
  * @brief  DeInitStateMachine
  *         De-Initialize the Host state machine.
  * @param  phost: Host Handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef DeInitStateMachine(USBH_HandleTypeDef *phost)
{
  uint32_t i = 0U;

  /* Clear Pipes flags*/
  for (i = 0U; i < USBH_MAX_PIPES_NBR; i++)
  {
    phost->Pipes[i] = 0U;
  }

  for (i = 0U; i < USBH_MAX_DATA_BUFFER; i++)
  {
    phost->device.Data[i] = 0U;
  }

  phost->gState = HOST_IDLE;
  phost->EnumState = ENUM_IDLE;
  phost->RequestState = CMD_SEND;
  phost->Timer = 0U;

  phost->Control.state = CTRL_SETUP;
  phost->Control.pipe_size = USBH_MPS_DEFAULT;
  phost->Control.errorcount = 0U;

  phost->device.address = USBH_ADDRESS_DEFAULT;
  phost->device.speed = (uint8_t)USBH_SPEED_FULL;
  phost->device.RstCnt = 0U;
  phost->device.EnumCnt = 0U;

  return USBH_OK;
}


/**
  * @brief  USBH_RegisterClass
  *         Link class driver to Host Core.
  * @param  phost : Host Handle
  * @param  pclass: Class handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_RegisterClass(USBH_HandleTypeDef *phost, USBH_ClassTypeDef *pclass)
{
  USBH_StatusTypeDef status = USBH_OK;

  if (pclass != NULL)
  {
    if (phost->ClassNumber < USBH_MAX_NUM_SUPPORTED_CLASS)
    {
      /* link the class to the USB Host handle */
      phost->pClass[phost->ClassNumber++] = pclass;
      status = USBH_OK;
    }
    else
    {
      USBH_ErrLog("Max Class Number reached");
      status = USBH_FAIL;
    }
  }
  else
  {
    USBH_ErrLog("Invalid Class handle");
    status = USBH_FAIL;
  }

  return status;
}

/**
  * @brief  USBH_SelectInterface
  *         Select current interface.
  * @param  phost: Host Handle
  * @param  interface: Interface number
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_SelectInterface(USBH_HandleTypeDef *phost, uint8_t interface)
{
  USBH_StatusTypeDef status = USBH_OK;

#if 0
  if (interface < phost->device.CfgDesc.bNumInterfaces)
#else
  if (interface < USBH_MAX_NUM_IFDESC)
#endif
  {
    phost->device.current_interface = interface;
    USBH_UsrLog("Switching to Interface (#%d)", interface);
    USBH_UsrLog("Class    : %xh", phost->device.CfgDesc.Itf_Desc[interface].bInterfaceClass);
    USBH_UsrLog("SubClass : %xh", phost->device.CfgDesc.Itf_Desc[interface].bInterfaceSubClass);
    USBH_UsrLog("Protocol : %xh", phost->device.CfgDesc.Itf_Desc[interface].bInterfaceProtocol);
  }
  else
  {
    USBH_ErrLog("Cannot Select This Interface.");
    status = USBH_FAIL;
  }

  return status;
}

/**
  * @brief  USBH_FindInterface
  *         Find the interface index for a specific class.
  * @param  phost: Host Handle
  * @param  Class: Class code
  * @param  SubClass: SubClass code
  * @param  Protocol: Protocol code
  * @retval interface index in the configuration structure
  * @note : (1)interface index 0xFF means interface index not found
  */
uint8_t  USBH_FindInterface(USBH_HandleTypeDef *phost, uint8_t Class, uint8_t SubClass, uint8_t Protocol)
{
  USBH_InterfaceDescTypeDef *pif;
  uint8_t if_ix = 0U;
    
  pif = &phost->device.CfgDesc.Itf_Desc[0];
  
  while (if_ix < USBH_MAX_NUM_IFDESC)
  {
    if (((pif->bInterfaceClass == Class) || (Class == 0xFFU)) &&
        ((pif->bInterfaceSubClass == SubClass) || (SubClass == 0xFFU)) &&
        ((pif->bInterfaceProtocol == Protocol) || (Protocol == 0xFFU)))
    {
#if 0
      return  if_ix;
#else
      return pif->bInterfaceNumber;
#endif
    }
    if_ix++;
    pif++;
  }
  return 0xFFU;
}

/**
  * @brief  USBH_FindInterfaceIndex
  *         Find the interface index for a specific class interface and alternate setting number.
  * @param  phost: Host Handle
  * @param  interface_number: interface number
  * @param  alt_settings    : alternate setting number
  * @retval interface index in the configuration structure
  * @note : (1)interface index 0xFF means interface index not found
  */
uint8_t  USBH_FindInterfaceIndex(USBH_HandleTypeDef *phost, uint8_t interface_number, uint8_t alt_settings)
{
  USBH_InterfaceDescTypeDef *pif;
  USBH_CfgDescTypeDef *pcfg;
  uint8_t if_ix = 0U;

  pif = (USBH_InterfaceDescTypeDef *)NULL;
  pcfg = &phost->device.CfgDesc;

  while (if_ix < USBH_MAX_NUM_IFDESC)
  {
    pif = &pcfg->Itf_Desc[if_ix];
    if ((pif->bInterfaceNumber == interface_number) && (pif->bAlternateSetting == alt_settings))
    {
      return  if_ix;
    }
    if_ix++;
  }
  return 0xFFU;
}


/**
  * @brief  USBH_Start
  *         Start the USB Host Core.
  * @param  phost: Host Handle
  * @retval USBH Status
  */
USBH_StatusTypeDef  USBH_Start(USBH_HandleTypeDef *phost)
{ 
  /* Start the low level driver  */
  (void)USBH_LL_Start(phost);
  
  /* Activate VBUS on the port */
  (void)USBH_LL_DriverVBUS(phost, TRUE);

  return USBH_OK;
} 

/**
  * @brief  USBH_LL_SetTimer
  *         Set the initial Host Timer tick
  * @param  phost: Host Handle
  * @retval None
  */
void  USBH_LL_SetTimer(USBH_HandleTypeDef *phost, uint32_t time)
{
  phost->Timer = time;
}

/**
  * @brief  USBH_LL_IncTimer
  *         Increment Host Timer tick
  * @param  phost: Host Handle
  * @retval None
  */
void  USBH_LL_IncTimer(USBH_HandleTypeDef *phost)
{
  USBH_ClassTypeDef *pclass;

  phost->Timer++;

  for (pclass = phost->pActiveClassList; pclass; pclass = pclass->pNext)
  {
    pclass->SOFProcess(pclass, phost);
  }
}

/**
  * @brief  USBH_HandleSof
  *         Call SOF process
  * @param  phost: Host Handle
  * @retval None
  */
static void  USBH_HandleSof(USBH_HandleTypeDef *phost)
{
  USBH_ClassTypeDef *pclass;

  for (pclass = phost->pActiveClassList; pclass; pclass = pclass->pNext)
  {
    pclass->SOFProcess(pclass, phost);
  }
}

/**
  * @brief  USBH_PortEnabled
  *         Port Enabled
  * @param  phost: Host Handle
  * @retval None
  */
void USBH_LL_PortEnabled(USBH_HandleTypeDef *phost)
{
  phost->device.PortEnabled = 1U;

  phost->os_msg = (uint32_t)USBH_PORT_EVENT;
  (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);

  return;
}

/**
  * @brief  USBH_LL_PortDisabled
  *         Port Disabled
  * @param  phost: Host Handle
  * @retval None
  */
void USBH_LL_PortDisabled(USBH_HandleTypeDef *phost)
{
  phost->device.PortEnabled = 0U;

  return;
}

/**
  * @brief  USBH_LL_Connect
  *         Handle USB Host connexion event
  * @param  phost: Host Handle
  * @retval USBH_Status
  */
USBH_StatusTypeDef  USBH_LL_Connect(USBH_HandleTypeDef *phost)
{
  phost->device.is_connected = 1U;
  phost->device.is_disconnected = 0U;
  phost->device.is_ReEnumerated = 0U;

  phost->os_msg = (uint32_t)USBH_PORT_EVENT;
  (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);

  return USBH_OK;
}

/**
  * @brief  USBH_LL_Disconnect
  *         Handle USB Host disconnection event
  * @param  phost: Host Handle
  * @retval USBH_Status
  */
USBH_StatusTypeDef  USBH_LL_Disconnect(USBH_HandleTypeDef *phost)
{
  /* update device connection states */
  phost->device.is_disconnected = 1U;
  phost->device.is_connected = 0U;
  phost->device.PortEnabled = 0U;

  /* Stop Host */
  (void)USBH_LL_Stop(phost);

  /* FRee Control Pipes */
  (void)USBH_FreePipe(phost, phost->Control.pipe_in);
  (void)USBH_FreePipe(phost, phost->Control.pipe_out);
  phost->os_msg = (uint32_t)USBH_PORT_EVENT;
  (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);

  return USBH_OK;
}

/**
  * @brief  USBH_LL_NotifyURBChange
  *         Notify URB state Change
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef  USBH_LL_NotifyURBChange(USBH_HandleTypeDef *phost)
{
  phost->os_msg = (uint32_t)USBH_PORT_EVENT;

  (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);

  return USBH_OK;
}

void GrabUrb(USBH_HandleTypeDef *phost)
{
  osSemaphoreAcquire(phost->control_sem, osWaitForever);
  phost->urb_thread = osThreadGetId();
}

void UngrabUrb(USBH_HandleTypeDef *phost)
{
  phost->urb_thread = 0;
  osSemaphoreRelease(phost->control_sem);
}
