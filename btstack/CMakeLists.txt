set(BTSRCS
  btstack_port.c
  btstack_tlv_flash_bank.c
  hid_gamepad.c
  hal_flash_bank_memory.c
  hci_dump_segger_rtt_stdout.c
  platform/freertos/btstack_run_loop_freertos.c
  src/ad_parser.c
  src/btstack_linked_list.c
  src/btstack_memory.c
  src/btstack_memory_pool.c
  src/btstack_tlv.c
  src/btstack_util.c
  src/hci.c
  src/hci_cmd.c
  src/hci_dump.c
  src/btstack_memory.c
  src/btstack_run_loop.c
  src/l2cap.c
  src/l2cap_signaling.c
  src/classic/hid_host.c
  src/classic/btstack_link_key_db_tlv.c
  src/classic/sdp_client.c
  src/classic/sdp_server.c
  src/classic/sdp_util.c
)

add_library(btstack STATIC ${BTSRCS})

