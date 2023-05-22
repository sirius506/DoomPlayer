#include "DoomPlayer.h"
#include "btstack.h"
#include "btstack_run_loop_freertos.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hci_transport.h"
#include "hci_dump_segger_rtt_stdout.h"
#include "hci_if.h"

#include "hal_flash_bank_memory.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;

static int hci_acl_can_send_now;
// data source for integration with BTstack Runloop
static btstack_data_source_t transport_data_source;
static btstack_data_source_t appapi_data_source;

static hal_flash_bank_memory_t hal_flash_bank_context;

#define	STORAGE_SIZE	(1024*8)
SECTION_SRDSRAM static uint8_t tlvStorage_p[STORAGE_SIZE];

#include "hal_time_ms.h"
uint32_t hal_time_ms(void)
{
#if 0
  return HAL_GetTick();
#else
  return osKernelGetTickCount();
#endif
}

static void (*transport_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

// run from main thread

static void transport_send_hardware_error(uint8_t error_code){
    uint8_t event[] = { HCI_EVENT_HARDWARE_ERROR, 1, error_code};
    transport_packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
}

#if 0
static void transport_notify_packet_send(void){
    // notify upper stack that it might be possible to send again
    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    transport_packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
}
#endif

static void transport_notify_ready(void){
    // notify upper stack that it transport is ready
    uint8_t event[] = { HCI_EVENT_TRANSPORT_READY, 0};
    transport_packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
}

static void transport_deliver_hci_packets(void)
{
  HCIEVT hciEvent;
  HCIEVT *p;

  while (osMessageQueueGet(HciIfInfo.hcievqId, &hciEvent, NULL, 0) == osOK)
  {
    p = &hciEvent;

    switch (p->packet_type)
    {
      case HCI_EVENT_PACKET:
        /* Send buffer to upper stack */
        transport_packet_handler(
                    p->packet_type, p->packet, p->size);
        break;

      case HCI_ACL_DATA_PACKET:
        /* Send buffer to upper stack */
        transport_packet_handler(
                    p->packet_type, p->packet, p->size);
        break;
            
      default:
        transport_send_hardware_error(0x01);  // invalid HCI packet
        break;
    }
  }
}

static int tlv_init;

static void transport_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_POLL:
            if (tlv_init == 0)
            {
              transport_notify_ready();
              tlv_init = 1;
            }
            // process hci packets
            transport_deliver_hci_packets();
            break;
        default:
            break;
    }
}

static void appapi_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_POLL:
//debug_printf("process BT api\n");
            process_btapi_request();
            break;
        default:
            break;
    }
}

/**
 * init transport
 * @param transport_config
 */
static void transport_init(const void *transport_config){

    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&transport_data_source, &transport_process);
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&transport_data_source);

    hci_acl_can_send_now = 1;

    btstack_run_loop_set_data_source_handler(&appapi_data_source, &appapi_process);
    btstack_run_loop_enable_data_source_callbacks(&appapi_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&appapi_data_source);
}

void transport_set_send_now(int val)
{
    hci_acl_can_send_now = val;
}

/**
 * open transport connection
 */
static int transport_open(void){
    usbh_dongle_enable();
    return 0;
}

/**
 * close transport connection
 */
static int transport_close(void){
    return 0;
}

/**
 * register packet handler for HCI packets: ACL and Events
 */
static void transport_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    debug_printf("transport_register_packet_handler\n");
    transport_packet_handler = handler;
}

/**
 * support async transport layers, e.g. IRQ driven without buffers
 */
static int transport_can_send_packet_now(uint8_t packet_type) {
    switch (packet_type)
    {
        case HCI_COMMAND_DATA_PACKET:
            //return 1;
            return hci_acl_can_send_now;

        case HCI_ACL_DATA_PACKET:
            return hci_acl_can_send_now;
    }
    return 1;
}



/**
 * send packet
 */
static int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size)
{
    switch (packet_type){ 
        case HCI_COMMAND_DATA_PACKET:
            usbh_bluetooth_send_cmd(packet, size);
            return 0;
        case HCI_ACL_DATA_PACKET:
            usbh_bluetooth_send_acl(packet, size);
            return 0;
        default:
            break;
    }
    return -1;
}

static const hci_transport_t transport = {
    "stm32h7-hci",
    &transport_init,
    &transport_open,
    &transport_close,
    &transport_register_packet_handler,
    &transport_can_send_packet_now,
    &transport_send_packet,
    NULL, // set baud rate
    NULL, // reset link
    NULL, // set SCO config
};


static const hci_transport_t * transport_get_instance(void){
    return &transport;
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void setup_dbs(void){

    const hal_flash_bank_t * hal_flash_bank_impl = hal_flash_bank_memory_init_instance(
            &hal_flash_bank_context,
            tlvStorage_p,
            STORAGE_SIZE);

    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(
            &btstack_tlv_flash_bank_context,
            hal_flash_bank_impl,
            &hal_flash_bank_context);

    // setup global TLV
    btstack_tlv_set_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);

    hci_set_link_key_db(btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context));
#if 0
    // configure LE Device DB for TLV
    le_device_db_tlv_configure(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
#endif
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;

    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_TRANSPORT_READY:
            setup_dbs();
            break;
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            gap_local_bd_addr(local_addr);
            debug_printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
            app_pairing_open();
            break;
        default:
            break;
    }
}

void btstack_assert_failed(const char * file, uint16_t line_nr)
{
    debug_printf("ASSERT in %s, line %u failed - HALT\n", file, line_nr);
    while(1) osDelay(100);
}

void StartBtstackTask(void *arg)
{
  UNUSED(arg);

  //hci_dump_init(hci_dump_segger_rtt_stdout_get_instance());

  // start with BTstack init - especially configure HCI Transport
  btstack_memory_init();
  btstack_run_loop_init(btstack_run_loop_freertos_get_instance());

  // init HCI
  hci_init(transport_get_instance(), NULL);

  // inform about BTstack state
  hci_event_callback_registration.callback = &packet_handler;
  hci_add_event_handler(&hci_event_callback_registration);

  // hand over to btstack embedded code
  btstack_main(0, NULL);

  // go
  btstack_run_loop_execute();
}

void postHCIEvent(uint8_t ptype, uint8_t *pkt, uint16_t size)
{
  HCIEVT evtdata;

  if (HciIfInfo.hcievqId)
  {
    evtdata.packet_type = ptype;
    evtdata.packet = pkt;
    evtdata.size = size;
    if (osMessageQueuePut(HciIfInfo.hcievqId, &evtdata, 0, osWaitForever) != osOK)
      debug_printf("hci event post failed.\n");
    btstack_run_loop_poll_data_sources_from_irq();
  }
}
