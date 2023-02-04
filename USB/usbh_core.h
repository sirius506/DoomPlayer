#ifndef __USBH_CORE_H
#define __USBH_CORE_H

#include "usbh_def.h"

#define HOST_USER_SELECT_CONFIGURATION          0x01U
#define HOST_USER_CLASS_ACTIVE                  0x02U
#define HOST_USER_CLASS_SELECTED                0x03U
#define HOST_USER_CONNECTION                    0x04U
#define HOST_USER_DISCONNECTION                 0x05U
#define HOST_USER_UNRECOVERED_ERROR             0x06U

USBH_StatusTypeDef  USBH_Init(USBH_HandleTypeDef *phost, uint8_t id);

/* USBH Low Level Driver */
USBH_StatusTypeDef   USBH_LL_Init(USBH_HandleTypeDef *phost);
USBH_StatusTypeDef   USBH_LL_DeInit(USBH_HandleTypeDef *phost);
USBH_StatusTypeDef   USBH_LL_Start(USBH_HandleTypeDef *phost);
USBH_StatusTypeDef   USBH_LL_Stop(USBH_HandleTypeDef *phost);

USBH_StatusTypeDef   USBH_LL_Connect(USBH_HandleTypeDef *phost);
USBH_StatusTypeDef   USBH_LL_Disconnect(USBH_HandleTypeDef *phost);
USBH_SpeedTypeDef    USBH_LL_GetSpeed(USBH_HandleTypeDef *phost);
USBH_StatusTypeDef   USBH_LL_ResetPort(USBH_HandleTypeDef *phost);
uint32_t             USBH_LL_GetLastXferSize(USBH_HandleTypeDef *phost,
                                             uint8_t pipe);

USBH_StatusTypeDef   USBH_LL_DriverVBUS(USBH_HandleTypeDef *phost,
                                        uint8_t state);
USBH_StatusTypeDef   USBH_LL_OpenPipe(USBH_HandleTypeDef *phost,
                                      uint8_t pipe,
                                      uint8_t epnum,
                                      uint8_t dev_address,
                                      uint8_t speed,
                                      uint8_t ep_type,
                                      uint16_t mps);

USBH_StatusTypeDef   USBH_LL_ClosePipe(USBH_HandleTypeDef *phost,
                                       uint8_t pipe);
USBH_StatusTypeDef   USBH_LL_SubmitURB(USBH_HandleTypeDef *phost,
                                       uint8_t pipe,
                                       uint8_t direction,
                                       uint8_t ep_type,
                                       uint8_t token,
                                       uint8_t *pbuff,
                                       uint16_t length,
                                       uint8_t do_ping);

USBH_URBStateTypeDef USBH_LL_GetURBState(USBH_HandleTypeDef *phost,
                                         uint8_t pipe);

USBH_StatusTypeDef  USBH_LL_NotifyURBChange(USBH_HandleTypeDef *phost);

USBH_StatusTypeDef USBH_LL_SetToggle(USBH_HandleTypeDef *phost,
                                     uint8_t pipe, uint8_t toggle);

uint8_t USBH_LL_GetToggle(USBH_HandleTypeDef *phost, uint8_t pipe);

void                 USBH_LL_PortDisabled(USBH_HandleTypeDef *phost);
void                 USBH_LL_PortEnabled(USBH_HandleTypeDef *phost);

/* USBH Time base */
void USBH_LL_SetTimer(USBH_HandleTypeDef *phost, uint32_t time);
void USBH_LL_IncTimer(USBH_HandleTypeDef *phost);

extern void GrabUrb(USBH_HandleTypeDef *phost);
extern void UngrabUrb(USBH_HandleTypeDef *phost);

USBH_StatusTypeDef USBH_SelectInterface(USBH_HandleTypeDef *phost, uint8_t interface);

uint8_t  USBH_FindInterface(USBH_HandleTypeDef *phost, uint8_t Class, uint8_t SubClass, uint8_t Protocol);
uint8_t  USBH_FindInterfaceIndex(USBH_HandleTypeDef *phost, uint8_t interface_number, uint8_t alt_settings);

#endif
