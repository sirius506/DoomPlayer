/**
 * @brief SONY Dual Sensor Controller driver
 */
#include "DoomPlayer.h"
#include "usbh_hid_dualsense.h"
#include "usbh_hid_parser.h"
#include "Fusion.h"
#include "lvgl.h"
#include "app_gui.h"
#include "app_task.h"

#define	REPORT_SIZE	sizeof(struct dualsense_input_report)

SECTION_USBSRAM uint8_t calibdata[DS_FEATURE_REPORT_CALIBRATION_SIZE+3];
SECTION_USBSRAM uint8_t firmdata[DS_FEATURE_REPORT_FIRMWARE_INFO_SIZE+3];
SECTION_USBSRAM uint8_t dual_rx_buf[REPORT_SIZE+4];
SECTION_USBSRAM uint8_t dual_rx_report_buf[REPORT_SIZE*5];
SECTION_USBSRAM struct dualsense_input_report cur_report;
SECTION_USBSRAM struct dualsense_input_report prev_report;
SECTION_USBSRAM struct dualsense_output_report out_report;
SECTION_USBSRAM struct dualsense_touch_point prev_points[2];

#define	SAMPLE_PERIOD	(0.004f)
#define	SAMPLE_RATE	(250)

/*
 * Vertual Button bitmask definitions
 */
#define	VBMASK_SQUARE	(1<<0)
#define	VBMASK_CROSS	(1<<1)
#define	VBMASK_CIRCLE	(1<<2)
#define	VBMASK_TRIANGLE	(1<<3)
#define	VBMASK_L1	(1<<4)
#define	VBMASK_R1	(1<<5)
#define	VBMASK_L2	(1<<6)
#define	VBMASK_R2	(1<<7)
#define	VBMASK_CREATE	(1<<8)
#define	VBMASK_OPTION	(1<<9)
#define	VBMASK_L3	(1<<10)
#define	VBMASK_R3	(1<<11)
#define	VBMASK_PS	(1<<12)
#define	VBMASK_TOUCH	(1<<13)
#define	VBMASK_MUTE	(1<<14)
#define	VBMASK_UP	(1<<15)
#define	VBMASK_LEFT	(1<<16)
#define	VBMASK_RIGHT	(1<<17)
#define	VBMASK_DOWN	(1<<18)

extern int fft_getcolor(uint8_t *p);
extern void GetPlayerHealthColor(uint8_t *cval);

extern USBH_StatusTypeDef USBH_InterruptSendData(USBH_HandleTypeDef *phost, uint8_t *buff,
                                          uint8_t length, uint8_t pipe_num);

const void (*HidProcTable[])(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton) = {
      Generate_LVGL_Keycode,
      Display_DualSense_Info,
      Generate_DOOM_Keycode,
};

/* 0x08:  No button
 * 0x00:  Up
 * 0x01:  RightUp
 * 0x02:  Right
 * 0x03:  RightDown
 * 0x04:  Down
 * 0x05:  LeftDown
 * 0x06:  Left
 * 0x07:  LeftUp
 */

/*
 * @brief Hat key to Virtual button mask conversion table
 */
static const uint32_t hatmap[16] = {
  VBMASK_UP,   VBMASK_UP|VBMASK_RIGHT,  VBMASK_RIGHT, VBMASK_RIGHT|VBMASK_DOWN,
  VBMASK_DOWN, VBMASK_LEFT|VBMASK_DOWN, VBMASK_LEFT,  VBMASK_UP|VBMASK_LEFT,
  0, 0, 0, 0,
  0, 0, 0, 0,
};

typedef struct {
  uint32_t mask;
  lv_key_t lvkey;
} PADKEY_DATA;

/*
 * Map Virtual button bitmask to LVGL Keypad code
 */
static const PADKEY_DATA PadKeyDefs[] = {
  { VBMASK_DOWN,     LV_KEY_DOWN },
  { VBMASK_RIGHT,    LV_KEY_NEXT },
  { VBMASK_LEFT,     LV_KEY_PREV },
  { VBMASK_UP,       LV_KEY_UP },
  { VBMASK_PS,       LV_KEY_HOME },
  { VBMASK_TRIANGLE, LV_KEY_DEL },
  { VBMASK_CIRCLE,   LV_KEY_ENTER },
  { VBMASK_CROSS,    LV_KEY_DEL },
  { VBMASK_SQUARE,   LV_KEY_ENTER },
  { VBMASK_L1,       LV_KEY_LEFT },
  { VBMASK_R1,       LV_KEY_RIGHT },
  { 0, 0 },			/* Data END marker */
};

#define	VBMASK_CHECK	(VBMASK_DOWN|VBMASK_RIGHT|VBMASK_LEFT| \
			 VBMASK_UP|VBMASK_PS|VBMASK_TRIANGLE| \
                         VBMASK_L1|VBMASK_R1| \
			 VBMASK_CIRCLE|VBMASK_CROSS|VBMASK_SQUARE)

static struct ds_data DsData;
static FusionAhrs ahrs;
static FusionOffset hoffset;

#define	le16_to_cpu(x)	(x)

int16_t get_le16val(uint8_t *bp)
{
  int16_t val;

  val = bp[0] + (bp[1] << 8);
  return val;
}

int16_t calibVals[17];

#define	TRIG_FEEDBACK	0x21

void TriggerFeedbackSetup(uint8_t *dst, int position, int strength)
{
  if (position > 9 || position < 0)
    return;
  if (strength > 8 || strength < 0)
    return;
  if (strength > 0)
  {
    uint8_t forceValue = (strength - 1) & 0x07;
    uint32_t forceZones = 0;
    uint16_t activeZones = 0;

    for (int i = position; i < 10; i++)
    {
      forceZones |= (uint32_t)(forceValue << (3 * i));
      activeZones |= (uint16_t)(i << i);
    }
    dst[0] = TRIG_FEEDBACK;
    dst[1] = (activeZones & 0xff);
    dst[2] = (activeZones >> 8) & 0xff;
    dst[3] = (forceZones & 0xff);
    dst[4] = (forceZones >> 8) & 0xff;
    dst[5] = (forceZones >> 16) & 0xff;
    dst[6] = (forceZones >> 24) & 0xff;
    dst[7] = 0x00;
    dst[8] = 0x00;
    dst[9] = 0x00;
    dst[10] = 0x00;
  }
}

void DualSenseSetup(USBH_ClassTypeDef *pclass)
{
  USBH_StatusTypeDef st;
  uint32_t *vph, *vpf;
  struct ds_data *ds = &DsData;

  FusionOffsetInitialise(&hoffset, SAMPLE_RATE);
  FusionAhrsInitialise(&ahrs);
debug_printf("AHRS init started.\n");

  // Set AHRS algorithm settings
  const FusionAhrsSettings settings = {
    .gain = 0.5f,
    .accelerationRejection = 10.0f,
    .rejectionTimeout = 10 * SAMPLE_RATE,
  };
  FusionAhrsSetSettings(&ahrs, &settings);

  memset(firmdata, 0, DS_FEATURE_REPORT_FIRMWARE_INFO_SIZE);
  st = USBH_HID_GetReport(pclass->phost, 3, DS_FEATURE_REPORT_FIRMWARE_INFO, firmdata, DS_FEATURE_REPORT_FIRMWARE_INFO_SIZE);
  (void) st;
  vph = (uint32_t *)&firmdata[0x1c];
  vpf = (uint32_t *)&firmdata[0x2c];
  debug_printf("hwv: %x, swv: %x\n", *vph, *vpf);
  osDelay(5);
  memset(calibdata, 0, DS_FEATURE_REPORT_CALIBRATION_SIZE);
  st = USBH_HID_GetReport(pclass->phost, 3, DS_FEATURE_REPORT_CALIBRATION, calibdata, DS_FEATURE_REPORT_CALIBRATION_SIZE);

  int16_t gyro_pitch_bias, gyro_yaw_bias, gyro_roll_bias;
  int16_t gyro_pitch_plus, gyro_pitch_minus;
  int16_t gyro_yaw_plus, gyro_yaw_minus;
  int16_t gyro_roll_plus, gyro_roll_minus;
  int16_t gyro_speed_plus, gyro_speed_minus;
  int16_t acc_x_plus, acc_x_minus;
  int16_t acc_y_plus, acc_y_minus;
  int16_t acc_z_plus, acc_z_minus;

  float flNumerator;
  int range_2g;

  calibVals[0] = gyro_pitch_bias = get_le16val(&calibdata[1]);
  calibVals[1] = gyro_yaw_bias = get_le16val(&calibdata[3]);
  calibVals[2] = gyro_roll_bias = get_le16val(&calibdata[5]);
  calibVals[3] = gyro_pitch_plus = get_le16val(&calibdata[7]);
  calibVals[4] = gyro_pitch_minus = get_le16val(&calibdata[9]);
  calibVals[5] = gyro_yaw_plus = get_le16val(&calibdata[11]);
  calibVals[6] = gyro_yaw_minus = get_le16val(&calibdata[13]);
  calibVals[7] = gyro_roll_plus = get_le16val(&calibdata[15]);
  calibVals[8] = gyro_roll_minus = get_le16val(&calibdata[17]);
  calibVals[9] = gyro_speed_plus = get_le16val(&calibdata[19]);
  calibVals[10] = gyro_speed_minus = get_le16val(&calibdata[21]);
  calibVals[11] = acc_x_plus = get_le16val(&calibdata[23]);
  calibVals[12] = acc_x_minus = get_le16val(&calibdata[25]);
  calibVals[13] = acc_y_plus = get_le16val(&calibdata[27]);
  calibVals[14] = acc_y_minus = get_le16val(&calibdata[29]);
  calibVals[15] = acc_z_plus = get_le16val(&calibdata[31]);
  calibVals[16] = acc_z_minus = get_le16val(&calibdata[33]);
  osDelay(5);

  /*
   * Set gyroscope calibration and normalization parameters.
   * Data values will be normalized to 1/DS_GYRO_RES_PER_DEG_S degree/s.
   */

  flNumerator = (gyro_speed_plus + gyro_speed_minus) * DS_GYRO_RES_PER_DEG_S;

  ds->gyro_calib_data[0].bias = gyro_pitch_bias;
  ds->gyro_calib_data[0].sensitivity = flNumerator / (gyro_pitch_plus - gyro_pitch_minus);

  ds->gyro_calib_data[1].bias = gyro_yaw_bias;
  ds->gyro_calib_data[1].sensitivity = flNumerator / (gyro_yaw_plus - gyro_yaw_minus);

  ds->gyro_calib_data[2].bias = gyro_roll_bias;
  ds->gyro_calib_data[2].sensitivity = flNumerator / (gyro_roll_plus - gyro_roll_minus);

  /*
   * Set accelerometer calibration and normalization parameters.
   * Data values will be normalized to 1/DS_ACC_RES_PER_G g.
   */
  range_2g = acc_x_plus - acc_x_minus;
  ds->accel_calib_data[0].bias = acc_x_plus - range_2g / 2;
  ds->accel_calib_data[0].sensitivity = 2.0f * DS_ACC_RES_PER_G / (float)range_2g;

  range_2g = acc_y_plus - acc_y_minus;
  ds->accel_calib_data[1].bias = acc_y_plus - range_2g / 2;
  ds->accel_calib_data[1].sensitivity = 2.0f * DS_ACC_RES_PER_G / (float)range_2g;

  range_2g = acc_z_plus - acc_z_minus;
  ds->accel_calib_data[2].bias = acc_z_plus - range_2g / 2;
  ds->accel_calib_data[2].sensitivity = 2.0f * DS_ACC_RES_PER_G / (float)range_2g;

  USBH_HID_SetDualSenseLightbar(pclass);
  osDelay(10);
  SetupTrigger(pclass);
}

void DualSenseResetFusion()
{
  FusionAhrsReset(&ahrs);
}

USBH_StatusTypeDef USBH_HID_DualSenseInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) pclass->pData;

  memset((uint8_t *)&prev_report, 0, sizeof(prev_report));

  if (HID_Handle->length > sizeof(dual_rx_report_buf))
  {
    HID_Handle->length = sizeof(dual_rx_report_buf);
  }
  HID_Handle->pData = (uint8_t *)(void *)dual_rx_buf;
  USBH_HID_FifoInit(&HID_Handle->fifo, phost->device.Data, sizeof(dual_rx_report_buf));

  postGuiEventMessage(GUIEV_DUALSENSE_READY, 0, NULL, NULL);
  return USBH_OK;
}

FUSION_ANGLE ImuAngle;

/*
 * Decode DualSense Input report
 */
void USBH_HID_DualSenseDecode(HID_HandleTypeDef *HID_Handle)
{
  struct dualsense_input_report *rp;
  int i;
  struct ds_data *ds = &DsData;
  FUSION_ANGLE ImuTmp;

  if (HID_Handle->length != REPORT_SIZE)
  {
    return;
  }

  FusionVector gyroscope;
  FusionVector accelerometer;
  FusionEuler euler;
  int raw_data;
  float calib_data;
  int16_t angle;

  /*Fill report */
  rp = &cur_report;
  SCB_InvalidateDCache_by_Addr((uint32_t *)HID_Handle->pData, REPORT_SIZE);
  memcpy(&cur_report, HID_Handle->pData, REPORT_SIZE);

  for (i = 0; i < 3; i++)
  {
      raw_data = le16_to_cpu(rp->gyro[i]);
      calib_data = (float)(raw_data - ds->gyro_calib_data[i].bias) * ds->gyro_calib_data[i].sensitivity;
#define	SWAP_ZYx
#ifdef SWAP_ZY
      switch (i)
      {
      case 0:
        gyroscope.array[0] = calib_data / DS_GYRO_RES_PER_DEG_S;
        break;
      case 1:
        gyroscope.array[2] = calib_data / DS_GYRO_RES_PER_DEG_S;
        break;
      case 2:
      default:
        gyroscope.array[1] = calib_data / DS_GYRO_RES_PER_DEG_S;
        break;
      }
#else
      gyroscope.array[i] = calib_data / DS_GYRO_RES_PER_DEG_S;
#endif
  }
  for (i = 0; i < 3; i++)
  {
      raw_data = le16_to_cpu(rp->accel[i]);
      calib_data = (float)(raw_data - ds->accel_calib_data[i].bias) * ds->accel_calib_data[i].sensitivity;
#ifdef SWAP_XY
      switch (i)
      {
      case 0:
        accelerometer.array[0] = calib_data / DS_ACC_RES_PER_G;
        break;
      case 1:
        accelerometer.array[2] = calib_data / DS_ACC_RES_PER_G;
        break;
      case 2:
      default:
        accelerometer.array[1] = calib_data / DS_ACC_RES_PER_G;
        break;
      }
#else
      accelerometer.array[i] = calib_data / DS_ACC_RES_PER_G;
#endif
  }

#define DO_CALIB
#ifdef DO_CALIB
  // Apply calibration
  gyroscope = FusionCalibrationInertial(gyroscope, FUSION_IDENTITY_MATRIX, FUSION_VECTOR_ONES, FUSION_VECTOR_ZERO );
  accelerometer = FusionCalibrationInertial(accelerometer, FUSION_IDENTITY_MATRIX, FUSION_VECTOR_ONES, FUSION_VECTOR_ZERO );

  gyroscope = FusionOffsetUpdate(&hoffset, gyroscope);
#endif
  FusionAhrsUpdateNoMagnetometer(&ahrs, gyroscope, accelerometer, SAMPLE_PERIOD);

  if (ahrs.initialising == 0)
  {

  euler  = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));

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

  if ((rp->seq_number & 0x07) == 0)
  {
    uint8_t hat;
    uint32_t vbutton;

    hat = (rp->buttons[0] & 0x0f);
    vbutton = (rp->buttons[2] & 0x0f) << 12;	/* PS, Touch, Mute */
    vbutton |= rp->buttons[1] << 4;		/* L1, R1, L2, R2, L3, R3, Create, Option */
    vbutton |= (rp->buttons[0] & 0xf0)>> 4;	/* Square, Cross, Circle, Triangle */
    vbutton |= hatmap[hat];

    HidProcTable[HID_Handle->hid_mode](rp, hat, vbutton);
  }
}

void SetupTrigger(USBH_ClassTypeDef *pclass)
{
  struct dualsense_output_report *rp = &out_report;
  HID_HandleTypeDef *HID_Handle;

  memset(rp, 0, sizeof(out_report));
  TriggerFeedbackSetup(rp->RightTriggerFFB,  3, 3);
  TriggerFeedbackSetup(rp->LeftTriggerFFB,  3, 3);

  rp->report_id = DS_OUTPUT_REPORT_USB;
  rp->valid_flag0 = VALID_FLAG0_RIGHT_TRIGGER | VALID_FLAG0_LEFT_TRIGGER;
  HID_Handle = (HID_HandleTypeDef *) pclass->pData;

  USBH_InterruptSendData(pclass->phost, (uint8_t *)rp, sizeof(out_report), HID_Handle->OutPipe);
      osEventFlagsWait (pclass->classEventFlag, EVF_URB_DONE, osFlagsWaitAny, osWaitForever);
}

void USBH_HID_SetDualSenseLightbar(USBH_ClassTypeDef *pclass)
{
  struct dualsense_output_report *rp = &out_report;
  int st;
  HID_HandleTypeDef *HID_Handle;

  memset(rp, 0, sizeof(out_report));
  rp->report_id = DS_OUTPUT_REPORT_USB;
  rp->valid_flag2 = DS_OUTPUT_VALID_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE;
  rp->lightbar_setup = DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT;
  HID_Handle = (HID_HandleTypeDef *) pclass->pData;
  st = USBH_InterruptSendData(pclass->phost, (uint8_t *)rp, sizeof(out_report), HID_Handle->OutPipe);
      osEventFlagsWait (pclass->classEventFlag, EVF_URB_DONE, osFlagsWaitAny, osWaitForever);
  //debug_printf("set_report0: %d (%d)\n", st, sizeof(out_report));

  memset(rp, 0, sizeof(out_report));
  rp->report_id = DS_OUTPUT_REPORT_USB;
  rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_PLAYER_INDICATOR_CONTROL_ENABLE;
  rp->player_leds = 1 | (1 << 3);
  st = USBH_InterruptSendData(pclass->phost, (uint8_t *)rp, sizeof(out_report), HID_Handle->OutPipe);
      osEventFlagsWait (pclass->classEventFlag, EVF_URB_DONE, osFlagsWaitAny, osWaitForever);

  memset(rp, 0, sizeof(out_report));
  rp->report_id = DS_OUTPUT_REPORT_USB;
  rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE;
  rp->lightbar_red = 0;
  rp->lightbar_green = 255;
  rp->lightbar_blue = 0;
  rp->led_brightness = 255;

  st = USBH_InterruptSendData(pclass->phost, (uint8_t *)rp, sizeof(out_report), HID_Handle->OutPipe);
      osEventFlagsWait (pclass->classEventFlag, EVF_URB_DONE, osFlagsWaitAny, osWaitForever);
  //debug_printf("set_report: %d (%d)\n", st, sizeof(out_report));
  (void) st;
}

void UpdateBarColor(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost, uint8_t uc)
{
  struct dualsense_output_report *rp = &out_report;
  HID_HandleTypeDef *HID_Handle;
  int st;
  uint8_t cval[3];

  {
    HID_Handle = (HID_HandleTypeDef *) pclass->pData;
    rp->report_id = DS_OUTPUT_REPORT_USB;

    if (uc & 1)
    {
      rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE;

      if (HID_Handle->hid_mode != HID_MODE_DOOM)
      {
        fft_getcolor(cval);
        rp->lightbar_red = cval[0];
        rp->lightbar_green = cval[1];
        rp->lightbar_blue = cval[2];
      }
      else
      {
        GetPlayerHealthColor(cval);
        rp->lightbar_red = cval[0];
        rp->lightbar_green = cval[1];
        rp->lightbar_blue = cval[2];
      }
    }
    else
    {
      rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_PLAYER_INDICATOR_CONTROL_ENABLE;
      if (HID_Handle->hid_mode != HID_MODE_DOOM)
      {
        int vval;

        vval = fft_getcolor(cval);
        if (vval > 250)
          rp->player_leds = 0x1f;	// Turn on all LEDs
        else if (vval > 150)
          rp->player_leds = 0x0e;
        else if (vval > 20)
          rp->player_leds = 0x04;	// Center only
        else
          rp->player_leds = 0;		// Turn off all LEDs
      }
      else
      {
        rp->player_leds = (1 << 2);
      }
    }

    st = USBH_InterruptSendData(pclass->phost, (uint8_t *)rp, sizeof(out_report), HID_Handle->OutPipe);
  }
  (void) st;
} 

static uint32_t last_button;
static int pad_timer;
static int16_t xinc, yinc;

/**
 * @brief Convert HID input report to LVGL kaycode
 */
void Generate_LVGL_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton)
{
  static lv_indev_data_t pad_data;
  int ix, iy, ax, ay;

  if (vbutton != last_button)
  {
    uint32_t changed;
    const PADKEY_DATA *padkey = PadKeyDefs;

    changed = last_button ^ vbutton;
    changed &= VBMASK_CHECK;

    while (changed && padkey->mask)
    {
      if (changed & padkey->mask)
      {
        changed &= ~padkey->mask;

        pad_data.key = padkey->lvkey;
        pad_data.state = (vbutton & padkey->mask)? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        pad_data.continue_reading = (changed != 0)? true : false;
        pad_timer = 0;
        send_padkey(&pad_data);

      }
      padkey++;
    }
    last_button = vbutton;
  }
  else if (pad_data.state == LV_INDEV_STATE_PRESSED)
  {
    /* Key has been pressed */

    pad_timer++;
    if (pad_timer > 15)		/* Takes repeat start delay */
    {
      if ((pad_timer & 3) == 0)	/* inter repeat delay passed */
      {
        /* Generate release and press event */
        pad_data.state = LV_INDEV_STATE_RELEASED;
        send_padkey(&pad_data);
        pad_data.state = LV_INDEV_STATE_PRESSED;
        send_padkey(&pad_data);
      }
    }
  }
  ix = rp->rx - 128;
  iy = rp->ry - 128;
  ax = (ix < 0)? -ix : ix;
  ay = (iy < 0)? -iy : iy;
  ax = (ax > 80) ? 1 : 0;
  ay = (ay > 80) ? 1 : 0;

  if (ax != 0)
  {
    if (ix < 0) ax = -1;
    if (ax != xinc)
      postGuiEventMessage((ax > 0)? GUIEV_XDIR_INC : GUIEV_XDIR_DEC, 0, NULL, NULL);
  }
  xinc = ax;

  if (ay != 0)
  {
    if (iy < 0) ay = -1;
    if (ay != yinc)
      postGuiEventMessage((ay > 0)? GUIEV_YDIR_DEC : GUIEV_YDIR_INC, 0, NULL, NULL);
  }
  yinc = ay;
}

