set(USBSRCS
  usbh_core.c
  usbh_ctlreq.c
  usbh_audio.c
  usbh_hid.c
  usbh_hid_keybd.c
  usbh_hid_dualsense.c
  usbh_hid_parser.c
  usbh_ioreq.c
  usbh_pipes.c
)

add_library(usb STATIC ${USBSRCS})
