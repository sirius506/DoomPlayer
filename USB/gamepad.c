/**
 *  Gamepad controller driver
 */
#include "DoomPlayer.h"
#include "Fusion.h"
#include "usbh_hid.h"
#include "usbh_def.h"
#include "usbh_hid_parser.h"
#include "usbh_ctlreq.h"
#include "usbh_ioreq.h"
#include "gamepad.h"

#define	PS_OUTPUT_CRC32_SEED	0xA2

extern CRC_HandleTypeDef hcrc;

static FusionAhrs ahrs;
static FusionOffset hoffset;

FUSION_ANGLE ImuAngle;

static GAMEPAD_INFO GamePadInfo;

/**
 * @brief List of supported gamepads.
 */
struct sGamePad KnownGamePads[] = {
  { VID_SONY, PID_DUALSHOCK, "DualShock4", &DualShockDriver },
  { VID_SONY, PID_DUALSENSE, "DualSense",  &DualSenseDriver },
  { VID_SONY, PID_ZERO2,     "8BitDo Zero 2",  &Zero2Driver },
};

GAMEPAD_INFO *IsSupportedGamePad(uint16_t vid, uint16_t pid)
{
  int i;
  struct sGamePad *gp = KnownGamePads;

  for (i = 0; i < sizeof(KnownGamePads)/sizeof(struct sGamePad); i++)
  {
    if (gp->vid == vid && gp->pid == pid)
    {
      GamePadInfo.name = gp->name;
      GamePadInfo.hid_mode = HID_MODE_LVGL;
      GamePadInfo.padDriver = gp->padDriver;
      return &GamePadInfo;
    }
    gp++;
  }
  debug_printf("This gamepad is not supported.\n");
  return NULL;
}

const int modemap[3] = {
 HID_LVGL_BIT, HID_TEST_BIT, HID_DOOM_BIT,
};

void GamepadHidMode(GAMEPAD_INFO *padInfo, int mode)
{
  padInfo->hid_mode = mode;

  if (padInfo->pclass)
  {
    PIPE_EVENT pev;

    pev.channel = 0;
    pev.state = modemap[mode];
    osMessageQueuePut(padInfo->pclass->classEventQueue, &pev, 0, 0);
  }
}

SECTION_USBSRAM uint8_t gamepad_report_data[100];
SECTION_USBSRAM uint8_t gamepad_rx_report_buf[100];

#define GAMEPAD_QUEUE_SIZE	7

USBH_StatusTypeDef GamepadHidInit(HID_HandleTypeDef *HID_Handle, USBH_HandleTypeDef *phost, int report_size)
{
  if (HID_Handle->length > report_size)
  {
    HID_Handle->length = report_size;
  }
  HID_Handle->pData = HID_Handle->report.ptr = gamepad_rx_report_buf;

  debug_printf("%s: %d, %d\n", __FUNCTION__, GAMEPAD_QUEUE_SIZE * report_size, sizeof(phost->device.Data));
  if ((GAMEPAD_QUEUE_SIZE * report_size) > sizeof(phost->device.Data))
  {
    debug_printf("Bad report_size: %d\n", report_size);
    return USBH_FAIL;
  }
  else
  {
#ifdef USE_HID_FIFO
    USBH_HID_FifoInit(&HID_Handle->fifo, phost->device.Data, (uint16_t)(GAMEPAD_QUEUE_SIZE * report_size));
#endif
  }
  return USBH_OK;
}

static const uint8_t output_seed[] = { PS_OUTPUT_CRC32_SEED };

/**
 * @brief Compute CRC32 value for BT output report
 */
uint32_t bt_comp_crc(uint8_t *ptr, int len)
{
  uint32_t crcval;

  SCB_InvalidateDCache_by_Addr((uint32_t *)ptr, len);
  crcval = HAL_CRC_Calculate(&hcrc, (uint32_t *)output_seed, 1);
  crcval = HAL_CRC_Accumulate(&hcrc, (uint32_t *)ptr, len - 4);
  crcval = ~crcval;
  return crcval;
}

void setup_fusion(int sample_rate, const FusionAhrsSettings *psettings)
{
  FusionOffsetInitialise(&hoffset, sample_rate);
  FusionAhrsInitialise(&ahrs);

  FusionAhrsSetSettings(&ahrs, psettings);
}

void gamepad_reset_fusion()
{
  FusionAhrsReset(&ahrs);
}

void gamepad_process_fusion(float sample_period, FusionVector gyroscope, FusionVector accelerometer)
{
  FusionEuler euler;
  FUSION_ANGLE ImuTmp;
  int16_t angle;

  // Apply calibration
  gyroscope = FusionCalibrationInertial(gyroscope, FUSION_IDENTITY_MATRIX, FUSION_VECTOR_ONES, FUSION_VECTOR_ZERO );
  accelerometer = FusionCalibrationInertial(accelerometer, FUSION_IDENTITY_MATRIX, FUSION_VECTOR_ONES, FUSION_VECTOR_ZERO );

  gyroscope = FusionOffsetUpdate(&hoffset, gyroscope);
  FusionAhrsUpdateNoMagnetometer(&ahrs, gyroscope, accelerometer, sample_period);

  if (ahrs.initialising == 0)
  {
    euler  = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));

#ifdef XDEBUG
    if ((rp->seq_number & 0x3F) == 0)
    {
#ifdef SHOW_EULER
      debug_printf("%d, %d, %d\n", (int)euler.angle.roll, (int)euler.angle.pitch, (int)euler.angle.yaw);
#endif
#ifdef SHOW_ACCEL
      debug_printf("A: %d, %d, %d\n",
        (int)(accelerometer.array[0] * 1000.0f),	// X
        (int)(accelerometer.array[1] * 1000.0f),	// Z
        (int)(accelerometer.array[2] * 1000.0f));	// Y
#endif
#ifdef SHOW_GYRO
      debug_printf("G: %d, %d, %d\n",
        (int)(gyroscope.array[0] * 10.0f),	// X
        (int)(gyroscope.array[1] * 10.0f),	// Z
        (int)(gyroscope.array[2] * 10.0f));	// Y
#endif
      //debug_printf("Temp = %x (%d) @ %d\n", rp->Temperature, rp->Temperature, rp->timestamp);
    }
#endif

    angle = (int16_t) euler.angle.roll;
#if 1
    angle -= 90;
    if (angle < -180)
      angle += 360;
#endif
    ImuTmp.roll = angle;
    angle = (int16_t) euler.angle.pitch;
    ImuTmp.pitch = angle;
    ImuTmp.yaw = (int16_t) euler.angle.yaw;
    if (ImuTmp.yaw < 0)
      ImuTmp.yaw += 360;
    ImuAngle = ImuTmp;

  }
}
