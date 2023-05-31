/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hid_gamepad.c"

#include "DoomPlayer.h"
/*
 * hid_host_demo.c
 */

/* EXAMPLE_START(hid_host_demo): HID Host Classic
 *
 * @text This example implements a HID Host. For now, it connects to a fixed device.
 * It will connect in Report protocol mode if this mode is supported by the HID Device,
 * otherwise it will fall back to BOOT protocol mode. 
 */

#include <inttypes.h>
#include <stdio.h>

#include "btstack_config.h"
#include "btstack.h"
#include "gamepad.h"
#include "usbh_hid.h"
#include "app_task.h"
#include "app_gui.h"

#define	BTREQ_START_SCAN	(1<<0)
#define	BTREQ_DISCONNECT	(1<<1)
#define	BTREQ_PUSH_REPORT	(1<<2)

typedef struct {
 uint16_t code;
 uint8_t  *data;
 uint16_t length;
} BTREPORT_REQ;

#define	BTREQ_DEPTH	3

static uint8_t btreqBuffer[BTREQ_DEPTH * sizeof(BTREPORT_REQ)];

MESSAGEQ_DEF(btreqq, btreqBuffer, sizeof(btreqBuffer))

EVFLAG_DEF(btreq_flag)

#define MAX_ATTRIBUTE_VALUE_SIZE 300

static bd_addr_t remote_addr;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t l2cap_event_callback_registration;

static hci_con_handle_t con_handle;

#define	MAX_DEVICES 20
enum DEVICE_STATE {
    REMOTE_NAME_INIT, REMOTE_NAME_REQUEST, REMOTE_NAME_INQUIRED, REMOTE_NAME_FETCHED
};

struct device {
    bd_addr_t address;
    uint8_t pageScanRepetitionMode;
    uint16_t clockOffset;
    uint32_t CoD;
    enum DEVICE_STATE state;
};

#define	INQUIRY_INTERVAL 5
struct device devices[MAX_DEVICES];
int deviceCount = 0;

enum STATE {
  INIT, ACTIVE, CONNECT
};

enum STATE state = INIT;

// SDP
static uint8_t hid_descriptor_storage[MAX_ATTRIBUTE_VALUE_SIZE];

// App
static enum {
    APP_IDLE,
    APP_CONNECTED
} app_state = APP_IDLE;

static uint16_t hid_host_cid = 0;
static bool     hid_host_descriptor_available = false;
//static hid_protocol_mode_t hid_host_report_mode = HID_PROTOCOL_MODE_REPORT_WITH_FALLBACK_TO_BOOT;
static hid_protocol_mode_t hid_host_report_mode = HID_PROTOCOL_MODE_REPORT;

static const struct sGamePadDriver *padDriver;
static GAMEPAD_INFO *padInfo;

/* @section Main application configuration
 *
 * @text In the application configuration, L2CAP and HID host are initialized, and the link policies 
 * are set to allow sniff mode and role change. 
 */

/* LISTING_START(PanuSetup): Panu setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void hid_host_setup(void){

    // Initialize L2CAP
    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    sdp_init();

    // Initialize HID Host
    hid_host_init(hid_descriptor_storage, sizeof(hid_descriptor_storage));
    hid_host_register_packet_handler(packet_handler);

    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // try to become master on incoming connections
    hci_set_master_slave_policy(HCI_ROLE_MASTER);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for L2CAP events
    l2cap_event_callback_registration.callback = &packet_handler;
    l2cap_add_event_handler(&l2cap_event_callback_registration);

    // Disable stdout buffering
    setvbuf(stdin, NULL, _IONBF, 0);
}
/* LISTING_END */

static int getDeviceIndexForAddress(bd_addr_t addr) {
  int j;
  for (j = 0; j < deviceCount; j++) {
    if (bd_addr_cmp(addr, devices[j].address) == 0) {
      return j;
    }
  }
  return -1;
}

static void start_scan(void) {
  debug_printf("Starting inquiry scan..\n");
  gap_inquiry_start(INQUIRY_INTERVAL);
}

static int has_more_remote_name_requests(void) {
  int i;
  for (i = 0; i < deviceCount; i++) {
    if (devices[i].state == REMOTE_NAME_REQUEST)
      return 1;
  }
  return 0;
}

static void do_next_remote_name_request(void) {
  int i;
  for (i = 0; i < deviceCount; i++) {
    // remote name request
    if (devices[i].state == REMOTE_NAME_REQUEST) {
      devices[i].state = REMOTE_NAME_INQUIRED;
      debug_printf("Get remote name of %s...\n", bd_addr_to_str(devices[i].address));
      gap_remote_name_request(devices[i].address,
                              devices[i].pageScanRepetitionMode,
                              devices[i].clockOffset | 0x8000);
      return;
    }
  }
}

static void continue_remote_names(void) {
  if (has_more_remote_name_requests()) {
    do_next_remote_name_request();
    return;
  }
  start_scan();
}

static uint16_t hid_vid, hid_pid;

static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  int type, id;
  int offset;
  uint8_t bdata;
  uint16_t *vp;

  type = hci_event_packet_get_type(packet);

  switch (type)
  {
  case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
    id = sdp_event_query_attribute_byte_get_attribute_id(packet);

    switch (id)
    {
    case 0x0201:
      vp = &hid_vid;
      break;
    case 0x202:
      vp = &hid_pid;
      break;
    default:
      return;
      break;
    }

    offset = sdp_event_query_attribute_byte_get_data_offset(packet);
    bdata = sdp_event_query_attribute_byte_get_data(packet);

    switch (offset)
    {
    case 1:
          *vp = bdata << 8;
          break;
    case 2:
          *vp |= bdata;
          break;
    default:
          *vp = 0;
          break;
    }
    break;
  case SDP_EVENT_QUERY_COMPLETE:
      break;
  default:
      break;
  }
}

/*
 * @section Packet Handler
 * 
 * @text The packet handler responds to various HID events.
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    /* LISTING_PAUSE */
    UNUSED(channel);
    UNUSED(size);

    uint8_t   event = 0;
    bd_addr_t event_addr;
    uint8_t   status;
    uint32_t  codval;

    bd_addr_t addr;
    int i, index;

    /* LISTING_RESUME */
    switch (packet_type) {
    case HCI_EVENT_PACKET:
      event = hci_event_packet_get_type(packet);
            
      switch (state) {
      /* @text In INIT, an inquiry scan is started, and the application transit to
       * ACTIVE stae.
       */
      case INIT:
        switch (event) {
        case BTSTACK_EVENT_STATE:
                if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                  //start_scan();
                  state = ACTIVE;
                }
                break;
        default:
                break;
        }
        break;		// End of INIT state

      /* @text In ACTIVE, the following events are processed:
       *  - GAP Inquiry result event: BTstack provices a unified inquiry result that contain
       *    Class of Device (CoD), page scan mode, clock offset. RSSI and name (from EIR) are ootional.
       *
       *  - Inquiry complete event: the remote name is requested for device widthout a fetched name.
       *    The state of a remote name can be one of the following:
       *      REMOTE_NAME_REQUET, REMOTE_NAME_INQUIRED, or REMOTE_NAME_FETCHED.
       *
       *  - Remote name request complet event: the remote name is stored in the table and the
       *    state is updated to REMOTE_NAME_FETCHED. The query of remote name is continued.
       */
      case ACTIVE:
        switch (event) {
        case GAP_EVENT_INQUIRY_RESULT:
          if (deviceCount > MAX_DEVICES)
            break;  // already full
          gap_event_inquiry_result_get_bd_addr(packet, addr);
          index = getDeviceIndexForAddress(addr);
          if (index >= 0)
            break;	// already in our lisst

          memcpy(devices[deviceCount].address, addr, 6);
          devices[deviceCount].pageScanRepetitionMode =
                     gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
          devices[deviceCount].clockOffset =
                     gap_event_inquiry_result_get_clock_offset(packet);
          // print info
          debug_printf("Device found: %s ", bd_addr_to_str(addr));
          codval = (unsigned int) gap_event_inquiry_result_get_class_of_device(packet);
          devices[deviceCount].CoD = codval;
          debug_printf("width COD: 0x%06x, ", codval);
          debug_printf("pageScan %d, ", devices[deviceCount].pageScanRepetitionMode);
          debug_printf("clock offset 0x%04x", devices[deviceCount].clockOffset);
          if (gap_event_inquiry_result_get_rssi_available(packet)) {
                  debug_printf(", rssi %d dBm", (int8_t) gap_event_inquiry_result_get_rssi(packet));
          }
          if (gap_event_inquiry_result_get_name_available(packet)) {
              char name_buffer[240];
              int name_len = gap_event_inquiry_result_get_name_len(packet);

              memcpy(name_buffer, gap_event_inquiry_result_get_name(packet), name_len);
              name_buffer[name_len] = 0;
              debug_printf(", anme '%s'", name_buffer);
              devices[deviceCount].state = REMOTE_NAME_FETCHED;
              if ((strcmp(name_buffer, "Wireless Controller") == 0) && (devices[deviceCount].CoD == 0x002508))
              {
               debug_printf(" -- DualSense has found!!\n");
              }
          }
          else if (devices[deviceCount].CoD == 0x002508) {
              devices[deviceCount].state = REMOTE_NAME_REQUEST;
          }
          if (gap_event_inquiry_result_get_device_id_available(packet)) {
              hid_vid = gap_event_inquiry_result_get_device_id_vendor_id(packet);
              hid_pid = gap_event_inquiry_result_get_device_id_product_id(packet);
debug_printf("vid, pid: %x, %x\n", hid_vid, hid_pid);
          }
          debug_printf("\n");
          deviceCount++;
          break;
        case GAP_EVENT_INQUIRY_COMPLETE:
          for (i = 0; i < deviceCount; i++) {
              // retry remote name request
              if ((devices[i].state == REMOTE_NAME_INQUIRED) && (devices[i].CoD == 0x002508))
                    devices[i].state = REMOTE_NAME_REQUEST;
          }
          continue_remote_names();
          break;

        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
          reverse_bd_addr(&packet[3], addr);
          index = getDeviceIndexForAddress(addr);
          if (index >= 0) {
            if (packet[2] == 0) {
              debug_printf("Name: '%s'\n", &packet[9]);
              devices[index].state = REMOTE_NAME_FETCHED;

              if ((strcmp((char *)&packet[9], "Wireless Controller") == 0) && (devices[index].CoD == 0x002508)) {
                debug_printf(" -- DualSense has found!!\n");
                //state = CONNECT;
                gap_inquiry_stop();
                memcpy(&remote_addr, &devices[index].address, 6);
                sdp_client_query_uuid16(
                      &handle_sdp_client_query_result, remote_addr,
                      BLUETOOTH_SERVICE_CLASS_PNP_INFORMATION);
                debug_printf("Connect to HID dev (%s).\n", bd_addr_to_str(remote_addr));
                status = hid_host_connect(remote_addr, hid_host_report_mode, &hid_host_cid);
                postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_SET | ICON_BLUETOOTH, NULL, NULL);
                postGuiEventMessage(GUIEV_BLUETOOTH_READY, 1, NULL, NULL);
                break;
              }
            }
            else {
                    debug_printf("Failed to get name: pag timeout.\n");
            }
          }
          continue_remote_names();
          break;
        case HCI_EVENT_CONNECTION_COMPLETE:
          gap_inquiry_stop();
          state = CONNECT;
          con_handle = (hci_con_handle_t) little_endian_read_16(packet, 3);
          reverse_bd_addr(&packet[5], remote_addr);
          debug_printf("Connect (%s), handle = %x.\n", bd_addr_to_str(remote_addr), con_handle);
          sdp_client_query_uuid16(
                      &handle_sdp_client_query_result, remote_addr,
                      BLUETOOTH_SERVICE_CLASS_PNP_INFORMATION);
          break;
        default:
          break;
        }		// end of switch(event)
        break;		// end of ACTIVE state
      case CONNECT:
        switch (event) {
        case BTSTACK_EVENT_STATE:
          break;
          /* LISTING_PAUSE */
        case HCI_EVENT_PIN_CODE_REQUEST:
          // inform about pin code request
          debug_printf("Pin code request - using '0000'\n");
          hci_event_pin_code_request_get_bd_addr(packet, event_addr);
          gap_pin_code_response(event_addr, "0000");
          break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
          // inform about user confirmation request
          debug_printf("SSP User Confirmation Request with numeric value '%"PRIu32"'\n",
                                   little_endian_read_32(packet, 8));
          debug_printf("SSP User Confirmation Auto accept\n");
          break;

        case HCI_EVENT_HID_META:
          switch (hci_event_hid_meta_get_subevent_code(packet)) {
          case HID_SUBEVENT_INCOMING_CONNECTION:
            debug_printf("Incoming conn.\n");
            // There is an incoming connection: we can accept it or decline it.
            // The hid_host_report_mode in the hid_host_accept_connection function
            // allows the application to request a protocol mode.
            // For available protocol modes, see hid_protocol_mode_t in btstack_hid.h file.
            hid_host_accept_connection(hid_subevent_incoming_connection_get_hid_cid(packet), hid_host_report_mode);
            break;
                        
          case HID_SUBEVENT_CONNECTION_OPENED:
            // The status field of this event indicates if the control and interrupt
            // connections were opened successfully.
            status = hid_subevent_connection_opened_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
              debug_printf("HID Connection failed, status 0x%x\n", status);
              app_state = APP_IDLE;
              hid_host_cid = 0;
              return;
            }
            app_state = APP_CONNECTED;
            hid_host_descriptor_available = false;
            hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
            debug_printf("HID Host connected (%04x, %04x).\n", hid_vid, hid_pid);
            app_pairing_close();
            postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_SET | ICON_BLUETOOTH, NULL, NULL);
            postGuiEventMessage(GUIEV_BLUETOOTH_READY, 1, NULL, NULL);
            padInfo = IsSupportedGamePad(hid_vid, hid_pid);
            if (padInfo)
            {
              padDriver = padInfo->padDriver;
              padInfo->pclass = NULL;
              postGuiEventMessage(GUIEV_GAMEPAD_READY, 0, padInfo, NULL);
            }
            break;
          case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
#if 0
            // This event will follows HID_SUBEVENT_CONNECTION_OPENED event.
            // For incoming connections, i.e. HID Device initiating the connection,
            // the HID_SUBEVENT_DESCRIPTOR_AVAILABLE is delayed, and some HID
            // reports may be received via HID_SUBEVENT_REPORT event. It is up to
            // the application if these reports should be buffered or ignored until
            // the HID descriptor is available.
            status = hid_subevent_descriptor_available_get_status(packet);
            if (status == ERROR_CODE_SUCCESS){
              hid_host_descriptor_available = true;
              debug_printf("HID Descriptor available, please start typing.\n");
              if (padDriver)
                  (padDriver->btSetup)(hid_host_cid);
            } else {
              debug_printf("Cannot handle input report, HID Descriptor is not available.\n");
            }
#else
            if (padDriver)
              (padDriver->btSetup)(hid_host_cid);
#endif
            break;
          case HID_SUBEVENT_REPORT:
            if (padDriver)
            {
                HID_REPORT report;

                report.ptr = (uint8_t *)hid_subevent_report_get_report(packet);
                report.len = hid_subevent_report_get_report_len(packet);
                report.hid_mode = padInfo->hid_mode;
                padDriver->DecodeInputReport(&report);
            }
            break;
          case HID_SUBEVENT_SET_REPORT_RESPONSE:
            debug_printf("report_response: %x\n",
hid_subevent_set_report_response_get_handshake_status(packet));
            break;
          case HID_SUBEVENT_SET_PROTOCOL_RESPONSE:
            // For incoming connections, the library will set the protocol mode of the
            // HID Device as requested in the call to hid_host_accept_connection. The event
            // reports the result. For connections initiated by calling hid_host_connect,
            // this event will occur only if the established report mode is boot mode.
            status = hid_subevent_set_protocol_response_get_handshake_status(packet);
            if (status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                debug_printf("Error set protocol, status 0x%02x\n", status);
                break;
            }
            switch ((hid_protocol_mode_t)hid_subevent_set_protocol_response_get_protocol_mode(packet)){
            case HID_PROTOCOL_MODE_BOOT:
              debug_printf("Protocol mode set: BOOT.\n");
              break;
            case HID_PROTOCOL_MODE_REPORT:
              debug_printf("Protocol mode set: REPORT.\n");
              break;
            default:
              debug_printf("Unknown protocol mode.\n");
              break;
            }
            break;
          case HID_SUBEVENT_CONNECTION_CLOSED:
            // The connection was closed.
            hid_host_cid = 0;
            hid_host_descriptor_available = false;
            debug_printf("HID Host disconnected.\n");
            postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_CLEAR | ICON_BLUETOOTH, NULL, NULL);
            postGuiEventMessage(GUIEV_BLUETOOTH_READY, 0, NULL, NULL);
            gap_disconnect(con_handle);
            break;
          case HID_SUBEVENT_GET_REPORT_RESPONSE:
            status = hid_subevent_get_report_response_get_handshake_status(packet);
            if (status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                  printf("Error get report, status 0x%02x\n", status);
                  break;
            }
            debug_printf("Received report[%d]: ", hid_subevent_get_report_response_get_report_len(packet));
            debug_printf("\n");
            if (padDriver)
                (padDriver->btProcessGetReport)(hid_subevent_get_report_response_get_report(packet), hid_subevent_get_report_response_get_report_len(packet));
            break;
          default:
            break;
          }	/* end of subevent */
          break;	/* end of HCI_EVENT_HID_META */
        case L2CAP_EVENT_CHANNEL_CLOSED:
          debug_printf("L2CAP: chan close.\n");
          break;
           
        case HCI_EVENT_DISCONNECTION_COMPLETE:
          state = ACTIVE;
          if (padDriver)
            (padDriver->btDisconnect)();
          break;
        default:
          break;
        }		/* end of switch(event) */
        break;	/* end of CONNECT state */
      default:
        break;
      }	/* end of switch(state) */
      break;	/* end of HCI_EVENT_PACKET */
    case L2CAP_DATA_PACKET:
#if 0
printf("%d: ", size);
            printf_hexdump(packet, size);
#endif
      break;
    default:
      break;
    }	/* end of switch(packet_type) */
}

static osMessageQueueId_t btreqqId;
static osEventFlagsId_t btreqflagId;

static void btapi_setup()
{
  btreqflagId = osEventFlagsNew(&attributes_btreq_flag);
  btreqqId = osMessageQueueNew(BTREQ_DEPTH, sizeof(BTREPORT_REQ), &attributes_btreqq);
}

void process_btapi_request()
{
  int32_t evflag;
  BTREPORT_REQ req;

  evflag = osEventFlagsGet(btreqflagId);
  if (evflag > 0)
  {
    if (evflag & BTREQ_START_SCAN)
    {
      start_scan();
      state = ACTIVE;
      osEventFlagsClear(btreqflagId, BTREQ_START_SCAN);
    }
    if (evflag & BTREQ_DISCONNECT)
    {
      if (state == CONNECT && hid_host_cid != 0)
      {
        hid_host_disconnect(hid_host_cid);
        osEventFlagsClear(btreqflagId, BTREQ_DISCONNECT);
      }
    }
    if (evflag & BTREQ_PUSH_REPORT)
    {
      if (osMessageQueueGet(btreqqId, &req, 0, 0) == osOK)
      {
        hid_host_send_report(hid_host_cid, req.code, req.data + 1, req.length - 1);
        if (padDriver)
          (*padDriver->btReleaseBuffer)(req.data);
      }
      osEventFlagsClear(btreqflagId, BTREQ_PUSH_REPORT);
    }
  }
}

void btapi_start_scan()
{
  osEventFlagsSet(btreqflagId, BTREQ_START_SCAN);
  btstack_run_loop_poll_data_sources_from_irq();
}

void btapi_disconnect()
{
  if (btreqflagId)
  {
    osEventFlagsSet(btreqflagId, BTREQ_DISCONNECT);
    btstack_run_loop_poll_data_sources_from_irq();
  }
}

void btapi_send_report(uint8_t *ptr, int len)
{
  BTREPORT_REQ request;

  SCB_CleanDCache();
  request.code = *ptr;
  request.data = ptr;
  request.length = len;
  osMessageQueuePut(btreqqId, &request, 0, 0);
}

void btapi_push_report()
{
  osEventFlagsSet(btreqflagId, BTREQ_PUSH_REPORT);
  btstack_run_loop_poll_data_sources_from_irq();
}

int btstack_main(int argc, const char * argv[])
{

  (void)argc;
  (void)argv;

  btapi_setup();

  hid_host_setup();

  // Turn on the device 
  hci_power_control(HCI_POWER_ON);
  return 0;
}

/* EXAMPLE_END */
