//
// btstack_config.h for STM32 F4 Discovery + USB Bluetooth Dongle port
//
// Documentation: https://bluekitchen-gmbh.com/btstack/#how_to/
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
//#define HAVE_BTSTACK_STDIN
#define HAVE_EMBEDDED_TIME_MS
#define	HAVE_FREERTOS_TASK_NOTIFICATIONS


// BTstack features that can be enabled
//#define ENABLE_BLE
#define ENABLE_BTSTACK_ASSERT
#define ENABLE_CLASSIC
#define	ENABLE_CROSS_TRANSPORT_KEY_DERIVATION
//#define ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
#define ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
#define ENABLE_LOG_INFO
#define ENABLE_LOG_ERROR
//#define ENABLE_PRINTF_HEXDUMP
#define ENABLE_SEGGER_RTT

#define ENABLE_SDP_EXTRA_QUERIES

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES  2
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS 2
#define	MAX_NR_HID_HOST_CONNECTIONS 1
#define MAX_NR_L2CAP_CHANNELS  4
#define MAX_NR_L2CAP_SERVICES  3
#define MAX_NR_RFCOMM_CHANNELS 1
#define MAX_NR_RFCOMM_MULTIPLEXERS 1
#define MAX_NR_RFCOMM_SERVICES 1
#define MAX_NR_SERVICE_RECORD_ITEMS 4
#define MAX_NR_SM_LOOKUP_ENTRIES 3
#define MAX_NR_WHITELIST_ENTRIES 1

// Link Key DB and LE Device DB using TLV on top of Flash Sector interface
#define NVM_NUM_DEVICE_DB_ENTRIES 16
#define NVM_NUM_LINK_KEYS 16

#endif
