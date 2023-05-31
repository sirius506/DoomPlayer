/**
  ******************************************************************************
  * @file    usbh_audio.c
  * @author  MCD Application Team
  * @brief   This file is the AC Layer Handlers for USB Host AC class.
  *
  * @verbatim
  *
  *          ===================================================================
  *                                AUDIO Class  Description
  *          ===================================================================
  *           This driver manages the Audio Class 1.0 following the "USB Device
  *           Class Definition for Audio Devices V1.0 Mar 18, 98".
  *
  *  @endverbatim
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                      www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "DoomPlayer.h"
#include "usbh_audio.h"
#include "usbh_ctlreq.h"
#include "usbh_pipes.h"
#include "board_if.h"

#include "mplayer.h"
#include "audio_output.h"

extern uint8_t  USBH_FindInterfaceIndex(USBH_HandleTypeDef *phost, uint8_t interface_number, uint8_t alt_settings);
extern uint8_t  USBH_FindInterface(USBH_HandleTypeDef *phost, uint8_t Class, uint8_t SubClass, uint8_t Protocol);
extern USBH_StatusTypeDef USBH_IsocSendData(USBH_HandleTypeDef *phost, uint8_t *buff, uint32_t length, uint8_t pipe_num);

#define	FLAG_TIMEOUT	osWaitForever

/** @addtogroup USBH_LIB
  * @{
  */

/** @addtogroup USBH_CLASS
  * @{
  */

/** @addtogroup USBH_AUDIO_CLASS
  * @{
  */

/** @defgroup USBH_AUDIO_CORE
  * @brief    This file includes HID Layer Handlers for USB Host HID class.
  * @{
  */

/** @defgroup USBH_AUDIO_CORE_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */


/** @defgroup USBH_AUDIO_CORE_Private_Defines
  * @{
  */
#define EVMSGQ_DEPTH     8
static uint8_t audioevmsgqBuffer[EVMSGQ_DEPTH * sizeof(PIPE_EVENT)];

MESSAGEQ_DEF(audioevmsgq, audioevmsgqBuffer, sizeof(audioevmsgqBuffer))
/**
  * @}
  */

extern void StartPlayerTask(void *arg);


/** @defgroup USBH_AUDIO_CORE_Private_FunctionPrototypes
  * @{
  */

static USBH_StatusTypeDef USBH_AUDIO_InterfaceInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_AUDIO_InterfaceDeInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_AUDIO_SOFProcess(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_AUDIO_FindAudioStreamingIN(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_AUDIO_FindAudioStreamingOUT(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_AUDIO_FindHIDControl(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_AUDIO_ParseCSDescriptors(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_AUDIO_BuildHeadphonePath(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
#ifdef USE_MIC
static USBH_StatusTypeDef USBH_AUDIO_BuildMicrophonePath(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
#endif

static USBH_StatusTypeDef ParseCSDescriptors(AUDIO_ClassSpecificDescTypedef *class_desc,
                                             uint8_t ac_subclass,
                                             uint8_t *pdesc);

static USBH_StatusTypeDef USBH_AC_SetCur(USBH_ClassTypeDef *phost,
                                         uint8_t subtype,
                                         uint8_t feature,
                                         uint8_t controlSelector,
                                         uint8_t channel,
                                         uint16_t length);

static USBH_StatusTypeDef USBH_AC_GetCur(USBH_ClassTypeDef *pclass,
                                         uint8_t subtype,
                                         uint8_t feature,
                                         uint8_t controlSelector,
                                         uint8_t channel,
                                         uint16_t length);

static USBH_StatusTypeDef USBH_AC_GetMin(USBH_ClassTypeDef *pclass,
                                         uint8_t subtype,
                                         uint8_t feature,
                                         uint8_t controlSelector,
                                         uint8_t channel,
                                         uint16_t length);

static USBH_StatusTypeDef USBH_AC_GetMax(USBH_ClassTypeDef *pclass,
                                         uint8_t subtype,
                                         uint8_t feature,
                                         uint8_t controlSelector,
                                         uint8_t channel,
                                         uint16_t length);

static USBH_StatusTypeDef USBH_AC_GetRes(USBH_ClassTypeDef *pclass,
                                         uint8_t subtype,
                                         uint8_t feature,
                                         uint8_t controlSelector,
                                         uint8_t channel,
                                         uint16_t length);

static USBH_StatusTypeDef USBH_AUDIO_SetEndpointControls(USBH_HandleTypeDef *phost,
                                                         uint8_t  Ep,
                                                         uint8_t *buff);

#if 0
static USBH_StatusTypeDef AUDIO_SetVolume(USBH_HandleTypeDef *phost, uint8_t feature, uint8_t channel, uint16_t volume);

static USBH_StatusTypeDef USBH_AUDIO_Control(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_AUDIO_SetControlAttribute(USBH_HandleTypeDef *phost, uint8_t attrib);
#endif
static uint32_t USBH_AUDIO_FindLinkedUnit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost, uint8_t UnitID);

SECTION_USBSRAM AUDIO_HandleTypeDef AudioHandleBuffer;

#define AUDIO_STACK_SIZE  800

TASK_DEF(audiotask, AUDIO_STACK_SIZE, osPriorityHigh2)

#define PLAYER_STACK_SIZE  5500

USBH_ClassTypeDef  AUDIO_Class =
{
  "AUDIO",
  AC_CLASS,
  USBH_AUDIO_InterfaceInit,
  USBH_AUDIO_InterfaceDeInit,
  NULL,
  NULL,
  USBH_AUDIO_SOFProcess,
  NULL,
};

AUDIO_PlayStateTypeDef get_audio_status(USBH_ClassTypeDef *pclass)
{
  AUDIO_HandleTypeDef *AUDIO_Handle;

  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;

  return AUDIO_Handle->play_state;
}

static osMessageQueueId_t  audioEventQueue;

static AUDIO_OUTPUT_DRIVER *usbDriver;

void StartAUDIOTask(void *arg)
{
  USBH_ClassTypeDef *pclass = (USBH_ClassTypeDef *)arg;
  USBH_HandleTypeDef *phost;
  AUDIO_HandleTypeDef *AUDIO_Handle;
  USBH_StatusTypeDef st;
  USBH_StatusTypeDef req_status = USBH_BUSY;
  int16_t VolumeCtl;
  int msec_frames;
  int max_msec;
  int offset_frames;
  uint16_t ResolutionCtl;
  uint8_t *buff;
  volatile int done_flag = 0;
  PIPE_EVENT pev;

  SEGGER_RTT_printf(0, "AUDIO task started\n");

  phost = pclass->phost;

  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;
//  AUDIO_Handle->state = AUDIO_INIT;


  if (AUDIO_Handle->headphone.supported == 1U)
  {
    GrabUrb(phost);
    phost->RequestState = CMD_SEND;

    req_status = USBH_SetInterface(phost,
                        AUDIO_Handle->headphone.interface,
                        0U);
    do
    {
      osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
      req_status = USBH_CtlReq(phost, NULL, 0);
    } while (req_status == USBH_BUSY);

    if (req_status == USBH_OK)
    {
          AUDIO_Handle->req_state = AUDIO_REQ_CS_REQUESTS;
          AUDIO_Handle->cs_req_state = AUDIO_REQ_GET_VOLUME;

          AUDIO_Handle->temp_feature  = AUDIO_Handle->headphone.asociated_feature;
          AUDIO_Handle->temp_channels = AUDIO_Handle->headphone.asociated_channels;
#if 0
        debug_printf("feature = %d, channel = %d\n", AUDIO_Handle->headphone.asociated_feature, AUDIO_Handle->headphone.asociated_channels);
#endif
    }
    else if (req_status == USBH_NOT_SUPPORTED)
    {
          USBH_ErrLog("Control error: AUDIO: Device Set interface request failed");
          st = USBH_FAIL;
    }
    else
    {
          /* .. */
    }
    UngrabUrb(phost);
  }
  else
  {
    AUDIO_Handle->req_state = AUDIO_REQ_CS_REQUESTS;
    AUDIO_Handle->cs_req_state = AUDIO_REQ_GET_VOLUME;
  }

  GrabUrb(phost);
  st = USBH_AC_GetCur(pclass,
             UAC_FEATURE_UNIT,     /* subtype  */
             AUDIO_Handle->temp_feature,              /* feature  */
             VOLUME_CONTROL,       /* Selector */
             0,                    /* channel  */
             0x02U);               /* length   */

  VolumeCtl = LE16(&(AUDIO_Handle->mem[0]));
  AUDIO_Handle->headphone.attribute.volume = (int32_t)VolumeCtl;

  st = USBH_AC_GetMin(pclass,
             UAC_FEATURE_UNIT,     /* subtype  */
             AUDIO_Handle->temp_feature,              /* feature  */
             VOLUME_CONTROL,       /* Selector */
             0,                    /* channel  */
             0x02U);               /* length   */

  VolumeCtl = LE16(&(AUDIO_Handle->mem[0]));
  AUDIO_Handle->headphone.attribute.volumeMin = (int32_t)VolumeCtl;

  st = USBH_AC_GetMax(pclass,
             UAC_FEATURE_UNIT,     /* subtype  */
             AUDIO_Handle->temp_feature,              /* feature  */
             VOLUME_CONTROL,       /* Selector */
             0,                    /* channel  */
             0x02U);               /* length   */

  VolumeCtl = LE16(&(AUDIO_Handle->mem[0]));
  AUDIO_Handle->headphone.attribute.volumeMax = (int32_t)VolumeCtl;

  st = USBH_AC_GetRes(pclass,
             UAC_FEATURE_UNIT,     /* subtype  */
             AUDIO_Handle->temp_feature,              /* feature  */
             VOLUME_CONTROL,       /* Selector */
             0,                    /* channel  */
             0x02U);               /* length   */

  ResolutionCtl = LE16(&(AUDIO_Handle->mem[0]));
  AUDIO_Handle->headphone.attribute.resolution = (uint32_t)ResolutionCtl;

#if 0
  debug_printf("Volume: Cur = %d, Min = %d, Max = %d, Res = %d\n",
          AUDIO_Handle->headphone.attribute.volume,
          AUDIO_Handle->headphone.attribute.volumeMin,
          AUDIO_Handle->headphone.attribute.volumeMax,
          AUDIO_Handle->headphone.attribute.resolution);
#endif

  /* Set new volume value */
  //st = USBH_AUDIO_HandleCSRequest(phost);
  VolumeCtl = AUDIO_Handle->headphone.attribute.volumeMax - ResolutionCtl * 20;
  AUDIO_Handle->mem[0] = VolumeCtl;
  st = USBH_AC_SetCur(pclass, UAC_FEATURE_UNIT, AUDIO_Handle->temp_feature,
                          VOLUME_CONTROL, 0, 2U);

  st = USBH_AC_GetCur(pclass,
             UAC_FEATURE_UNIT,     /* subtype  */
             AUDIO_Handle->temp_feature,              /* feature  */
             VOLUME_CONTROL,       /* Selector */
             0,                    /* channel  */
             0x02U);               /* length   */

  VolumeCtl = LE16(&(AUDIO_Handle->mem[0]));
#if 0
  debug_printf("newvol = %d (%d)\n", VolumeCtl, st);
#endif

  if (AUDIO_Handle->headphone.supported == 1U)
  {
    st = USBH_SetInterface(phost,
                   AUDIO_Handle->headphone.interface,
                   AUDIO_Handle->headphone.AltSettings);
    while (st == USBH_BUSY)
    {
      osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
      st = USBH_CtlReq(phost, NULL, 0U);
    }
#if 0
    debug_printf("SetInterface: %d, %d\n", 
                   AUDIO_Handle->headphone.interface,
                   AUDIO_Handle->headphone.AltSettings);
#endif
  }


  AUDIO_Handle->play_state = AUDIO_PLAYBACK_INIT;

  if (AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->bSamFreqType == 0U)
  {
    buff = (uint8_t *)AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->tSamFreq[0];

    st = USBH_AUDIO_SetEndpointControls(phost, AUDIO_Handle->headphone.Ep, buff);
  }
#if 0
  else
  {
    buff = (uint8_t *)(void *)&AUDIO_Handle->headphone.frequency;
    st = USBH_AUDIO_SetEndpointControls(phost, AUDIO_Handle->headphone.Ep, buff);
    if (st == USBH_OK)
    {
        AUDIO_Handle->play_state = AUDIO_PLAYBACK_IDLE;
        USBH_AUDIO_FrequencySet(phost);
    }
  }
#endif

  UngrabUrb(phost);

  AUDIO_Handle->play_state = AUDIO_PLAYBACK_IDLE;

  msec_frames = offset_frames = 0;

  /* Wait until user select USB as audio output */

  UngrabUrb(phost);
  do
  {
    osMessageQueueGet(pclass->classEventQueue, &pev, NULL, osWaitForever);
  }
  while (pev.state != EVF_START);

  {
      GrabUrb(phost);
      USBH_AUDIO_SetFrequency(pclass, AUDIO_Handle->sampleFreq, AUDIO_Handle->numChan,  16);
      msec_frames = AUDIO_Handle->headphone.frame_length;
      max_msec = NUM_FRAMES * 1000 / AUDIO_Handle->sampleFreq;
debug_printf("msec_frames = %d\n", msec_frames);
debug_printf("freq = %d --> %d\n", AUDIO_Handle->sampleFreq, max_msec);
      AUDIO_Handle->play_state = AUDIO_PLAYBACK_PLAY;
      pclass->cState = 2;
      UngrabUrb(phost);

      pclass->cState = 3;
      offset_frames = 0;
      done_flag = -1;
  }

  UngrabUrb(phost);

  done_flag = 1;

  while (1)
  {
    osMessageQueueGet(pclass->classEventQueue, &pev, NULL, osWaitForever);

    switch (pev.state)
    {
    case EVF_SOF:
      if ((AUDIO_Handle->play_state == AUDIO_PLAYBACK_PLAY) && done_flag)
      {

        SCB_CleanDCache();
        if (usbDriver->play_index >= max_msec)
        {
          usbDriver->play_index = 0;
        }
//debug_printf("play: %x, %d\n", usbDriver->playbuffer_ptr, usbDriver->play_index);
        done_flag = 0;
        (void)USBH_IsocSendData(phost,
                    usbDriver->playbuffer_ptr,
                    msec_frames,
                    AUDIO_Handle->headphone.Pipe);

      }
      else
      {
// debug_printf("SOF: %d, %d\n", AUDIO_Handle->play_state, done_flag);
      }
      break;
    case USBH_URB_DONE:
      if (pev.channel == AUDIO_Handle->headphone.Pipe)
      {
        done_flag = 1;

        usbDriver->play_index++;
        usbDriver->playbuffer_ptr += msec_frames;
        if (usbDriver->playbuffer_ptr >= usbDriver->sound_buffer + usbDriver->sound_buffer_size)
        {
          usbDriver->playbuffer_ptr = usbDriver->sound_buffer;
          mix_request_data(1);
        }
        else if (usbDriver->playbuffer_ptr == usbDriver->sound_buffer + usbDriver->sound_buffer_size/2)
        {
          mix_request_data(0);
        }
      }
      break;
    case EVF_VOLUME:
      GrabUrb(phost);
      VolumeCtl = AUDIO_Handle->headphone.attribute.volumeMax - ResolutionCtl * (100 - (pev.channel & 0xff)); 
      AUDIO_Handle->mem[0] = VolumeCtl; 
      st = USBH_AC_SetCur(pclass, UAC_FEATURE_UNIT, AUDIO_Handle->temp_feature,
                          VOLUME_CONTROL, 0, 2U);
      UngrabUrb(phost);
      break;
    case EVF_STOP:
      debug_printf("EVF_STOP\n");
      AUDIO_Handle->play_state = AUDIO_PLAYBACK_DRAIN;
      break;
    default:
      debug_printf("state = %d\n", pev.state);
      break;
    }
  }
}

void UsbAudio_Output_Start(AUDIO_CONF *audio_conf)
{
  PIPE_EVENT pev;

  usbDriver = audio_conf->pDriver;

  pev.channel = 0;
  pev.state = EVF_START;
  osMessageQueuePut(audioEventQueue, &pev, 0, 0);
}

void usbAudio_SetVolume(int vol)
{
  PIPE_EVENT pev;

  if (vol < 0 || vol > 100) vol = 75;

  pev.channel = vol;
  pev.state = EVF_VOLUME;
  osMessageQueuePut(audioEventQueue, &pev, 0, 0);
}

/**
  * @}
  */

/** @defgroup USBH_AUDIO_CORE_Private_Functions
  * @{
  */

/**
  * @brief  USBH_AUDIO_InterfaceInit
  *         The function init the Audio class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_InterfaceInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef out_status, in_status;
  AUDIO_HandleTypeDef *AUDIO_Handle;
  uint8_t  interface, index;
  uint16_t ep_size_out = 0U;
  uint16_t ep_size_in = 0U;

  interface = USBH_FindInterface(phost, AC_CLASS, USB_SUBCLASS_AUDIOCONTROL, 0x00U);

  if (interface == 0xFFU) /* Not Valid Interface */
  {
    USBH_DbgLog("Cannot Find the interface for %s class.", pclass->Name);
    return USBH_FAIL;
  }

  pclass->pData = &AudioHandleBuffer;
  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;

  if (AUDIO_Handle == NULL)
  {
    USBH_DbgLog("Cannot allocate memory for AUDIO Handle");
    return USBH_FAIL;
  }

  /* Initialize audio handler */
  (void)USBH_memset(AUDIO_Handle, 0, sizeof(AUDIO_HandleTypeDef));

  /* 1st Step:  Find Audio Interfaces */
  out_status = USBH_AUDIO_FindAudioStreamingIN(pclass, phost);

  in_status = USBH_AUDIO_FindAudioStreamingOUT(pclass, phost);

  if ((out_status == USBH_FAIL) && (in_status == USBH_FAIL))
  {
    USBH_DbgLog("%s class configuration not supported.", pclass->Name);
    return USBH_FAIL;
  }

  /* 2nd Step:  Select Audio Streaming Interfaces with largest endpoint size : default behavior */
  for (index = 0U; index < AUDIO_MAX_AUDIO_STD_INTERFACE; index ++)
  {
    if (AUDIO_Handle->stream_out[index].valid == 1U)
    {
      if (ep_size_out < AUDIO_Handle->stream_out[index].EpSize)
      {
        ep_size_out = AUDIO_Handle->stream_out[index].EpSize;
        AUDIO_Handle->headphone.interface = AUDIO_Handle->stream_out[index].interface;
        AUDIO_Handle->headphone.AltSettings = AUDIO_Handle->stream_out[index].AltSettings;
        AUDIO_Handle->headphone.Ep = AUDIO_Handle->stream_out[index].Ep;
        AUDIO_Handle->headphone.EpSize = AUDIO_Handle->stream_out[index].EpSize;
        AUDIO_Handle->headphone.Poll = (uint8_t)AUDIO_Handle->stream_out[index].Poll;
        AUDIO_Handle->headphone.supported = 1U;
      }
    }
debug_printf("HP Poll = %d\n", AUDIO_Handle->headphone.Poll);

    if (AUDIO_Handle->stream_in[index].valid == 1U)
    {
      if (ep_size_in < AUDIO_Handle->stream_in[index].EpSize)
      {
        ep_size_in = AUDIO_Handle->stream_in[index].EpSize;
        AUDIO_Handle->microphone.interface = AUDIO_Handle->stream_in[index].interface;
        AUDIO_Handle->microphone.AltSettings = AUDIO_Handle->stream_in[index].AltSettings;
        AUDIO_Handle->microphone.Ep = AUDIO_Handle->stream_in[index].Ep;
        AUDIO_Handle->microphone.EpSize = AUDIO_Handle->stream_in[index].EpSize;
        AUDIO_Handle->microphone.Poll = (uint8_t)AUDIO_Handle->stream_out[index].Poll;
        AUDIO_Handle->microphone.supported = 1U;
      }
    }
  }

  if (USBH_AUDIO_FindHIDControl(pclass, phost) == USBH_OK)
  {
    AUDIO_Handle->control.supported = 1U;
  }

  /* 3rd Step:  Find and Parse Audio interfaces */
  (void)USBH_AUDIO_ParseCSDescriptors(pclass, phost);

  if (AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->bSamFreqType != 0U)
  {
    uint8_t num_supported_freq;
    int nchan, SampleRate, index;

    num_supported_freq = (AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->bLength - 8U) / 3U;

    nchan = AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->bNrChannels;
    for (index = 0U; index < num_supported_freq; index++)
    {
      SampleRate = LE24(
                AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->tSamFreq[index]);
      debug_printf("%d: %d, %d\n", index, nchan, SampleRate);
      if (index == 0)
      {
         AUDIO_Handle->sampleFreq = SampleRate;
         AUDIO_Handle->numChan = nchan;
      }
    }
  }

  /* 4th Step:  Open the Audio streaming pipes*/
  if (AUDIO_Handle->headphone.supported == 1U)
  {
    (void)USBH_AUDIO_BuildHeadphonePath(pclass, phost);

    AUDIO_Handle->headphone.Pipe = USBH_AllocPipe(phost, AUDIO_Handle->headphone.Ep);
#if 0
debug_printf("HP pipe = %d\n", AUDIO_Handle->headphone.Pipe);
#endif

    /* Open pipe for IN endpoint */
    (void)USBH_OpenPipe(phost,
                        AUDIO_Handle->headphone.Pipe,
                        AUDIO_Handle->headphone.Ep,
                        phost->device.address,
                        phost->device.speed,
                        USB_EP_TYPE_ISOC,
                        AUDIO_Handle->headphone.EpSize);

    (void)USBH_LL_SetToggle(phost,  AUDIO_Handle->headphone.Pipe, 0U);
  }

#ifdef USE_MIC_SUPPORT
  if (AUDIO_Handle->microphone.supported == 1U)
  {
    (void)USBH_AUDIO_BuildMicrophonePath(pclass, phost);
    AUDIO_Handle->microphone.Pipe = USBH_AllocPipe(phost, AUDIO_Handle->microphone.Ep);

    /* Open pipe for IN endpoint */
    (void)USBH_OpenPipe(phost,
                        AUDIO_Handle->microphone.Pipe,
                        AUDIO_Handle->microphone.Ep,
                        phost->device.address,
                        phost->device.speed,
                        USB_EP_TYPE_ISOC,
                        AUDIO_Handle->microphone.EpSize);

    (void)USBH_LL_SetToggle(phost,  AUDIO_Handle->microphone.Pipe, 0U);
  }
#endif

  if (AUDIO_Handle->control.supported == 1U)
  {
    AUDIO_Handle->control.Pipe  = USBH_AllocPipe(phost, AUDIO_Handle->control.Ep);

    /* Open pipe for IN endpoint */
    (void)USBH_OpenPipe(phost,
                        AUDIO_Handle->control.Pipe,
                        AUDIO_Handle->control.Ep,
                        phost->device.address,
                        phost->device.speed,
                        USB_EP_TYPE_INTR,
                        AUDIO_Handle->control.EpSize);

    (void)USBH_LL_SetToggle(phost,  AUDIO_Handle->control.Pipe, 0U);

  }
  debug_printf("Pipe: control = %d, headphone = %d\n", AUDIO_Handle->control.Pipe, AUDIO_Handle->headphone.Pipe);

  AUDIO_Handle->req_state = AUDIO_REQ_INIT;
  AUDIO_Handle->control_state = AUDIO_CONTROL_INIT;

  pclass->cState = 1;
  pclass->phost = phost;

  audioEventQueue = pclass->classEventQueue = osMessageQueueNew(EVMSGQ_DEPTH, sizeof(PIPE_EVENT), &attributes_audioevmsgq);
  pclass->phost->PipeEvq[AUDIO_Handle->control.Pipe & 0x0F] = pclass->classEventQueue;
  pclass->phost->PipeEvq[AUDIO_Handle->headphone.Pipe & 0x0F] = pclass->classEventQueue;

  osThreadNew(StartAUDIOTask, pclass, &attributes_audiotask);

  return USBH_OK;
}


/**
  * @brief  USBH_AUDIO_InterfaceDeInit
  *         The function DeInit the Pipes used for the Audio class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_InterfaceDeInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  AUDIO_HandleTypeDef *AUDIO_Handle = (AUDIO_HandleTypeDef *) phost->pActiveClassList->pData;

  if (AUDIO_Handle->microphone.Pipe != 0x00U)
  {
    (void)USBH_ClosePipe(phost, AUDIO_Handle->microphone.Pipe);
    (void)USBH_FreePipe(phost, AUDIO_Handle->microphone.Pipe);
    AUDIO_Handle->microphone.Pipe = 0U;     /* Reset the pipe as Free */
  }

  if (AUDIO_Handle->headphone.Pipe != 0x00U)
  {
    (void)USBH_ClosePipe(phost,  AUDIO_Handle->headphone.Pipe);
    (void)USBH_FreePipe(phost,  AUDIO_Handle->headphone.Pipe);
    AUDIO_Handle->headphone.Pipe = 0U;     /* Reset the pipe as Free */
  }

  if (AUDIO_Handle->control.Pipe != 0x00U)
  {
    (void)USBH_ClosePipe(phost,  AUDIO_Handle->control.Pipe);
    (void)USBH_FreePipe(phost,  AUDIO_Handle->control.Pipe);
    AUDIO_Handle->control.Pipe = 0U;     /* Reset the pipe as Free */
  }

  if ((phost->pActiveClassList->pData) != 0U)
  {
    USBH_free(phost->pActiveClassList->pData);
    phost->pActiveClassList->pData = 0U;
  }
  return USBH_OK ;
}

/**
  * @brief  USBH_AUDIO_Process
  *         The function is for managing state machine for Audio data transfers
  * @param  phost: Host handle
  * @retval USBH Status
  */
#if 0
static USBH_StatusTypeDef USBH_AUDIO_Process(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status = USBH_BUSY;
  AUDIO_HandleTypeDef *AUDIO_Handle = (AUDIO_HandleTypeDef *)  phost->pActiveClassList->pData;

  if (AUDIO_Handle->headphone.supported == 1U)
  {
    //(void)USBH_AUDIO_OutputStream(phost);
  }

  if (AUDIO_Handle->microphone.supported == 1U)
  {
    (void)USBH_AUDIO_InputStream(phost);
  }

  return status;
}
#endif

/**
  * @brief  USBH_AUDIO_SOFProcess
  *         The function is for managing the SOF callback
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_SOFProcess(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
    PIPE_EVENT pev;

    switch (pclass->cState)
    {
    case 2:
      pclass->cState++;
      break;
    case 3:
      pev.channel = 0;
      pev.state = EVF_SOF;
      osMessageQueuePut(pclass->classEventQueue, &pev, 0, 0);
      break;
    default:
      break;
    }
  return USBH_OK;
}
/**
  * @brief  Find IN Audio Streaming interfaces
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_FindAudioStreamingIN(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  uint8_t interface, alt_settings;
  USBH_StatusTypeDef status = USBH_FAIL ;
  AUDIO_HandleTypeDef *AUDIO_Handle;

  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;

  /* Look For AUDIOSTREAMING IN interface */
  alt_settings = 0U;
  for (interface = 0U; interface < USBH_MAX_NUM_IFDESC; interface++)
  {
    if ((phost->device.CfgDesc.Itf_Desc[interface].bInterfaceClass == AC_CLASS) &&
        (phost->device.CfgDesc.Itf_Desc[interface].bInterfaceSubClass == USB_SUBCLASS_AUDIOSTREAMING))
    {
      if (((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 0x80U) != 0U) &&
          (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize > 0U))
      {
        AUDIO_Handle->stream_in[alt_settings].Ep = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
        AUDIO_Handle->stream_in[alt_settings].EpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
        AUDIO_Handle->stream_in[alt_settings].interface = phost->device.CfgDesc.Itf_Desc[interface].bInterfaceNumber;
        AUDIO_Handle->stream_in[alt_settings].AltSettings = phost->device.CfgDesc.Itf_Desc[interface].bAlternateSetting;
        AUDIO_Handle->stream_in[alt_settings].Poll = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bInterval;
        AUDIO_Handle->stream_in[alt_settings].valid = 1U;
        alt_settings++;
      }
    }
  }

  if (alt_settings > 0U)
  {
    status = USBH_OK;
  }

  return status;
}

/**
  * @brief  Find OUT Audio Streaming interfaces
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_FindAudioStreamingOUT(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  uint8_t interface, alt_settings;
  USBH_StatusTypeDef status = USBH_FAIL ;
  AUDIO_HandleTypeDef *AUDIO_Handle;

  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;

  /* Look For AUDIOSTREAMING IN interface */
  alt_settings = 0U;
  for (interface = 0U; interface < USBH_MAX_NUM_IFDESC; interface++)
  {
    if ((phost->device.CfgDesc.Itf_Desc[interface].bInterfaceClass == AC_CLASS) &&
        (phost->device.CfgDesc.Itf_Desc[interface].bInterfaceSubClass == USB_SUBCLASS_AUDIOSTREAMING))
    {
      if (((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 0x80U) == 0x00U) &&
          (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize > 0U))
      {
        AUDIO_Handle->stream_out[alt_settings].Ep = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
        AUDIO_Handle->stream_out[alt_settings].EpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
        AUDIO_Handle->stream_out[alt_settings].interface = phost->device.CfgDesc.Itf_Desc[interface].bInterfaceNumber;
        AUDIO_Handle->stream_out[alt_settings].AltSettings = phost->device.CfgDesc.Itf_Desc[interface].bAlternateSetting;
        AUDIO_Handle->stream_out[alt_settings].Poll = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bInterval;
        AUDIO_Handle->stream_out[alt_settings].valid = 1U;
        alt_settings++;
      }
    }
  }

  if (alt_settings > 0U)
  {
    status = USBH_OK;
  }

  return status;
}

/**
  * @brief  Find HID Control interfaces
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_FindHIDControl(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  uint8_t interface;
  USBH_StatusTypeDef status = USBH_FAIL;
  AUDIO_HandleTypeDef *AUDIO_Handle;

  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;

  /* Look For AUDIOCONTROL  interface */
  interface = USBH_FindInterface(phost, AC_CLASS, USB_SUBCLASS_AUDIOCONTROL, 0xFFU);
  if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_INTERFACES))
  {
    return USBH_FAIL;
  }

  for (interface = 0U; interface < USBH_MAX_NUM_IFDESC; interface++)
  {
    if ((phost->device.CfgDesc.Itf_Desc[interface].bInterfaceClass == 0x03U) && /*HID*/
        (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize > 0U))
    {
      if ((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 0x80U) == 0x80U)
      {
        AUDIO_Handle->control.Ep = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
        AUDIO_Handle->control.EpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
        AUDIO_Handle->control.interface = phost->device.CfgDesc.Itf_Desc[interface].bInterfaceNumber;
        AUDIO_Handle->control.Poll = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bInterval;
        AUDIO_Handle->control.supported = 1U;
        status = USBH_OK;
        break;
      }
    }
  }

  return status;
}

/**
  * @brief  Parse AC and interfaces Descriptors
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_ParseCSDescriptors(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status = USBH_OK;
  USBH_DescHeader_t            *pdesc;
  uint16_t                      ptr;
  uint8_t                       itf_index = 0U;
  uint8_t                       itf_number = 0U;
  uint8_t                       alt_setting;
  AUDIO_HandleTypeDef           *AUDIO_Handle;

  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;
  pdesc   = (USBH_DescHeader_t *)(void *)(phost->device.CfgDesc_Raw);
  ptr = USB_LEN_CFG_DESC;

  AUDIO_Handle->class_desc.FeatureUnitNum = 0U;
  AUDIO_Handle->class_desc.InputTerminalNum = 0U;
  AUDIO_Handle->class_desc.OutputTerminalNum = 0U;
  AUDIO_Handle->class_desc.ASNum = 0U;

  while (ptr < phost->device.CfgDesc.wTotalLength)
  {
    pdesc = USBH_GetNextDesc((uint8_t *)(void *)pdesc, &ptr);

    switch (pdesc->bDescriptorType)
    {

      case USB_DESC_TYPE_INTERFACE:
        itf_number = *((uint8_t *)(void *)pdesc + 2U);
        alt_setting = *((uint8_t *)(void *)pdesc + 3U);
        itf_index = USBH_FindInterfaceIndex(phost, itf_number, alt_setting);
        break;

      case USB_DESC_TYPE_CS_INTERFACE:
        if (itf_number <= phost->device.CfgDesc.bNumInterfaces)
        {
          if ((itf_index == 0xFFU) || (itf_index >= USBH_MAX_NUM_INTERFACES)) /* No Valid Interface */
          {
            USBH_DbgLog("Cannot Find the audio interface index for %s class.", phost->pActiveClassList->Name);
            status = USBH_FAIL;
          }
          else
          {

            (void)ParseCSDescriptors(&AUDIO_Handle->class_desc,
                                     phost->device.CfgDesc.Itf_Desc[itf_index].bInterfaceSubClass,
                                     (uint8_t *)pdesc);
          }
        }
        break;

      default:
        break;
    }
  }

  return status;
}

/**
  * @brief  Parse AC interfaces
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef ParseCSDescriptors(AUDIO_ClassSpecificDescTypedef *class_desc,
                                             uint8_t ac_subclass,
                                             uint8_t *pdesc)
{
  if (ac_subclass == USB_SUBCLASS_AUDIOCONTROL)
  {
    switch (pdesc[2])
    {
      case UAC_HEADER:
        class_desc->cs_desc.HeaderDesc = (AUDIO_HeaderDescTypeDef *)(void *)pdesc;
        break;

      case UAC_INPUT_TERMINAL:
        class_desc->cs_desc.InputTerminalDesc[class_desc->InputTerminalNum++] = (AUDIO_ITDescTypeDef *)(void *)pdesc;
        break;

      case UAC_OUTPUT_TERMINAL:
        class_desc->cs_desc.OutputTerminalDesc[class_desc->OutputTerminalNum++] = (AUDIO_OTDescTypeDef *)(void *)pdesc;
        break;

      case UAC_FEATURE_UNIT:
        class_desc->cs_desc.FeatureUnitDesc[class_desc->FeatureUnitNum++] = (AUDIO_FeatureDescTypeDef *)(void *)pdesc;
        break;

      case UAC_SELECTOR_UNIT:
        class_desc->cs_desc.SelectorUnitDesc[class_desc->SelectorUnitNum++] = (AUDIO_SelectorDescTypeDef *)(void *)pdesc;
        break;

      case UAC_MIXER_UNIT:
        class_desc->cs_desc.MixerUnitDesc[class_desc->MixerUnitNum++] = (AUDIO_MixerDescTypeDef *)(void *)pdesc;
        break;

      default:
        break;
    }
  }
  else
  {
    if (ac_subclass == USB_SUBCLASS_AUDIOSTREAMING)
    {
      switch (pdesc[2])
      {
        case UAC_AS_GENERAL:
          class_desc->as_desc[class_desc->ASNum].GeneralDesc = (AUDIO_ASGeneralDescTypeDef *)(void *)pdesc;
          break;
        case UAC_FORMAT_TYPE:
          class_desc->as_desc[class_desc->ASNum++].FormatTypeDesc = (AUDIO_ASFormatTypeDescTypeDef *)(void *)pdesc;
          break;
        default:
          break;
      }
    }
  }

  return USBH_OK;
}


/**
  * @brief  Link a Unit to next associated one
  * @param  phost: Host handle
  * @param  UnitID: Unit identifier
  * @retval UnitID, Index and Type of the associated Unit
  */
static uint32_t USBH_AUDIO_FindLinkedUnit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost, uint8_t UnitID)
{
  uint8_t Index;
  AUDIO_HandleTypeDef *AUDIO_Handle;

  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;

  /* Find Feature Unit */
  for (Index = 0U; Index < AUDIO_Handle->class_desc.FeatureUnitNum; Index ++)
  {
    if (AUDIO_Handle->class_desc.cs_desc.FeatureUnitDesc[Index]->bSourceID == UnitID)
    {
      UnitID = AUDIO_Handle->class_desc.cs_desc.FeatureUnitDesc[Index]->bUnitID;

      return (((uint32_t)UnitID << 16U) | (UAC_FEATURE_UNIT << 8U) | (uint32_t)Index);
    }
  }

  /* Find Mixer Unit */
  for (Index = 0U; Index < AUDIO_Handle->class_desc.MixerUnitNum; Index ++)
  {
    if ((AUDIO_Handle->class_desc.cs_desc.MixerUnitDesc[Index]->bSourceID0 == UnitID) ||
        (AUDIO_Handle->class_desc.cs_desc.MixerUnitDesc[Index]->bSourceID1 == UnitID))
    {
      UnitID = AUDIO_Handle->class_desc.cs_desc.MixerUnitDesc[Index]->bUnitID;

      return (uint32_t)((UnitID << 16U) | (UAC_MIXER_UNIT << 8U) | Index);
    }
  }

  /* Find Selector Unit */
  for (Index = 0U; Index < AUDIO_Handle->class_desc.SelectorUnitNum; Index ++)
  {
    if (AUDIO_Handle->class_desc.cs_desc.SelectorUnitDesc[Index]->bSourceID0 == UnitID)
    {
      UnitID = AUDIO_Handle->class_desc.cs_desc.SelectorUnitDesc[Index]->bUnitID;

      return (uint32_t)((UnitID << 16U) | (UAC_SELECTOR_UNIT << 8U) | Index);
    }
  }

  /* Find Output Terminal Unit */
  for (Index = 0U; Index < AUDIO_Handle->class_desc.OutputTerminalNum; Index ++)
  {
    if (AUDIO_Handle->class_desc.cs_desc.OutputTerminalDesc[Index]->bSourceID == UnitID)
    {
      UnitID = AUDIO_Handle->class_desc.cs_desc.OutputTerminalDesc[Index]->bTerminalID;

      return (uint32_t)((UnitID << 16U) | (UAC_OUTPUT_TERMINAL << 8U) | Index);
    }
  }

  /* No associated Unit found return undefined ID 0x00*/
  return 0U;
}

#ifdef USE_MIC
/**
  * @brief  Build full path for Microphone device
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_BuildMicrophonePath(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  uint8_t UnitID = 0U, Type, Index;
  uint32_t value;
  uint8_t terminalIndex;
  AUDIO_HandleTypeDef *AUDIO_Handle;
  USBH_StatusTypeDef ret = USBH_OK;

  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;

  /*Find microphone IT*/
  for (terminalIndex = 0U; terminalIndex < AUDIO_Handle->class_desc.InputTerminalNum; terminalIndex++)
  {
    if (LE16(AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[terminalIndex]->wTerminalType) == 0x201U)
    {
      UnitID = AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[terminalIndex]->bTerminalID;
      AUDIO_Handle->microphone.asociated_channels =  AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[terminalIndex]->bNrChannels;
      break;
    }
  }

  do
  {
    value = USBH_AUDIO_FindLinkedUnit(pclass, phost, UnitID);

    if (value == 0U)
    {
      return USBH_FAIL;
    }

    Index = (uint8_t)(value & 0xFFU);
    Type = (uint8_t)((value >> 8U) & 0xFFU);
    UnitID = (uint8_t)((value >> 16U) & 0xFFU);

    switch (Type)
    {
      case UAC_FEATURE_UNIT:
        AUDIO_Handle->microphone.asociated_feature = Index;
        break;

      case UAC_MIXER_UNIT:
        AUDIO_Handle->microphone.asociated_mixer = Index;
        break;

      case UAC_SELECTOR_UNIT:
        AUDIO_Handle->microphone.asociated_selector = Index;
        break;

      case UAC_OUTPUT_TERMINAL:
        AUDIO_Handle->microphone.asociated_terminal = Index;
        break;

      default:
        ret = USBH_FAIL;
        break;
    }
  } while ((Type != UAC_OUTPUT_TERMINAL) && (value > 0U));

  return ret;
}
#endif

/**
  * @brief  Build full path for Headphone device
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_BuildHeadphonePath(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  uint8_t UnitID = 0U, Type, Index;
  uint32_t value;
  uint8_t terminalIndex;
  AUDIO_HandleTypeDef *AUDIO_Handle;
  USBH_StatusTypeDef ret = USBH_OK;

  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;

  /* Find association between audio streaming and microphone */
  for (terminalIndex = 0U; terminalIndex < AUDIO_Handle->class_desc.InputTerminalNum; terminalIndex++)
  {
    if (LE16(AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[terminalIndex]->wTerminalType) == 0x101U)
    {
      UnitID = AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[terminalIndex]->bTerminalID;
      AUDIO_Handle->headphone.asociated_channels =  AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[terminalIndex]->bNrChannels;
      break;
    }
  }

  for (Index = 0U; Index < AUDIO_Handle->class_desc.ASNum; Index++)
  {
    if (AUDIO_Handle->class_desc.as_desc[Index].GeneralDesc->bTerminalLink == UnitID)
    {
      AUDIO_Handle->headphone.asociated_as = Index;
      break;
    }
  }

  do
  {
    value = USBH_AUDIO_FindLinkedUnit(pclass, phost, UnitID);

    if (value == 0U)
    {
      return USBH_FAIL;
    }

    Index = (uint8_t)(value & 0xFFU);
    Type = (uint8_t)((value >> 8U) & 0xFFU);
    UnitID = (uint8_t)((value >> 16U) & 0xFFU);

    switch (Type)
    {
      case UAC_FEATURE_UNIT:
        AUDIO_Handle->headphone.asociated_feature = Index;
        break;

      case UAC_MIXER_UNIT:
        AUDIO_Handle->headphone.asociated_mixer = Index;
        break;

      case UAC_SELECTOR_UNIT:
        AUDIO_Handle->headphone.asociated_selector = Index;
        break;

      case UAC_OUTPUT_TERMINAL:
        AUDIO_Handle->headphone.asociated_terminal = Index;
        if (Index < AUDIO_MAX_NUM_OUT_TERMINAL)
        {
          if (LE16(AUDIO_Handle->class_desc.cs_desc.OutputTerminalDesc[Index]->wTerminalType) != 0x103U)
          {
            return  USBH_OK;
          }
        }
        else
        {
          ret = USBH_FAIL;
        }
        break;

      default:
        ret = USBH_FAIL;
        break;
    }
  } while ((Type != UAC_OUTPUT_TERMINAL) && (value > 0U));

  return ret;
}


/**
  * @brief  Handle Set Cur request
  * @param  phost: Host handle
  * @param  subtype: subtype index
  * @param  feature: feature index
  * @param  controlSelector: control code
  * @param  channel: channel index
  * @param  length: Command length
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AC_SetCur(USBH_ClassTypeDef *pclass,
                                         uint8_t subtype,
                                         uint8_t feature,
                                         uint8_t controlSelector,
                                         uint8_t channel,
                                         uint16_t length)
{
  uint16_t wValue = 0U, wIndex = 0U, wLength = 0U;
  uint8_t UnitID, InterfaceNum;
  AUDIO_HandleTypeDef *AUDIO_Handle;
  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;
  USBH_StatusTypeDef ret = USBH_OK;
  USBH_HandleTypeDef *phost = pclass->phost;

  switch (subtype)
  {
    case UAC_INPUT_TERMINAL:
      UnitID = AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[0]->bTerminalID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      wValue = (COPY_PROTECT_CONTROL << 8U);
      AUDIO_Handle->mem[0] = 0x00U;

      wLength = 1U;
      break;
    case UAC_FEATURE_UNIT:
      UnitID = AUDIO_Handle->class_desc.cs_desc.FeatureUnitDesc[feature]->bUnitID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum ;
      /*holds the CS(control selector ) and CN (channel number)*/
      wValue = (uint16_t)((uint32_t)controlSelector << 8U) | (uint16_t)channel;
      wLength = length;
      break;

    default:
      ret = USBH_FAIL;
      break;
  }

  if (ret != USBH_OK)
  {
    return ret;
  }

  phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE
                                         | USB_REQ_TYPE_CLASS;

  phost->Control.setup.b.bRequest = UAC_SET_CUR;
  phost->Control.setup.b.wValue.w = wValue;
  phost->Control.setup.b.wIndex.w = wIndex;
  phost->Control.setup.b.wLength.w = wLength;

  ret = USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength);
  while (ret == USBH_BUSY)
  {
    osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
    ret = USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength);
  }
  return ret;
}

/**
  * @brief  Handle Get Cur request
  * @param  phost: Host handle
  * @param  subtype: subtype index
  * @param  feature: feature index
  * @param  controlSelector: control code
  * @param  channel: channel index
  * @param  length: Command length
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AC_GetCur(USBH_ClassTypeDef *pclass,
                                         uint8_t subtype,
                                         uint8_t feature,
                                         uint8_t controlSelector,
                                         uint8_t channel,
                                         uint16_t length)
{
  uint16_t wValue = 0U, wIndex = 0U, wLength = 0U;
  uint8_t UnitID = 0U, InterfaceNum = 0U;
  AUDIO_HandleTypeDef *AUDIO_Handle;
  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;
  USBH_StatusTypeDef ret = USBH_OK;
  USBH_HandleTypeDef *phost = pclass->phost;

  switch (subtype)
  {
    case UAC_INPUT_TERMINAL:
      UnitID = AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[0]->bTerminalID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      wValue = (COPY_PROTECT_CONTROL << 8U);
      AUDIO_Handle->mem[0] = 0x00U;

      wLength = 1U;
      break;
    case UAC_FEATURE_UNIT:
      UnitID = AUDIO_Handle->class_desc.cs_desc.FeatureUnitDesc[feature]->bUnitID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      /*holds the CS(control selector ) and CN (channel number)*/
      wValue = (uint16_t)((uint32_t)controlSelector << 8U) | (uint16_t)channel;
      wLength = length;
      break;

    case UAC_OUTPUT_TERMINAL:
      UnitID = AUDIO_Handle->class_desc.cs_desc.OutputTerminalDesc[0]->bTerminalID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      wValue = (COPY_PROTECT_CONTROL << 8U);
      wLength = 1U;
      break;

    default:
      ret = USBH_FAIL;
      break;
  }

  if (ret != USBH_OK)
  {
    return ret;
  }

  phost->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_INTERFACE | \
                                         USB_REQ_TYPE_CLASS;

  phost->Control.setup.b.bRequest = UAC_GET_CUR;
  phost->Control.setup.b.wValue.w = wValue;
  phost->Control.setup.b.wIndex.w = wIndex;
  phost->Control.setup.b.wLength.w = wLength;

  ret = USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength);
  while (ret == USBH_BUSY)
  {
    osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
    ret = USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength);
  }
  SCB_InvalidateDCache_by_Addr((void *)AUDIO_Handle->mem, length);
  return ret;
}


/**
  * @brief  Handle Get Max request
  * @param  phost: Host handle
  * @param  subtype: subtype index
  * @param  feature: feature index
  * @param  controlSelector: control code
  * @param  channel: channel index
  * @param  length: Command length
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AC_GetMax(USBH_ClassTypeDef *pclass,
                                         uint8_t subtype,
                                         uint8_t feature,
                                         uint8_t controlSelector,
                                         uint8_t channel,
                                         uint16_t length)
{
  uint16_t wValue = 0U, wIndex = 0U, wLength = 0U;
  uint8_t UnitID = 0U, InterfaceNum = 0U;
  AUDIO_HandleTypeDef *AUDIO_Handle;
  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;
  USBH_StatusTypeDef ret = USBH_OK;
  USBH_HandleTypeDef *phost = pclass->phost;

  switch (subtype)
  {
    case UAC_INPUT_TERMINAL:
      UnitID = AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[0]->bTerminalID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      wValue = (COPY_PROTECT_CONTROL << 8U);
      AUDIO_Handle->mem[0] = 0x00U;

      wLength = 1U;
      break;
    case UAC_FEATURE_UNIT:
      UnitID = AUDIO_Handle->class_desc.cs_desc.FeatureUnitDesc[feature]->bUnitID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum ;
      /*holds the CS(control selector ) and CN (channel number)*/
      wValue = (uint16_t)((uint32_t)controlSelector << 8U) | (uint16_t)channel;
      wLength = length;
      break;

    case UAC_OUTPUT_TERMINAL:
      UnitID = AUDIO_Handle->class_desc.cs_desc.OutputTerminalDesc[0]->bTerminalID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum ;
      wValue = (COPY_PROTECT_CONTROL << 8U) ;
      wLength = 1U;
      break;

    default:
      ret = USBH_FAIL;
      break;
  }

  if (ret != USBH_OK)
  {
    return ret;
  }

  phost->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_INTERFACE | \
                                         USB_REQ_TYPE_CLASS;

  phost->Control.setup.b.bRequest = UAC_GET_MAX;
  phost->Control.setup.b.wValue.w = wValue;
  phost->Control.setup.b.wIndex.w = wIndex;
  phost->Control.setup.b.wLength.w = wLength;

  //return (USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength));
  ret = USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength);
  while (ret == USBH_BUSY)
  {
    osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
    ret = USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength);
  }
  SCB_InvalidateDCache_by_Addr((void *)AUDIO_Handle->mem, length);
  return ret;

}

/**
  * @brief  Handle Get Res request
  * @param  phost: Host handle
  * @param  subtype: subtype index
  * @param  feature: feature index
  * @param  controlSelector: control code
  * @param  channel: channel index
  * @param  length: Command length
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AC_GetRes(USBH_ClassTypeDef *pclass,
                                         uint8_t subtype,
                                         uint8_t feature,
                                         uint8_t controlSelector,
                                         uint8_t channel,
                                         uint16_t length)
{
  uint16_t wValue = 0U, wIndex = 0U, wLength = 0U;
  uint8_t UnitID = 0U, InterfaceNum = 0U;
  AUDIO_HandleTypeDef *AUDIO_Handle;
  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;
  USBH_StatusTypeDef ret = USBH_OK;
  USBH_HandleTypeDef *phost = pclass->phost;

  switch (subtype)
  {
    case UAC_INPUT_TERMINAL:
      UnitID = AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[0]->bTerminalID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      wValue = (COPY_PROTECT_CONTROL << 8U) ;
      AUDIO_Handle->mem[0] = 0x00U;

      wLength = 1U;
      break;
    case UAC_FEATURE_UNIT:
      UnitID = AUDIO_Handle->class_desc.cs_desc.FeatureUnitDesc[feature]->bUnitID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      /*holds the CS(control selector ) and CN (channel number)*/
      wValue = (uint16_t)((uint32_t)controlSelector << 8U) | (uint16_t)channel;
      wLength = length;
      break;

    case UAC_OUTPUT_TERMINAL:
      UnitID = AUDIO_Handle->class_desc.cs_desc.OutputTerminalDesc[0]->bTerminalID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      wValue = (COPY_PROTECT_CONTROL << 8U) ;
      wLength = 1U;
      break;

    default:
      ret = USBH_FAIL;
      break;
  }

  if (ret != USBH_OK)
  {
    return ret;
  }

  phost->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_INTERFACE
                                         | USB_REQ_TYPE_CLASS;

  phost->Control.setup.b.bRequest = UAC_GET_RES;
  phost->Control.setup.b.wValue.w = wValue;
  phost->Control.setup.b.wIndex.w = wIndex;
  phost->Control.setup.b.wLength.w = wLength;

  //return (USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength));
  ret = USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength);
  while (ret == USBH_BUSY)
  {
    osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
    ret = USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength);
  }
  SCB_InvalidateDCache_by_Addr((void *)AUDIO_Handle->mem, length);
  return ret;

}

/**
  * @brief  Handle Get Min request
  * @param  phost: Host handle
  * @param  subtype: subtype index
  * @param  feature: feature index
  * @param  controlSelector: control code
  * @param  channel: channel index
  * @param  length: Command length
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AC_GetMin(USBH_ClassTypeDef *pclass,
                                         uint8_t subtype,
                                         uint8_t feature,
                                         uint8_t controlSelector,
                                         uint8_t channel,
                                         uint16_t length)
{
  uint16_t wValue = 0U, wIndex = 0U, wLength = 0U;
  uint8_t UnitID = 0U, InterfaceNum = 0U;
  AUDIO_HandleTypeDef *AUDIO_Handle;
  AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;
  USBH_StatusTypeDef ret = USBH_OK;
  USBH_HandleTypeDef *phost = pclass->phost;

  switch (subtype)
  {
    case UAC_INPUT_TERMINAL:
      UnitID = AUDIO_Handle->class_desc.cs_desc.InputTerminalDesc[0]->bTerminalID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      wValue = (COPY_PROTECT_CONTROL << 8U);
      AUDIO_Handle->mem[0] = 0x00U;

      wLength = 1U;
      break;
    case UAC_FEATURE_UNIT:
      UnitID = AUDIO_Handle->class_desc.cs_desc.FeatureUnitDesc[feature]->bUnitID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      /*holds the CS(control selector ) and CN (channel number)*/
      wValue = (uint16_t)((uint32_t)controlSelector << 8U) | (uint16_t)channel;
      wLength = length;
      break;

    case UAC_OUTPUT_TERMINAL:
      UnitID = AUDIO_Handle->class_desc.cs_desc.OutputTerminalDesc[0]->bTerminalID;
      InterfaceNum = 0U; /*Always zero Control Interface */
      wIndex = (uint16_t)((uint32_t)UnitID << 8U) | (uint16_t)InterfaceNum;
      wValue = (COPY_PROTECT_CONTROL << 8U);
      wLength = 1U;
      break;

    default:
      ret = USBH_FAIL;
      break;
  }

  if (ret != USBH_OK)
  {
    return ret;
  }

  phost->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_INTERFACE | \
                                         USB_REQ_TYPE_CLASS;

  phost->Control.setup.b.bRequest = UAC_GET_MIN;
  phost->Control.setup.b.wValue.w = wValue;
  phost->Control.setup.b.wIndex.w = wIndex;
  phost->Control.setup.b.wLength.w = wLength;

  //return (USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength));
  ret = USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength);
  while (ret == USBH_BUSY)
  {
    osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
    ret = USBH_CtlReq(phost, (uint8_t *)(void *)(AUDIO_Handle->mem), wLength);
  }
  SCB_InvalidateDCache_by_Addr((void *)AUDIO_Handle->mem, length);
  return ret;

}

/**
  * @brief  Handle Set Endpoint Controls Request
  * @param  phost: Host handle
  * @param  Ep: Endpoint address
  * @param  buf: pointer to data
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_SetEndpointControls(USBH_HandleTypeDef *phost,
                                                         uint8_t  Ep,
                                                         uint8_t *buff)
{
  uint16_t wValue, wIndex, wLength;
  USBH_StatusTypeDef ret = USBH_OK;

  wValue = SAMPLING_FREQ_CONTROL << 8U;
  wIndex = Ep;
  wLength = 3U; /*length of the frequency parameter*/

  phost->RequestState = CMD_SEND;

  phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_ENDPOINT | \
                                         USB_REQ_TYPE_CLASS;

  phost->Control.setup.b.bRequest = UAC_SET_CUR;
  phost->Control.setup.b.wValue.w = wValue;
  phost->Control.setup.b.wIndex.w = wIndex;
  phost->Control.setup.b.wLength.w = wLength;

  //return (USBH_CtlReq(phost, (uint8_t *)buff, wLength));
  ret = USBH_CtlReq(phost, (uint8_t *)buff, wLength);

  while (ret == USBH_BUSY)
  {
    osThreadFlagsWait (0xff, osFlagsWaitAny, FLAG_TIMEOUT);
    ret = USBH_CtlReq(phost, (uint8_t *)buff, wLength);
  }
  return ret;

}

/**
  * @brief  USBH_AUDIO_SetFrequency
  *         Set Audio sampling parameters
  * @param  phost: Host handle
  * @param  SampleRate: Sample Rate
  * @param  NbrChannels: Number of Channels
  * @param  BitPerSample: Bit Per Sample
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_AUDIO_SetFrequency(USBH_ClassTypeDef *pclass,
                                           uint16_t SampleRate,
                                           uint8_t  NbrChannels,
                                           uint8_t  BitPerSample)
{
  USBH_StatusTypeDef Status = USBH_BUSY;
  AUDIO_HandleTypeDef *AUDIO_Handle;
  uint8_t              index;
  uint8_t              change_freq = FALSE;
  uint32_t             freq_min, freq_max;
  uint8_t              num_supported_freq;
  USBH_HandleTypeDef *phost = pclass->phost;

  //if (phost->gState == HOST_CLASS)
  {
    AUDIO_Handle = (AUDIO_HandleTypeDef *) pclass->pData;

    if (AUDIO_Handle->play_state == AUDIO_PLAYBACK_IDLE)
    {
      if (AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->bSamFreqType == 0U)
      {
        freq_min = LE24(AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->tSamFreq[0]);
        freq_max = LE24(AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->tSamFreq[1]);

        if ((SampleRate >= freq_min) && (SampleRate <= freq_max))
        {
          change_freq = TRUE;
        }
      }
      else
      {
        num_supported_freq = (AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->bLength - 8U) / 3U;

        for (index = 0U; index < num_supported_freq; index++)
        {
          if (SampleRate == LE24(
                AUDIO_Handle->class_desc.as_desc[AUDIO_Handle->headphone.asociated_as].FormatTypeDesc->tSamFreq[index]))
          {
            change_freq = TRUE;
            break;
          }
        }
      }

      if (change_freq == TRUE)
      {
        uint8_t *buff;

        AUDIO_Handle->headphone.frequency = SampleRate;
        AUDIO_Handle->headphone.frame_length = (SampleRate * BitPerSample * NbrChannels) / 8000U;
#if 0
debug_printf("Rate:  = %d, BPS = %d, Chan = %d, FrameLength = %d\n", SampleRate, BitPerSample, NbrChannels, AUDIO_Handle->headphone.frame_length);
#endif
        AUDIO_Handle->play_state = AUDIO_PLAYBACK_SET_EP;
        buff = (uint8_t *)(void *)&AUDIO_Handle->headphone.frequency;
        Status = USBH_AUDIO_SetEndpointControls(phost, AUDIO_Handle->headphone.Ep, buff);
#if 0
debug_printf("Freq = %d, frame_len = %d\n", SampleRate, AUDIO_Handle->headphone.frame_length);
#endif
      }
    }
  }
  return Status;
}

/**
  * @brief  USBH_AUDIO_Play
  *         Start playback process
  * @param  phost: Host handle
  * @param  buf: pointer to raw audio data
  * @param  length: total length of the audio data
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_AUDIO_Play(USBH_HandleTypeDef *phost, uint8_t *buf, uint32_t length)
{
  USBH_StatusTypeDef Status = USBH_FAIL;
  AUDIO_HandleTypeDef *AUDIO_Handle;

  if (phost->gState == HOST_CLASS)
  {
    AUDIO_Handle = (AUDIO_HandleTypeDef *) phost->pActiveClassList->pData;

    if (AUDIO_Handle->play_state == AUDIO_PLAYBACK_IDLE)
    {
      AUDIO_Handle->headphone.buf = buf;
      AUDIO_Handle->headphone.total_length = length;
      AUDIO_Handle->play_state = AUDIO_PLAYBACK_PLAY;
      AUDIO_Handle->control_state = AUDIO_CONTROL_INIT;
      AUDIO_Handle->processing_state = AUDIO_DATA_START_OUT;
      Status = USBH_OK;

      phost->os_msg = (uint32_t)USBH_CLASS_EVENT;
      (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
    }
  }
  return Status;
}

/**
  * @brief  USBH_AUDIO_Pause
  *         Stop the playback process
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_AUDIO_Stop(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef Status = USBH_FAIL;
  Status = USBH_AUDIO_Suspend(phost);
  return Status;
}

/**
  * @brief  USBH_AUDIO_Suspend
  *         Suspend the playback process
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_AUDIO_Suspend(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef Status = USBH_FAIL;
  AUDIO_HandleTypeDef *AUDIO_Handle;

  if (phost->gState == HOST_CLASS)
  {
    AUDIO_Handle = (AUDIO_HandleTypeDef *) phost->pActiveClassList->pData;

    if (AUDIO_Handle->play_state == AUDIO_PLAYBACK_PLAY)
    {
      AUDIO_Handle->control_state = AUDIO_CONTROL_IDLE;
      AUDIO_Handle->play_state = AUDIO_PLAYBACK_IDLE;
      Status = USBH_OK;
    }
  }
  return Status;
}
/**
  * @brief  USBH_AUDIO_Resume
  *         Resume the playback process
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_AUDIO_Resume(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef Status = USBH_FAIL;
  AUDIO_HandleTypeDef *AUDIO_Handle;

  if (phost->gState == HOST_CLASS)
  {
    AUDIO_Handle = (AUDIO_HandleTypeDef *) phost->pActiveClassList->pData;

    if (AUDIO_Handle->play_state == AUDIO_PLAYBACK_IDLE)
    {
      AUDIO_Handle->control_state = AUDIO_CONTROL_INIT;
      AUDIO_Handle->play_state = AUDIO_PLAYBACK_PLAY;
    }
  }
  return Status;
}
/**
  * @brief  USBH_AUDIO_GetOutOffset
  *         return the current buffer pointer for OUT process
  * @param  phost: Host handle
  * @retval USBH Status
  */
int32_t USBH_AUDIO_GetOutOffset(USBH_HandleTypeDef *phost)
{
  AUDIO_HandleTypeDef *AUDIO_Handle;

  if (phost->gState == HOST_CLASS)
  {
    AUDIO_Handle = (AUDIO_HandleTypeDef *) phost->pActiveClassList->pData;

    if (AUDIO_Handle->play_state == AUDIO_PLAYBACK_PLAY)
    {
      return (int32_t)AUDIO_Handle->headphone.partial_ptr;
    }
  }
  return -1;
}

/**
  * @brief  USBH_AUDIO_ChangeOutBuffer
  *         Change audio data buffer address
  * @param  phost: Host handle
  * @param  buf: buffer address
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_AUDIO_ChangeOutBuffer(USBH_HandleTypeDef *phost, uint8_t *buf)
{
  USBH_StatusTypeDef Status = USBH_FAIL;
  AUDIO_HandleTypeDef *AUDIO_Handle;

  if (phost->gState == HOST_CLASS)
  {
    AUDIO_Handle = (AUDIO_HandleTypeDef *) phost->pActiveClassList->pData;

    if (AUDIO_Handle->play_state == AUDIO_PLAYBACK_PLAY)
    {
      if (AUDIO_Handle->headphone.buf <= buf)
      {
        AUDIO_Handle->headphone.cbuf = buf;

        if (AUDIO_Handle->headphone.buf == buf)
        {
          AUDIO_Handle->headphone.partial_ptr = 0U;
        }
        Status = USBH_OK;
      }
    }
  }
  return Status;
}

#if 0
/**
  * @brief  USBH_AUDIO_SetControlAttribute
  *         Set Control Attribute
  * @param  phost: Host handle
  * @param  attrib: control attribute
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_AUDIO_SetControlAttribute(USBH_HandleTypeDef *phost, uint8_t attrib)
{
  USBH_StatusTypeDef status = USBH_BUSY;
  AUDIO_HandleTypeDef *AUDIO_Handle;


  AUDIO_Handle = (AUDIO_HandleTypeDef *) phost->pActiveClassList->pData;

  switch (attrib)
  {
    case 0x01:
      AUDIO_Handle->headphone.attribute.volume += AUDIO_Handle->headphone.attribute.resolution;
      break;

    case 0x02:
      AUDIO_Handle->headphone.attribute.volume -= AUDIO_Handle->headphone.attribute.resolution;
      break;

    default :
      status = USBH_FAIL;
      break;
  }

  if (AUDIO_Handle->headphone.attribute.volume > AUDIO_Handle->headphone.attribute.volumeMax)
  {
    AUDIO_Handle->headphone.attribute.volume = AUDIO_Handle->headphone.attribute.volumeMax;
  }

  if (AUDIO_Handle->headphone.attribute.volume < AUDIO_Handle->headphone.attribute.volumeMin)
  {
    AUDIO_Handle->headphone.attribute.volume = AUDIO_Handle->headphone.attribute.volumeMin;
  }

  if (AUDIO_SetVolume(phost,
                      AUDIO_Handle->temp_feature,
                      (uint8_t)AUDIO_Handle->temp_channels,
                      (uint16_t)AUDIO_Handle->headphone.attribute.volume) != USBH_BUSY)
  {

    if (AUDIO_Handle->temp_channels == 1U)
    {
      AUDIO_Handle->temp_feature = AUDIO_Handle->headphone.asociated_feature;
      AUDIO_Handle->temp_channels = AUDIO_Handle->headphone.asociated_channels;
      status = USBH_OK;
    }
    else
    {
      AUDIO_Handle->temp_channels--;
    }
    AUDIO_Handle->cs_req_state = AUDIO_REQ_GET_VOLUME;
  }


  return status;
}


/**
  * @brief  USBH_AUDIO_SetVolume
  *         Set Volume
  * @param  phost: Host handle
  * @param  volume: VOLUME_UP/ VOLUME_DOWN
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_AUDIO_SetVolume(USBH_HandleTypeDef *phost, AUDIO_VolumeCtrlTypeDef volume_ctl)
{
  AUDIO_HandleTypeDef *AUDIO_Handle = (AUDIO_HandleTypeDef *) phost->pActiveClassList->pData;

  if ((volume_ctl == VOLUME_UP) || (volume_ctl == VOLUME_DOWN))
  {
    if (phost->gState == HOST_CLASS)
    {
      AUDIO_Handle = (AUDIO_HandleTypeDef *) phost->pActiveClassList->pData;
      if (AUDIO_Handle->play_state == AUDIO_PLAYBACK_PLAY)
      {
        AUDIO_Handle->control_state = (volume_ctl == VOLUME_UP) ? AUDIO_CONTROL_VOLUME_UP : AUDIO_CONTROL_VOLUME_DOWN;
        return USBH_OK;
      }
    }
  }

  return USBH_FAIL;
}
#endif

#if 0
/**
  * @brief  AUDIO_SetVolume
  *         Set Volume
  * @param  phost: Host handle
  * @param  feature: feature Unit index
  * @param  channel: channel index
  * @param  volume: new volume
  * @retval USBH Status
  */
static USBH_StatusTypeDef AUDIO_SetVolume(USBH_HandleTypeDef *phost, uint8_t feature, uint8_t channel, uint16_t volume)
{
  USBH_StatusTypeDef status = USBH_BUSY;
  AUDIO_HandleTypeDef *AUDIO_Handle;


debug_printf("%s: %d\n", __FUNCTION__, volume);
  AUDIO_Handle = (AUDIO_HandleTypeDef *) phost->pActiveClassList->pData;

  AUDIO_Handle->mem[0] = volume;

  status = USBH_AC_SetCur(phost, UAC_FEATURE_UNIT, feature,
                          VOLUME_CONTROL, channel, 2U);

  return status;
}
#endif

/**
  * @brief  The function informs user that Settings have been changed
  *  @param  phost: Selected device
  * @retval None
  */
__weak void USBH_AUDIO_FrequencySet(USBH_HandleTypeDef *phost)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(phost);
}

/**
  * @brief  The function informs user that User data are processed
  *  @param  phost: Selected device
  * @retval None
  */
__weak void  USBH_AUDIO_BufferEmptyCallback(USBH_HandleTypeDef *phost)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(phost);
}
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */


/**
  * @}
  */


/**
  * @}
  */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
