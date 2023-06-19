/**
 * @brief SONY Dual Sensor Controller driver
 */
#include "DoomPlayer.h"
#include "Fusion.h"
#include "lvgl.h"
#include "app_gui.h"
#include "app_task.h"
#include "gamepad.h"
#include "classic/hid_host.h"
#include "SDL.h"
#include "SDL_joystick.h"
#include "dualsense_report.h"
#include "audio_output.h"

#define	REPORT_SIZE	sizeof(struct dualsense_input_report)

SECTION_USBSRAM uint8_t calibdata[DS_FEATURE_REPORT_CALIBRATION_SIZE+3];
SECTION_USBSRAM uint8_t firmdata[DS_FEATURE_REPORT_FIRMWARE_INFO_SIZE+3];
SECTION_USBSRAM uint8_t dual_rx_buf[REPORT_SIZE+4];
SECTION_USBSRAM uint8_t dual_rx_report_buf[REPORT_SIZE*5];
SECTION_USBSRAM struct dualsense_input_report cur_report;
SECTION_USBSRAM struct dualsense_usbout_report out_report;

#define	BTREPQ_DEPTH	2
SECTION_USBSRAM struct dualsense_btout_report reportBuffer[BTREPQ_DEPTH];
struct dualsense_btout_report *reportqBuffer[BTREPQ_DEPTH];
MESSAGEQ_DEF(reportq, reportqBuffer, sizeof(reportqBuffer))

static osMessageQueueId_t btreportqId;
static int initial_report;


#define	SAMPLE_PERIOD	(0.004f)
#define	SAMPLE_RATE	(250)

extern int fft_getcolor(uint8_t *p);
extern void GetPlayerHealthColor(uint8_t *cval);

static void DualSense_LVGL_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton);
static void DualSense_DOOM_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton);
static void DualSense_Display_Status(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton);

const void (*HidProcTable[])(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton) = {
      DualSense_LVGL_Keycode,
      DualSense_Display_Status,
      DualSense_DOOM_Keycode,
};

extern SDL_Joystick *SDL_GetJoystickPtr();

const AUDIO_DEVCONF DualSenseUsbAudio = {
  .mix_mode = MIXER_USB_OUTPUT|MIXER_FFT_ENABLE,
  .playRate = 48000,
  .numChan = 4,
  .pseudoRate = 48000,
  .pDriver = &usb_output_driver,
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

#define	le16_to_cpu(x)	(x)

static int16_t get_le16val(uint8_t *bp)
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

static void set_joystick_params()
{
  SDL_Joystick *joystick = SDL_GetJoystickPtr();

  joystick->name = "DualSence";
  joystick->naxes = 1;
  joystick->nhats = 1;
  joystick->nbuttons = 17;
  joystick->hats = 0;
  joystick->buttons = 0;;
}

static void process_calibdata(struct ds_data *ds, uint8_t *dp)
{
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

  calibVals[0] = gyro_pitch_bias = get_le16val(dp + 1);
  calibVals[1] = gyro_yaw_bias = get_le16val(dp + 3);
  calibVals[2] = gyro_roll_bias = get_le16val(dp + 5);
  calibVals[3] = gyro_pitch_plus = get_le16val(dp + 7);
  calibVals[4] = gyro_pitch_minus = get_le16val(dp + 9);
  calibVals[5] = gyro_yaw_plus = get_le16val(dp + 11);
  calibVals[6] = gyro_yaw_minus = get_le16val(dp + 13);
  calibVals[7] = gyro_roll_plus = get_le16val(dp + 15);
  calibVals[8] = gyro_roll_minus = get_le16val(dp + 17);
  calibVals[9] = gyro_speed_plus = get_le16val(dp + 19);
  calibVals[10] = gyro_speed_minus = get_le16val(dp + 21);
  calibVals[11] = acc_x_plus = get_le16val(dp + 23);
  calibVals[12] = acc_x_minus = get_le16val(dp + 25);
  calibVals[13] = acc_y_plus = get_le16val(dp + 27);
  calibVals[14] = acc_y_minus = get_le16val(dp + 29);
  calibVals[15] = acc_z_plus = get_le16val(dp + 31);
  calibVals[16] = acc_z_minus = get_le16val(dp + 33);
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
}

// Set AHRS algorithm settings
static const FusionAhrsSettings settings = {
    .gain = 0.5f,
    .accelerationRejection = 10.0f,
    .rejectionTimeout = 10 * SAMPLE_RATE,
};

static void DualSenseSetup(USBH_ClassTypeDef *pclass)
{
  USBH_StatusTypeDef st;
  uint32_t *vph, *vpf;

  setup_fusion(SAMPLE_RATE, &settings);

  memset(firmdata, 0, DS_FEATURE_REPORT_FIRMWARE_INFO_SIZE);
  st = USBH_HID_GetReport(pclass->phost, 3, DS_FEATURE_REPORT_FIRMWARE_INFO, firmdata, DS_FEATURE_REPORT_FIRMWARE_INFO_SIZE);
  (void) st;
  vph = (uint32_t *)&firmdata[0x1c];
  vpf = (uint32_t *)&firmdata[0x2c];
  debug_printf("hwv: %x, swv: %x\n", *vph, *vpf);
  osDelay(5);
  memset(calibdata, 0, DS_FEATURE_REPORT_CALIBRATION_SIZE);
  st = USBH_HID_GetReport(pclass->phost, 3, DS_FEATURE_REPORT_CALIBRATION, calibdata, DS_FEATURE_REPORT_CALIBRATION_SIZE);

  process_calibdata(&DsData, calibdata);

  initial_report = 0;
  set_joystick_params();
}

static void DualSenseResetFusion()
{
  gamepad_reset_fusion();
}

static USBH_StatusTypeDef DualSenseInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) pclass->pData;

  if (HID_Handle->length > sizeof(dual_rx_report_buf))
  {
    HID_Handle->length = sizeof(dual_rx_report_buf);
  }
  HID_Handle->pData = HID_Handle->report.ptr = (uint8_t *)(void *)dual_rx_buf;
  USBH_HID_FifoInit(&HID_Handle->fifo, phost->device.Data, sizeof(dual_rx_report_buf));

  postGuiEventMessage(GUIEV_USB_AUDIO_READY, 0, (void *)&DualSenseUsbAudio, NULL);
  return USBH_OK;
}

static void dualsense_process_fusion(struct dualsense_input_report *rp)
{
  int i;
  struct ds_data *ds = &DsData;
  FusionVector gyroscope;
  FusionVector accelerometer;
  int raw_data;
  float calib_data;

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

  gamepad_process_fusion(SAMPLE_PERIOD, gyroscope, accelerometer);
}

static uint8_t prev_blevel;

static void DualSenseBtDisconnect()
{
  prev_blevel = 0;
}

/*
 * Decode DualSense Input report
 */
static void decode_report(struct dualsense_input_report *rp, int hid_mode)
{
  uint8_t hat;
  uint32_t vbutton;
  uint8_t blevel;

  hat = (rp->buttons[0] & 0x0f);
  vbutton = (rp->buttons[2] & 0x0f) << 12;	/* PS, Touch, Mute */
  vbutton |= rp->buttons[1] << 4;		/* L1, R1, L2, R2, L3, R3, Create, Option */
  vbutton |= (rp->buttons[0] & 0xf0)>> 4;	/* Square, Cross, Circle, Triangle */
  vbutton |= hatmap[hat];

  HidProcTable[hid_mode](rp, hat, vbutton);

  if (rp->battery_level != prev_blevel)
  {
    blevel = rp->battery_level & 0x0F;
    debug_printf("Battery: 0x%02x\n", blevel);
    if (blevel > 9) blevel = 9;
    blevel = blevel / 2;
    postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_BATTERY | blevel, NULL, NULL);
    prev_blevel = rp->battery_level;
  }
}

static void process_bt_reports(uint8_t hid_mode);

static void DualSenseDecodeInputReport(HID_REPORT *report)
{
  struct dualsense_input_report *rp;

  switch (report->len)
  {
  case DS_INPUT_REPORT_BT_SIZE:
    report->ptr += 2;
    rp = (struct dualsense_input_report *)report->ptr;
    break;
  case DS_INPUT_REPORT_USB_SIZE:
    /*Fill report */
    rp = &cur_report;
    SCB_InvalidateDCache_by_Addr((uint32_t *)report->ptr, DS_INPUT_REPORT_USB_SIZE);
    memcpy(&cur_report, report->ptr, DS_INPUT_REPORT_USB_SIZE);
    break;
  default:
    return;
  }

  if (report->hid_mode != HID_MODE_DOOM)
    dualsense_process_fusion(rp);

  switch (rp->report_id)
  {
  case DS_INPUT_REPORT_USB:	/* USB */
    if ((rp->seq_number & 0x07) == 0)
      decode_report(rp, report->hid_mode);
    break;
  case DS_INPUT_REPORT_BT:	/* Bluetooth */
    decode_report(rp, report->hid_mode);
    process_bt_reports(report->hid_mode);
    break;
  default:
    break;
  }
}

static uint8_t bt_seq;

static void bt_out_init(struct dualsense_btout_report *rp)
{
  memset(rp, 0, sizeof(*rp));
  rp->report_id = DS_OUTPUT_REPORT_BT;
  rp->tag = DS_OUTPUT_TAG;
  rp->seq_tag = bt_seq << 4;
  bt_seq++;
  if (bt_seq >= 16)
    bt_seq = 0;
}

static void bt_emit_report(struct dualsense_btout_report *brp)
{
  int len;

  len = sizeof(*brp);
  brp->crc = bt_comp_crc((uint8_t *)brp, len);
  btapi_send_report((uint8_t *)brp, len);
  btapi_push_report();
}

void DualSenseReleaseACLBuffer(uint8_t *bp)
{
  osMessageQueuePut(btreportqId, &bp, 0, 0);
}

void DualSenseProcessCalibReport(const uint8_t *bp, int len)
{
  if (len == DS_FEATURE_REPORT_CALIBRATION_SIZE)
  {
    process_calibdata(&DsData, (uint8_t *)bp);

    initial_report = 0;
    set_joystick_params();
  }
}

static void SetBarColor(DS_OUTPUT_REPORT *rp, int seq, int mode)
{
  uint8_t cval[3];

  if (seq & 1)
  {
    rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE;

    if (mode != HID_MODE_DOOM)
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
    if (mode != HID_MODE_DOOM)
    {
      int vval;

      vval = fft_getcolor(cval);
      if (vval > 250)
        rp->player_leds = 0x1f;		// Turn on all LEDs
      else if (vval > 150)
        rp->player_leds = 0x0e;
      else if (vval > 20)
        rp->player_leds = 0x04;		// Center only
      else
        rp->player_leds = 0;		// Turn off all LEDs
    }
    else
    {
      rp->player_leds = (1 << 2);
    }
  }
}

static int BtUpdateBarColor(uint8_t hid_mode)
{
  struct dualsense_btout_report *brp;

  if (osMessageQueueGet(btreportqId, &brp, 0, 0) != osOK)
    return 0;
  bt_out_init(brp);
  brp->report_id = DS_OUTPUT_REPORT_BT;

  SetBarColor(&brp->com_report, bt_seq, hid_mode);

  bt_emit_report(brp);
  return 1;
}

static void fill_output_request(int init_num, DS_OUTPUT_REPORT *rp)
{
  switch (init_num)
  {
    case 0:
      rp->valid_flag2 = DS_OUTPUT_VALID_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE;
      rp->lightbar_setup = DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT;
      break;
    case 1:
      rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_PLAYER_INDICATOR_CONTROL_ENABLE;
      rp->player_leds = 1 | (1 << 3);
      break;
    case 2:
      rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE;
      rp->lightbar_red = 0;
      rp->lightbar_green = 255;
      rp->lightbar_blue = 0;
      rp->led_brightness = 255;
      break;
    case 3:
      TriggerFeedbackSetup(rp->RightTriggerFFB,  3, 3);
      TriggerFeedbackSetup(rp->LeftTriggerFFB,  3, 3);
      rp->valid_flag0 = VALID_FLAG0_RIGHT_TRIGGER | VALID_FLAG0_LEFT_TRIGGER;
      break;
    default:
      break;
  }
}

static void process_bt_reports(uint8_t hid_mode)
{
  struct dualsense_btout_report *brp;
  DS_OUTPUT_REPORT *rp;

  if (initial_report < 4)
  {
    osMessageQueueGet(btreportqId, &brp, 0, osWaitForever);
    bt_out_init(brp);
    rp = &brp->com_report;

    fill_output_request(initial_report, rp);
    initial_report++;

    bt_emit_report(brp);
  }
  if (BtUpdateBarColor(hid_mode))
      btapi_push_report();
}

/*
 * Prepare output report to change bar color and status LED
 */
static void DualSenseGetOutputReport(uint8_t **ptr, int *plength, int hid_mode, uint8_t uc)
{
  struct dualsense_usbout_report *urp = &out_report;

  urp->report_id = DS_OUTPUT_REPORT_USB;

  if (initial_report < 4)
  {
    fill_output_request(initial_report, &urp->com_report);
    initial_report++;
  }
  else
  {
    SetBarColor(&urp->com_report, uc, hid_mode);
  }
  *ptr = (uint8_t *)urp;
  *plength = sizeof(out_report);
}

static uint32_t last_button;
static int pad_timer;
static int16_t left_xinc, left_yinc;
static int16_t right_xinc, right_yinc;

/**
 * @brief Convert HID input report to LVGL kaycode
 */
static void DualSense_LVGL_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton)
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
#ifdef USE_PAD_TIMER
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
#endif
  }
  ix = rp->x - 128;
  iy = rp->y - 128;
  ax = (ix < 0)? -ix : ix;
  ay = (iy < 0)? -iy : iy;
  ax = (ax > 80) ? 1 : 0;
  ay = (ay > 80) ? 1 : 0;

  if (ax != 0)
  {
    if (ix < 0) ax = -1;
    if (ax != left_xinc)
      postGuiEventMessage(GUIEV_LEFT_XDIR, ax, NULL, NULL);
  }
  left_xinc = ax;

  if (ay != 0)
  {
    if (iy < 0) ay = -1;
    if (ay != left_yinc)
      postGuiEventMessage(GUIEV_LEFT_YDIR, ay, NULL, NULL);
  }
  left_yinc = ay;

  ix = rp->rx - 128;
  iy = rp->ry - 128;
  ax = (ix < 0)? -ix : ix;
  ay = (iy < 0)? -iy : iy;
  ax = (ax > 80) ? 1 : 0;
  ay = (ay > 80) ? 1 : 0;

  if (ax != 0)
  {
    if (ix < 0) ax = -1;
    if (ax != right_xinc)
      postGuiEventMessage(GUIEV_RIGHT_XDIR, ax, NULL, NULL);
  }
  right_xinc = ax;

  if (ay != 0)
  {
    if (iy < 0) ay = -1;
    if (ay != right_yinc)
      postGuiEventMessage(GUIEV_RIGHT_YDIR, ay, NULL, NULL);
  }
  right_yinc = ay;
}

void DualSenseBtSetup(uint16_t hid_host_cid)
{
  uint8_t *ptr;
  int i;

  setup_fusion(SAMPLE_RATE, &settings);

  hid_host_send_get_report(hid_host_cid, HID_REPORT_TYPE_FEATURE, DS_FEATURE_REPORT_CALIBRATION);

  btreportqId = osMessageQueueNew(BTREPQ_DEPTH, sizeof(struct dualsense_bt_output_report *), &attributes_reportq);

  for (i = 0; i < BTREPQ_DEPTH; i++)
  {
    ptr = (uint8_t *)&reportBuffer[i];
    if (osMessageQueuePut(btreportqId, &ptr, 0, 0) != osOK)
      debug_printf("Queue put failed.\n");
  }
}

static const uint8_t sdl_hatmap[16] = {
  SDL_HAT_UP,       SDL_HAT_RIGHTUP,   SDL_HAT_RIGHT,    SDL_HAT_RIGHTDOWN,
  SDL_HAT_DOWN,     SDL_HAT_LEFTDOWN,  SDL_HAT_LEFT,     SDL_HAT_LEFTUP,
  SDL_HAT_CENTERED, SDL_HAT_CENTERED,  SDL_HAT_CENTERED, SDL_HAT_CENTERED,
  SDL_HAT_CENTERED, SDL_HAT_CENTERED,  SDL_HAT_CENTERED, SDL_HAT_CENTERED,
};

static void DualSense_DOOM_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton)
{
  SDL_JoyStickSetButtons(sdl_hatmap[hat], vbutton & 0x7FFF);
}

static struct gamepad_inputs dualsense_inputs;

static void DualSense_Display_Status(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton)
{
  struct gamepad_inputs *gin = &dualsense_inputs;

  gin->x = rp->x;
  gin->y = rp->y;
  gin->z = rp->z;
  gin->rx = rp->rx;
  gin->ry = rp->ry;
  gin->rz = rp->rz;
  gin->vbutton = vbutton;
  memcpy(gin->gyro, rp->gyro, sizeof(int16_t) * 3);
  memcpy(gin->accel, rp->accel, sizeof(int16_t) * 3);
  gin->points[0].contact = rp->points[0].contact;
  gin->points[0].xpos = (rp->points[0].x_hi << 8) | (rp->points[0].x_lo);
  gin->points[0].ypos = (rp->points[0].y_hi << 4) | (rp->points[0].y_lo);
  gin->points[1].contact = rp->points[1].contact;
  gin->points[1].xpos = (rp->points[1].x_hi << 8) | (rp->points[1].x_lo);
  gin->points[1].ypos = (rp->points[1].y_hi << 4) | (rp->points[1].y_lo);

  Display_GamePad_Info(gin, vbutton);
}

const struct sGamePadDriver DualSenseDriver = {
  DualSenseInit,			// USB
  DualSenseSetup,			// USB
  DualSenseGetOutputReport,		// USB
  DualSenseDecodeInputReport,		// USB and BT
  DualSenseBtSetup,			// BT
  DualSenseProcessCalibReport,		// BT
  DualSenseReleaseACLBuffer,		// BT
  DualSenseResetFusion,			// USB and BT
  DualSenseBtDisconnect,		// BT
};
