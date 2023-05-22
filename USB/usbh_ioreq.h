
#ifndef _USBH_IOREQ_H
#define _USBH_IOREQ_H

#include "usbh_conf.h"
#include "usbh_core.h"

#define USBH_PID_SETUP                            0U
#define USBH_PID_DATA                             1U

#define USBH_EP_CONTROL                           0U
#define USBH_EP_ISO                               1U
#define USBH_EP_BULK                              2U
#define USBH_EP_INTERRUPT                         3U

#define USBH_SETUP_PKT_SIZE                       8U

USBH_StatusTypeDef USBH_InterruptReceiveData(USBH_HandleTypeDef *phost,
                                             uint8_t *buff, uint8_t length, uint8_t pipe_num);
USBH_StatusTypeDef USBH_CtlSendSetup(USBH_HandleTypeDef *phost, uint8_t *buff, uint8_t pipe_num);
USBH_StatusTypeDef USBH_CtlReceiveData(USBH_HandleTypeDef *phost, uint8_t *buff,
                                       uint16_t length, uint8_t pipe_num);
USBH_StatusTypeDef USBH_CtlSendData(USBH_HandleTypeDef *phost, uint8_t *buff,
                                    uint16_t length, uint8_t pipe_num, uint8_t do_ping);
USBH_StatusTypeDef USBH_BulkReceiveData(USBH_HandleTypeDef *phost, uint8_t *buff,
                                        uint16_t length, uint8_t pipe_num);
USBH_StatusTypeDef USBH_BulkSendData(USBH_HandleTypeDef *phost, uint8_t *buff, uint16_t length,
                                     uint8_t pipe_num, uint8_t do_ping);
USBH_StatusTypeDef USBH_InterruptSendData(USBH_HandleTypeDef *phost, uint8_t *buff,
                                          uint8_t length, uint8_t pipe_num);
#endif
