/*
 * Copyright (C) 2020 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "usbh_bluetooth.c"
#include "DoomPlayer.h"
#include "app_task.h"
#include "usbh_pipes.h"
#include "usbh_ioreq.h"
#include "usbh_ctlreq.h"
#include "usbh_bluetooth.h"
#include "btstack_debug.h"
#include "hci.h"
#include "btstack_util.h"
#include "bluetooth.h"
#include "hci_if.h"
#include "gamepad.h"
#include "usbh_hid.h"

typedef struct {
    uint8_t acl_in_ep;
    uint8_t acl_in_pipe;
    uint16_t acl_in_len;
    uint32_t acl_in_frame;
    uint8_t acl_out_ep;
    uint8_t acl_out_pipe;
    uint32_t acl_out_frame;
    uint16_t acl_out_len;
    uint8_t event_in_ep;
    uint8_t event_in_pipe;
    uint16_t event_in_len;
    uint32_t event_in_frame;
} USB_Bluetooth_t;

#define	BTEV_SOF_BIT	(1<<5)
#define	BTOUT_CMD_BIT	(1<<6)
#define	BTOUT_ACL_BIT	(1<<7)
#define	BTIN_ACL_BIT	(1<<8)

static enum {
    USBH_OUT_OFF,
    USBH_OUT_IDLE,
    USBH_OUT_CMD,
} usbh_out_state;

EVFLAG_DEF(btdongle_flag)

#define EVMSGQ_DEPTH     6
static uint8_t evmsgqBuffer[EVMSGQ_DEPTH * sizeof(PIPE_EVENT)];

MESSAGEQ_DEF(evmsgq, evmsgqBuffer, sizeof(evmsgqBuffer))

static uint8_t hcievqBuffer[HCIEVQ_DEPTH * sizeof( HCIEVT ) ];

#define	EVB_SIZE	260

MESSAGEQ_DEF(hcievq, hcievqBuffer, sizeof(hcievqBuffer))

static SECTION_USBSRAM uint8_t evbuffqBuffer[BUFFQ_DEPTH * EVB_SIZE];
static uint8_t *evptrBuffer[BUFFQ_DEPTH];

MESSAGEQ_DEF(evbuffq, evptrBuffer, sizeof(evptrBuffer))

HCIIF_INFO HciIfInfo;

extern void postHCIEvent(uint8_t ptype, uint8_t *pkt, uint16_t size);
extern void transport_set_send_now(int val);
extern void usbh_bluetooth_send_cmd(const uint8_t * packet, uint16_t len);
extern void set_hid_class(USBH_ClassTypeDef *class);

// class state
static USB_Bluetooth_t usb_bluetooth;

void StartBTDongleTask(void *arg);

// outgoing
static const uint8_t * cmd_packet;
static uint16_t        cmd_len;

static const uint8_t * acl_packet;
static uint16_t        acl_len;

// incoming

static SECTION_USBSRAM uint8_t  acl_in_buffer0[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_ACL_BUFFER_SIZE];
static SECTION_USBSRAM uint8_t  acl_in_buffer1[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_ACL_BUFFER_SIZE];
static uint8_t *aclptrBuffer[BUFFQ_DEPTH];

MESSAGEQ_DEF(aclbuffq, aclptrBuffer, sizeof(aclptrBuffer))

#define	BTD_STACK_SIZE	500
//TASK_DEF(btdtask, BTD_STACK_SIZE, osPriorityNormal5)
TASK_DEF(btdtask, BTD_STACK_SIZE, osPriorityAboveNormal5)

#define	BTS_STACK_SIZE	800
TASK_DEF(btstacktask, BTS_STACK_SIZE, osPriorityAboveNormal6)
//TASK_DEF(btstacktask, BTS_STACK_SIZE, osPriorityNormal2)

extern void StartBtstackTask(void *arg);


USBH_StatusTypeDef USBH_Bluetooth_InterfaceInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
    debug_printf("USBH_Bluetooth_InterfaceInit\n");

    // dump everything
    uint8_t interface_index = 0;
    USBH_InterfaceDescTypeDef * interface = &phost->device.CfgDesc.Itf_Desc[interface_index];
    uint8_t num_endpoints = interface->bNumEndpoints;
    uint8_t ep_index;
    int16_t acl_in   = -1;
    int16_t acl_out  = -1;
    int16_t event_in = -1;
    for (ep_index=0;ep_index<num_endpoints;ep_index++){
        USBH_EpDescTypeDef * ep_desc = &interface->Ep_Desc[ep_index];
        debug_printf("Interface %u, endpoint #%u: address 0x%02x, attributes 0x%02x, packet size %u, poll %u ",
               interface_index, ep_index, ep_desc->bEndpointAddress, ep_desc->bmAttributes, ep_desc->wMaxPacketSize, ep_desc->bInterval);
        // type interrupt, direction incoming
        if  (((ep_desc->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_MSK) && (ep_desc->bmAttributes == USB_EP_TYPE_INTR)){
            event_in = ep_index;
            debug_printf("-> HCI Event\n");
        }
        // type bulk, direction incoming
        if  (((ep_desc->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_MSK) && (ep_desc->bmAttributes == USB_EP_TYPE_BULK)){
            acl_in = ep_index;
            debug_printf("-> HCI ACL IN\n");
        }
        // type bulk, direction incoming
        if  (((ep_desc->bEndpointAddress & USB_EP_DIR_MSK) == 0) && (ep_desc->bmAttributes == USB_EP_TYPE_BULK)){
            acl_out = ep_index;
            debug_printf("-> HCI ACL OUT\n");
        }
    }

    memset(acl_in_buffer0, 0, sizeof(acl_in_buffer0));
    memset(acl_in_buffer1, 0, sizeof(acl_in_buffer1));

    // all found
    if ((acl_in < 0) && (acl_out < 0) && (event_in < 0)) {
        debug_printf("Could not find all endpoints\n");
        return USBH_FAIL;
    }

    if (pclass->cState)
      return USBH_FAIL;

    // setup
    memset(&usb_bluetooth, 0, sizeof(USB_Bluetooth_t));
    pclass->pData = (void*) &usb_bluetooth;

    // Command
    usbh_out_state = USBH_OUT_OFF;

    // Event In
    USB_Bluetooth_t * usb = &usb_bluetooth;
    usb->event_in_ep =   interface->Ep_Desc[event_in].bEndpointAddress;
    usb->event_in_len =  interface->Ep_Desc[event_in].wMaxPacketSize;
    usb->event_in_pipe = USBH_AllocPipe(phost, usb->event_in_ep);

    /* Open pipe for IN endpoint */
    USBH_OpenPipe(phost, usb->event_in_pipe, usb->event_in_ep, phost->device.address,
                  phost->device.speed, USB_EP_TYPE_INTR, interface->Ep_Desc[event_in].wMaxPacketSize);

    USBH_LL_SetToggle(phost, usb->event_in_ep, 1U);

    // ACL In
    usb->acl_in_ep  =  interface->Ep_Desc[acl_in].bEndpointAddress;
    usb->acl_in_len =  interface->Ep_Desc[acl_in].wMaxPacketSize;
    usb->acl_in_pipe = USBH_AllocPipe(phost, usb->acl_in_ep);
    USBH_OpenPipe(phost, usb->acl_in_pipe, usb->acl_in_ep, phost->device.address, phost->device.speed, USB_EP_TYPE_BULK, usb->acl_in_len);
    USBH_LL_SetToggle(phost, usb->acl_in_pipe, 0U);


    // ACL Out
    usb->acl_out_ep  =  interface->Ep_Desc[acl_out].bEndpointAddress;
    usb->acl_out_len =  interface->Ep_Desc[acl_out].wMaxPacketSize;
    usb->acl_out_pipe = USBH_AllocPipe(phost, usb->acl_out_ep);
    USBH_OpenPipe(phost, usb->acl_out_pipe, usb->acl_out_ep, phost->device.address, phost->device.speed, USB_EP_TYPE_BULK, usb->acl_out_len);
    USBH_LL_SetToggle(phost, usb->acl_out_pipe, 0U);
debug_printf("pipe: event = %d, acl_in = %d, acl_out = %d\n", usb->event_in_pipe, usb->acl_in_pipe, usb->acl_out_pipe);

    pclass->phost = phost;
    pclass->classEventQueue = osMessageQueueNew(EVMSGQ_DEPTH, sizeof(PIPE_EVENT), &attributes_evmsgq);

    HCIIF_INFO *info;

    info = &HciIfInfo;
    info->hcievqId = osMessageQueueNew (HCIEVQ_DEPTH, sizeof(HCIEVT), &attributes_hcievq);
    info->dongle_flagId = osEventFlagsNew(&attributes_btdongle_flag);

    set_hid_class(pclass);

    osThreadNew(StartBTDongleTask, pclass, &attributes_btdtask);
    //osThreadNew(StartBtstackTask, NULL, &attributes_btstacktask);

    return USBH_OK;
}

USBH_StatusTypeDef USBH_Bluetooth_InterfaceDeInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost){
    debug_printf("USBH_Bluetooth_InterfaceDeInit\n");
    usbh_out_state = USBH_OUT_OFF;
    return USBH_OK;
}

USBH_StatusTypeDef USBH_Bluetooth_ClassRequest(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost){
debug_printf("%s:\n", __FUNCTION__);
    // ready!
    //usbh_out_state = USBH_OUT_IDLE;
    // notify host stack
    return USBH_OK;
}

USBH_StatusTypeDef USBH_Bluetooth_Process(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost){

    return USBH_OK;
}

USBH_StatusTypeDef USBH_Bluetooth_SOFProcess(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
    PIPE_EVENT pev;

    if (osMessageQueueGetSpace(pclass->classEventQueue) < 3)
      return USBH_OK;

    pev.channel = 0;
    pev.state = BTEV_SOF_BIT;
    osMessageQueuePut(pclass->classEventQueue, &pev, 0, 0);
    return USBH_OK;
}

USBH_ClassTypeDef  Bluetooth_Class = {
    "Bluetooth",
    USB_BLUETOOTH_CLASS,
    USBH_Bluetooth_InterfaceInit,
    USBH_Bluetooth_InterfaceDeInit,
    USBH_Bluetooth_ClassRequest,
    USBH_Bluetooth_Process,
    USBH_Bluetooth_SOFProcess,
    NULL,
};


static const uint8_t hci_psent_event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};

void StartBTDongleTask(void *arg)
{
  USBH_HandleTypeDef *phost;
  USBH_StatusTypeDef status;
  USB_Bluetooth_t * usb;
  HCIIF_INFO *info;
  int st;
  USBH_ClassTypeDef *pclass;

  pclass  = (USBH_ClassTypeDef *)arg;
  phost = pclass->phost;
  usb = (USB_Bluetooth_t *) pclass->pData;
  info = &HciIfInfo;

  /* Discard any pending events */
  osThreadFlagsWait (0xff, osFlagsWaitAny, 0);
  osThreadFlagsWait (0xff, osFlagsWaitAny, 0);

  uint8_t *bp;

  info->evbuffqId = osMessageQueueNew (BUFFQ_DEPTH, sizeof(uint8_t *), &attributes_evbuffq);
  bp = evbuffqBuffer;
  osMessageQueuePut(info->evbuffqId, &bp, 0, 0);
  bp += EVB_SIZE;
  osMessageQueuePut(info->evbuffqId, &bp, 0, 0);

  info->aclbuffqId = osMessageQueueNew (BUFFQ_DEPTH, sizeof(uint8_t *), &attributes_aclbuffq);
  bp = acl_in_buffer0;
  osMessageQueuePut(info->aclbuffqId, &bp, 0, 0);
  bp = acl_in_buffer1;
  osMessageQueuePut(info->aclbuffqId, &bp, 0, 0);
  info->acl_offset = 0;
  info->acl_out_offset = 0;
  info->dma_flag = 0;

  usb->event_in_frame = 0;
  usb->acl_in_frame = 0;

  usbh_out_state = USBH_OUT_IDLE;

  postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_SET | ICON_USB, NULL, NULL);

    pclass->cState = 1;
    pclass->phost->PipeEvq[0] = pclass->classEventQueue;
    pclass->phost->PipeEvq[usb->event_in_pipe & 0x0F] = pclass->classEventQueue;
    pclass->phost->PipeEvq[usb->acl_in_pipe & 0x0F] = pclass->classEventQueue;
    pclass->phost->PipeEvq[usb->acl_out_pipe & 0x0F] = pclass->classEventQueue;
  osThreadNew(StartBtstackTask, NULL, &attributes_btstacktask);

  while (1)
  {
    uint32_t XferSize;
    uint8_t  event_transfer_size;
    uint16_t event_size;
    uint16_t transfer_size;
    uint16_t acl_in_transfer_size;

    PIPE_EVENT pev;
    uint16_t acl_size;

    osMessageQueueGet(pclass->classEventQueue, &pev, NULL, osWaitForever);

    switch (pev.state)
    {
    case BTEV_SOF_BIT:
      if (info->dma_flag == 0)
      {
        /* Initiate Event input transaction */

        if (info->evb_offset == 0)
        {
          if (osMessageQueueGet(info->evbuffqId, &bp, 0, 0) == osOK)
          {
            info->ev_buffer = bp;
            event_transfer_size = btstack_min(usb->event_in_len, EVB_SIZE - info->evb_offset);
            usb->event_in_frame = phost->Timer;
            info->dma_flag |= DMA_EVINPUT;
            SCB_CleanInvalidateDCache();
            st = USBH_InterruptReceiveData(phost, info->ev_buffer,
		event_transfer_size, usb->event_in_pipe);
            if (st)
debug_printf("st0 = %d\n", st);
          }
          else
          {
            debug_printf("no buffer.\n");
          }
        }
        else
        {
            event_transfer_size = btstack_min(usb->event_in_len, EVB_SIZE - info->evb_offset);
            usb->event_in_frame = phost->Timer;
            info->dma_flag |= DMA_EVINPUT;
            SCB_CleanInvalidateDCache();
            st = USBH_InterruptReceiveData(phost, info->ev_buffer + info->evb_offset,
		event_transfer_size, usb->event_in_pipe);
            if (st)
debug_printf("st1 = %d\n", st);
        }

        /* Initiate ACL read transaction */

        if (info->acl_offset == 0)
        {
            if (osMessageQueueGet(info->aclbuffqId, &bp, 0, 0) == osOK)
            {
              acl_in_transfer_size = btstack_min(usb->acl_in_len, HCI_ACL_BUFFER_SIZE);

              info->acl_buffer = bp;
              SCB_CleanInvalidateDCache();
//debug_printf("Bulk 0 (%d)\n", usb->acl_in_pipe);
              info->dma_flag |= DMA_ACLINPUT;
              USBH_BulkReceiveData(phost, info->acl_buffer, acl_in_transfer_size, usb->acl_in_pipe);
              usb->acl_in_frame = phost->Timer;
            }
            else
            {
              debug_printf("no ACL buffer.\n");
            }
        }
        else
        {
//debug_printf("Bulk 1 (%d, %d)\n", usb->acl_in_pipe, info->acl_offset);
            acl_in_transfer_size = btstack_min(usb->acl_in_len, HCI_ACL_BUFFER_SIZE - info->acl_offset);
            usb->event_in_frame = phost->Timer;
            SCB_CleanInvalidateDCache();
            info->dma_flag |= DMA_ACLINPUT;
            USBH_BulkReceiveData(phost, info->acl_buffer + info->acl_offset,
		acl_in_transfer_size, usb->acl_in_pipe);
        }
      }

      /* See if we need to send remaining ACL out packets */
      //if ((info->dma_flag == 0) && (info->acl_out_offset > 0))
      if (!(info->dma_flag & DMA_ACLOUTPUT) && (info->acl_out_offset > 0))
      {
        transfer_size = btstack_min(usb->acl_out_len, acl_len - info->acl_out_offset);
        info->dma_flag |= DMA_ACLOUTPUT;
        USBH_BulkSendData(phost, (uint8_t *) acl_packet + info->acl_out_offset, transfer_size, usb->acl_out_pipe, 0);
      }
      break;
    //case USBH_URB_IDLE:
    case USBH_URB_NOTREADY:
      if (pev.channel == usb->acl_in_pipe)
      {
        {
          usb->acl_in_frame = phost->Timer;
          bp = info->acl_buffer;
          info->dma_flag &= ~DMA_ACLINPUT;
          osMessageQueuePut(info->aclbuffqId, &bp, 0, 0);
        }
      }
      else if (pev.channel == usb->event_in_pipe)
      {
        if (info->dma_flag & DMA_EVINPUT)
          debug_printf("URB_IDLE for event.\n");
      }
#if 0
      else
        debug_printf("URB_IDLE: %d\n", pev.channel);
#endif
      break;
    case USBH_URB_DONE:
      SCB_CleanInvalidateDCache();
      if (pev.channel == 0)
      {
        if (usbh_out_state == USBH_OUT_CMD)
        {
          UngrabUrb(phost);
          postHCIEvent(HCI_EVENT_PACKET, (uint8_t *)hci_psent_event, sizeof(hci_psent_event));
          transport_set_send_now(1);
          usbh_out_state = USBH_OUT_IDLE;
        }
      }
      else if (pev.channel == 1)
      {
        if (usbh_out_state == USBH_OUT_CMD)
        {
          if (phost->Control.state == CTRL_STATUS_IN_WAIT)
            phost->Control.state = CTRL_COMPLETE;
          if (phost->Control.state == CTRL_COMPLETE)
          {
            UngrabUrb(phost);
            postHCIEvent(HCI_EVENT_PACKET, (uint8_t *)hci_psent_event, sizeof(hci_psent_event));
            transport_set_send_now(1);
            usbh_out_state = USBH_OUT_IDLE;
          }
        }
      }
      else if (pev.channel == usb->event_in_pipe)
      {
        XferSize = USBH_LL_GetLastXferSize(phost, usb->event_in_pipe);
        if (info->dma_flag & DMA_EVINPUT)
        {
          if (XferSize > 0)
          {
            info->evb_offset += XferSize;
            if (info->evb_offset < 2)
              break;
            event_size = 2 + info->ev_buffer[1];
            if (info->evb_offset >= event_size)
            {
              postHCIEvent(HCI_EVENT_PACKET, info->ev_buffer, event_size);
              info->evb_offset = 0;
            }
          }
          else
          {
            //osEventFlagsSet(info->dongle_flagId, BTIN_ACL_BIT);
          }
        }
        info->dma_flag &= ~DMA_EVINPUT;
        bp = info->ev_buffer;
        osMessageQueuePut(info->evbuffqId, &bp, 0, 0);
      }
      else if (pev.channel == usb->acl_in_pipe)
      {
        if (info->dma_flag & DMA_ACLINPUT)
        {
          info->dma_flag &= ~DMA_ACLINPUT;
          XferSize = USBH_LL_GetLastXferSize(phost, usb->acl_in_pipe);
#ifdef DEBUG_ACL
          debug_printf("ACL_IN -- %d, state = %x\n", XferSize, USBH_LL_GetURBState(phost, usb->acl_in_pipe));
#endif
          if (XferSize > 0)
          {
            SCB_CleanInvalidateDCache();
            info->acl_offset += XferSize;
            acl_size = 4 + little_endian_read_16(info->acl_buffer, 2);
            if (info->acl_offset >= acl_size)
            {
              info->acl_offset = 0;
              postHCIEvent(HCI_ACL_DATA_PACKET, info->acl_buffer, acl_size);
              bp = info->acl_buffer;
              osMessageQueuePut(info->aclbuffqId, &bp, 0, 0);
            }
            else
            {
            }
          }
          usb->acl_in_frame = phost->Timer;
        }
      }
      else if (pev.channel == usb->acl_out_pipe)
      {
        info->dma_flag &= ~DMA_ACLOUTPUT;
        {
          info->acl_out_offset += usb->acl_out_len;
          if (info->acl_out_offset >= acl_len)
          {
            info->acl_out_offset = 0;
            postHCIEvent(HCI_EVENT_PACKET, (uint8_t *)hci_psent_event, sizeof(hci_psent_event));
            transport_set_send_now(1);
          }
        }
      }
      else
      {
        debug_printf("pipe = %d\n", pev.channel);
      }
      break;
    default:
      debug_printf("ev st = %d, chan = %d\n", pev.state, pev.channel);
      break;
    }

    uint32_t req_flags;

    req_flags = osEventFlagsGet(info->dongle_flagId);

    if (req_flags & BTOUT_CMD_BIT)
    {
      transport_set_send_now(0);
      GrabUrb(phost);

      phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_CLASS;
      phost->Control.setup.b.bRequest = 0;
      phost->Control.setup.b.wValue.w = 0;
      phost->Control.setup.b.wIndex.w = 0U;
      phost->Control.setup.b.wLength.w = cmd_len;
      SCB_CleanInvalidateDCache();
      usbh_out_state = USBH_OUT_CMD;
      USBH_CtlReq(phost, (uint8_t *) cmd_packet, cmd_len);
      do
      {
        status = USBH_CtlReq(phost, (uint8_t *) cmd_packet, cmd_len);
      } while (status == USBH_BUSY);
      osEventFlagsClear(info->dongle_flagId, BTOUT_CMD_BIT);
    }
    if (req_flags & BTOUT_ACL_BIT)
    {
      //if ((info->dma_flag == 0) && (phost->Timer & 3) == 1)
      //if (info->dma_flag == 0)
      if (!(info->dma_flag & DMA_ACLOUTPUT))
      {
        transport_set_send_now(0);
        transfer_size = btstack_min(usb->acl_out_len, acl_len);
        info->acl_out_offset = 0;
        info->dma_flag |= DMA_ACLOUTPUT;
        USBH_BulkSendData(phost, (uint8_t *) acl_packet, transfer_size, usb->acl_out_pipe, 0);
        usb->acl_out_frame = phost->Timer;
        osEventFlagsClear(info->dongle_flagId, BTOUT_ACL_BIT);
      }
    }
  }
}

void usbh_bluetooth_send_cmd(const uint8_t * packet, uint16_t len){
    //btstack_assert(usbh_out_state == USBH_OUT_IDLE);
    cmd_packet = packet;
    cmd_len    = len;
    HCIIF_INFO *info;

    info = &HciIfInfo;
    osEventFlagsSet(info->dongle_flagId, BTOUT_CMD_BIT);
}

void usbh_bluetooth_send_acl(const uint8_t * packet, uint16_t len){
    //btstack_assert(usbh_out_state == USBH_OUT_IDLE);
    acl_packet = packet;
    acl_len    = len;
    HCIIF_INFO *info;

    info = &HciIfInfo;
    osEventFlagsSet(info->dongle_flagId, BTOUT_ACL_BIT);
}
