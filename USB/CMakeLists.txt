set(USBSRCS
  usbh_bluetooth.c
  usbh_core.c
  usbh_ctlreq.c
  usbh_audio.c
  usbh_hid.c
  usbh_hid_keybd.c
  dualsense.c
  dualshock4.c
  8bitdozero2.c
  gamepad.c
  usbh_hid_parser.c
  usbh_ioreq.c
  usbh_pipes.c
)

add_library(usb STATIC ${USBSRCS})
