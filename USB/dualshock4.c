/**
 * @brief SONY DUALSHOCK 4 Controller driver
 */
#include "DoomPlayer.h"
#include "Fusion.h"
#include "lvgl.h"
#include "app_gui.h"
#include "app_task.h"
#include "dualshock4_report.h"
#include "gamepad.h"
#include "classic/hid_host.h"
#include "SDL.h"
#include "SDL_joystick.h"
#include "audio_output.h"

#define	REPORT_SIZE	sizeof(struct ds4_input_report)

SECTION_USBSRAM uint8_t ds4calibdata[DS4_FEATURE_REPORT_CALIBRATION_SIZE+4];
SECTION_USBSRAM uint8_t ds4_rx_buf[REPORT_SIZE+4];
SECTION_USBSRAM uint8_t ds4_rx_report_buf[REPORT_SIZE*5];
SECTION_USBSRAM DS4_INPUT_REPORT ds4_cur_report;
SECTION_USBSRAM struct ds4_usb_output_report ds4_out_report;
SECTION_USBSRAM struct ds4_bt_output_report bt_out_report[2];

#define	SAMPLE_PERIOD	(0.004f)
#define	SAMPLE_RATE	(250)

extern int fft_getcolor(uint8_t *p);
extern void GetPlayerHealthColor(uint8_t *cval);

static void DS4_LVGL_Keycode(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep);
static void DualShock_DOOM_Keycode(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep);
static void DualShock_Display_Status(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep);

static const void (*ds4HidProcTable[])(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep) = {
      DS4_LVGL_Keycode,
      DualShock_Display_Status,
      DualShock_DOOM_Keycode,
};

extern SDL_Joystick *SDL_GetJoystickPtr();

const AUDIO_DEVCONF DualShockUsbAudio = {
  .mix_mode = MIXER_USB_OUTPUT|MIXER_FFT_ENABLE,
  .playRate = 32000,
  .numChan = 2,
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

static int out_index;

#define	le16_to_cpu(x)	(x)

static int16_t get_le16val(uint8_t *bp)
{
  int16_t val;

  val = bp[0] + (bp[1] << 8);
  return val;
}

static int16_t calibVals[17];

static void process_calibdata(struct ds_data *ds, uint8_t *dp, int btmode)
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

  if (btmode == 0)
  {
    calibVals[3] = gyro_pitch_plus = get_le16val(dp + 7);
    calibVals[4] = gyro_pitch_minus = get_le16val(dp + 9);
    calibVals[5] = gyro_yaw_plus = get_le16val(dp + 11);
    calibVals[6] = gyro_yaw_minus = get_le16val(dp + 13);
    calibVals[7] = gyro_roll_plus = get_le16val(dp + 15);
    calibVals[8] = gyro_roll_minus = get_le16val(dp + 17);
  }
  else
  {
    calibVals[3] = gyro_pitch_plus = get_le16val(dp + 7);
    calibVals[4] = gyro_pitch_minus = get_le16val(dp + 13);
    calibVals[5] = gyro_yaw_plus = get_le16val(dp + 9);
    calibVals[6] = gyro_yaw_minus = get_le16val(dp + 15);
    calibVals[7] = gyro_roll_plus = get_le16val(dp + 11);
    calibVals[8] = gyro_roll_minus = get_le16val(dp + 17);
  }

  calibVals[9] = gyro_speed_plus = get_le16val(dp + 19);
  calibVals[10] = gyro_speed_minus = get_le16val(dp + 21);
  calibVals[11] = acc_x_plus = get_le16val(dp + 23);
  calibVals[12] = acc_x_minus = get_le16val(dp + 25);
  calibVals[13] = acc_y_plus = get_le16val(dp + 27);
  calibVals[14] = acc_y_minus = get_le16val(dp + 29);
  calibVals[15] = acc_z_plus = get_le16val(dp + 31);
  calibVals[16] = acc_z_minus = get_le16val(dp + 33);

  /*
   * Set gyroscope calibration and normalization parameters.
   * Data values will be normalized to 1/DS4_GYRO_RES_PER_DEG_S degree/s.
   */

  flNumerator = (gyro_speed_plus + gyro_speed_minus) * DS4_GYRO_RES_PER_DEG_S;

  ds->gyro_calib_data[0].bias = gyro_pitch_bias;
  ds->gyro_calib_data[0].sensitivity = flNumerator / (gyro_pitch_plus - gyro_pitch_minus);

  ds->gyro_calib_data[1].bias = gyro_yaw_bias;
  ds->gyro_calib_data[1].sensitivity = flNumerator / (gyro_yaw_plus - gyro_yaw_minus);

  ds->gyro_calib_data[2].bias = gyro_roll_bias;
  ds->gyro_calib_data[2].sensitivity = flNumerator / (gyro_roll_plus - gyro_roll_minus);

  /*
   * Set accelerometer calibration and normalization parameters.
   * Data values will be normalized to 1/DS4_ACC_RES_PER_G g.
   */
  range_2g = acc_x_plus - acc_x_minus;
  ds->accel_calib_data[0].bias = acc_x_plus - range_2g / 2;
  ds->accel_calib_data[0].sensitivity = 2.0f * DS4_ACC_RES_PER_G / (float)range_2g;

  range_2g = acc_y_plus - acc_y_minus;
  ds->accel_calib_data[1].bias = acc_y_plus - range_2g / 2;
  ds->accel_calib_data[1].sensitivity = 2.0f * DS4_ACC_RES_PER_G / (float)range_2g;

  range_2g = acc_z_plus - acc_z_minus;
  ds->accel_calib_data[2].bias = acc_z_plus - range_2g / 2;
  ds->accel_calib_data[2].sensitivity = 2.0f * DS4_ACC_RES_PER_G / (float)range_2g;
}

static void set_joystick_params()
{
  SDL_Joystick *joystick = SDL_GetJoystickPtr();

  joystick->name = "DualShock";
  joystick->naxes = 1;
  joystick->nhats = 1;
  joystick->nbuttons = 13;
  joystick->hats = 0;
  joystick->buttons = 0;;
}

// Set AHRS algorithm settings
static const FusionAhrsSettings settings = {
    .gain = 0.5f,
    .accelerationRejection = 10.0f,
    .rejectionTimeout = 10 * SAMPLE_RATE,
};

static void DualShockResetFusion()
{
  gamepad_reset_fusion();
}

static void DualShockSetup(USBH_ClassTypeDef *pclass)
{
  USBH_StatusTypeDef st;

  setup_fusion(SAMPLE_RATE, &settings);

  memset(ds4calibdata, 0, sizeof(ds4calibdata));
  st = USBH_HID_GetReport(pclass->phost, 3, DS4_FEATURE_REPORT_CALIBRATION, ds4calibdata, DS4_FEATURE_REPORT_CALIBRATION_SIZE);

  (void) st;
  process_calibdata(&DsData, ds4calibdata, 0);

  set_joystick_params();
}

static USBH_StatusTypeDef DualShockInit(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) pclass->pData;

  if (HID_Handle->length > sizeof(ds4_rx_report_buf))
  {
    HID_Handle->length = sizeof(ds4_rx_report_buf);
  }
  HID_Handle->pData = HID_Handle->report.ptr = (uint8_t *)(void *)ds4_rx_buf;
  USBH_HID_FifoInit(&HID_Handle->fifo, phost->device.Data, sizeof(ds4_rx_report_buf));

  postGuiEventMessage(GUIEV_USB_AUDIO_READY, 0, (void *)&DualShockUsbAudio, NULL);
  return USBH_OK;
}

static void ds4_process_fusion(struct ds4_input_report *rp)
{
  int i;
  struct ds_data *ds = &DsData;
  FusionVector gyroscope;
  FusionVector accelerometer;
  int raw_data;
  float calib_data;

#define DO_CALIB
  for (i = 0; i < 3; i++)
  {
      raw_data = le16_to_cpu(rp->gyro[i]);
#ifdef DO_CALIB
      calib_data = (float)(raw_data - ds->gyro_calib_data[i].bias) * ds->gyro_calib_data[i].sensitivity;
#else
      calib_data = raw_data;
#endif
#ifdef SWAP_ZY
      switch (i)
      {
      case 0:
        gyroscope.array[0] = calib_data / DS4_GYRO_RES_PER_DEG_S;
        break;
      case 1:
        gyroscope.array[2] = calib_data / DS4_GYRO_RES_PER_DEG_S;
        break;
      case 2:
      default:
        gyroscope.array[1] = calib_data / DS4_GYRO_RES_PER_DEG_S;
        break;
      }
#else
      gyroscope.array[i] = calib_data / DS4_GYRO_RES_PER_DEG_S;
#endif
  }
  for (i = 0; i < 3; i++)
  {
      raw_data = le16_to_cpu(rp->accel[i]);
#ifdef DO_CALIB
      calib_data = (float)(raw_data - ds->accel_calib_data[i].bias) * ds->accel_calib_data[i].sensitivity;
#else
      calib_data = raw_data;
#endif
#ifdef SWAP_XY
      switch (i)
      {
      case 0:
        accelerometer.array[0] = calib_data / DS4_ACC_RES_PER_G;
        break;
      case 1:
        accelerometer.array[2] = calib_data / DS4_ACC_RES_PER_G;
        break;
      case 2:
      default:
        accelerometer.array[1] = calib_data / DS4_ACC_RES_PER_G;
        break;
      }
#else
      accelerometer.array[i] = calib_data / DS4_ACC_RES_PER_G;
#endif
  }

  gamepad_process_fusion(SAMPLE_PERIOD, gyroscope, accelerometer);
}

void process_output_report(int hid_mode)
{
  struct ds4_bt_output_report *brp;
  struct ds4_output_report *rp;
  uint8_t cval[3];

  brp = &bt_out_report[out_index];
  memset(brp, 0, sizeof(*brp));
  brp->report_id = DS4_OUTPUT_REPORT_BT;
  brp->hw_control = DS4_OUTPUT_HWCTL_HID | DS4_OUTPUT_HWCTL_CRC32;
  rp = &brp->out_report;

  out_index ^= 1;

  brp->report_id = DS4_OUTPUT_REPORT_BT;
  rp->valid_flag0 = DS4_OUTPUT_VALID_FLAG0_LED;

  if (hid_mode != HID_MODE_DOOM)
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

  int len;

  len = sizeof(struct ds4_bt_output_report);
  brp->crc = bt_comp_crc((uint8_t *)brp, len);
  btapi_send_report((uint8_t *)brp, len);
  btapi_push_report();
}

static uint8_t prev_blevel;
static uint16_t prev_stamp;

static void DualShockBtDisconnect()
{
  prev_blevel = 0;
}

/*
 * Decode DualShock Input report
 */
static void DualShockDecodeInputReport(HID_REPORT *report)
{
  DS4_INPUT_REPORT *rp;
  static uint32_t in_seq;
  uint8_t blevel;
  uint8_t ctype;
  int tdiff;

  switch (report->len)
  {
  case DS4_INPUT_REPORT_USB_SIZE:
    break;
  case DS4_INPUT_REPORT_BT_SIZE:
    report->ptr++;
    break;
  default:
    return;
  }

  ctype = report->ptr[0];

  switch (ctype)
  {
  case DS4_INPUT_REPORT_USB:
    /*Fill report */
    SCB_InvalidateDCache_by_Addr((uint32_t *)report->ptr, REPORT_SIZE);
    memcpy(&ds4_cur_report, report->ptr + 1, sizeof(DS4_INPUT_REPORT));
    break;
  case DS4_INPUT_REPORT_BT:
    memcpy(&ds4_cur_report, report->ptr + 3, sizeof(DS4_INPUT_REPORT));
    break;
  default:
    return;
  }
  rp = &ds4_cur_report;
 
  if (rp->timestamp == prev_stamp)
    return;
  tdiff = rp->timestamp - prev_stamp;
  if (tdiff < 0) tdiff += 65536;

  if ((ctype == DS4_INPUT_REPORT_BT) && (tdiff < 1000))
  {
    return;
  }
  prev_stamp = rp->timestamp;

  if (report->hid_mode != HID_MODE_DOOM)
    ds4_process_fusion(rp);

  {
    uint8_t hat;
    uint32_t vbutton;

    hat = (rp->buttons[0] & 0x0f);
    vbutton = (rp->buttons[2] & 0x03) << 12;	/* Home, Pad */
    vbutton |= rp->buttons[1] << 4;		/* L1, R1, L2, R2, Share, Option, L3, R3 */
    vbutton |= (rp->buttons[0] & 0xf0)>> 4;	/* Square, Cross, Circle, Triangle */
    vbutton |= hatmap[hat];

    ds4HidProcTable[report->hid_mode](rp, hat, vbutton, report);
  }

  if ((rp->status[0] & 0x0F) != prev_blevel)
  {
    blevel = rp->status[0] & 0x0F;
    if (blevel > 9) blevel = 9;
    blevel = blevel / 2;
debug_printf("Battery: 0x%02x, %d\n", rp->status[0], blevel);
    postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_BATTERY | blevel, NULL, NULL);
    prev_blevel = rp->status[0] & 0x0F;
  }

  if (report->ptr[0] == DS4_INPUT_REPORT_BT)
  {
#define ENABLE_OUTPUT_REPORT
#ifdef ENABLE_OUTPUT_REPORT
    if ((in_seq & 3) == 0)
      process_output_report(report->hid_mode);
#endif
    in_seq++;
  }
}

static void DualShockGetOutputReport(uint8_t **ptr, int *plength, int hid_mode, uint8_t uc)
{
  struct ds4_output_report *rp = &ds4_out_report.out_report;
  uint8_t cval[3];

  ds4_out_report.report_id = DS4_OUTPUT_REPORT_USB;
  rp->valid_flag0 = DS4_OUTPUT_VALID_FLAG0_LED;

  if (hid_mode != HID_MODE_DOOM)
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
  *ptr = (uint8_t *)rp;
  *plength = sizeof(ds4_out_report);
}

static uint32_t last_button;
static int pad_timer;
static int16_t left_xinc, left_yinc;
static int16_t right_xinc, right_yinc;

/**
 * @brief Convert HID input report to LVGL kaycode
 */
static void DS4_LVGL_Keycode(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep)
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
    {
      postGuiEventMessage(GUIEV_LEFT_XDIR, ax, NULL, NULL);
    }
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

static const uint8_t sdl_hatmap[16] = {
  SDL_HAT_UP,       SDL_HAT_RIGHTUP,   SDL_HAT_RIGHT,    SDL_HAT_RIGHTDOWN,
  SDL_HAT_DOWN,     SDL_HAT_LEFTDOWN,  SDL_HAT_LEFT,     SDL_HAT_LEFTUP,
  SDL_HAT_CENTERED, SDL_HAT_CENTERED,  SDL_HAT_CENTERED, SDL_HAT_CENTERED,
  SDL_HAT_CENTERED, SDL_HAT_CENTERED,  SDL_HAT_CENTERED, SDL_HAT_CENTERED,
};

static void DualShock_DOOM_Keycode(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep)
{
  SDL_JoyStickSetButtons(sdl_hatmap[hat], vbutton & 0x7FFF);
}

void DualShockBtSetup(uint16_t hid_host_cid)
{
  memset(ds4calibdata, 0, sizeof(ds4calibdata));
  hid_host_send_get_report(hid_host_cid, HID_REPORT_TYPE_FEATURE, DS4_FEATURE_REPORT_CALIBRATION_BT);
  set_joystick_params();

  out_index = 0;
}

void DualShockBtProcessCalibReport(const uint8_t *bp, int len)
{
  if (len == DS4_FEATURE_REPORT_CALIBRATION_SIZE+4)
  {
    memcpy(ds4calibdata, bp, DS4_FEATURE_REPORT_CALIBRATION_SIZE + 4);
    process_calibdata(&DsData, ds4calibdata, 1);
  }
  setup_fusion(SAMPLE_RATE, &settings);
  //SDL_JoystickOpen(0);
}

void DualShockReleaseACLBuffer()
{
}

static struct gamepad_inputs ds4_inputs;

static void DualShock_Display_Status(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep)
{
  struct gamepad_inputs *gin = &ds4_inputs;
  struct ds4_usb_input_report *usb_rep;
  struct ds4_bt_input_report *bt_rep;
  struct ds4_touch_report *tp;
  int num_report;

  gin->x = rp->x;
  gin->y = rp->y;
  gin->z = rp->z;
  gin->rx = rp->rx;
  gin->ry = rp->ry;
  gin->rz = rp->rz;
  gin->vbutton = vbutton;
  memcpy(gin->gyro, rp->gyro, sizeof(int16_t) * 3);
  memcpy(gin->accel, rp->accel, sizeof(int16_t) * 3);
//debug_printf("accel: %d, %d, %d\n", gin->accel[0], gin->accel[1], gin->accel[2]);
//debug_printf("gyro: %d, %d, %d\n", gin->gyro[0], gin->gyro[1], gin->gyro[2]);

  if (rep->ptr[0] == DS4_INPUT_REPORT_USB)
  {
    usb_rep = (struct ds4_usb_input_report *)rep->ptr;
    num_report = usb_rep->num_touch_reports;
    tp = usb_rep->reports;
  }
  else if (rep->ptr[0] == DS4_INPUT_REPORT_BT)
  {
    bt_rep = (struct ds4_bt_input_report *)rep->ptr;
    num_report = bt_rep->num_touch_reports;
    tp = bt_rep->reports;
  }
  else
    return;

  if (num_report > 0)
  {
    gin->points[0].contact = tp->points[0].contact;
    gin->points[0].xpos = (tp->points[0].x_hi << 8) | (tp->points[0].x_lo);
    gin->points[0].ypos = (tp->points[0].y_hi << 4) | (tp->points[0].y_lo);
    gin->points[1].contact = tp->points[1].contact;
    gin->points[1].xpos = (tp->points[1].x_hi << 8) | (tp->points[1].x_lo);
    gin->points[1].ypos = (tp->points[1].y_hi << 4) | (tp->points[1].y_lo);
  }
  else
  {
    gin->points[0].contact = 0x80;
    gin->points[1].contact = 0x80;
  }
  Display_GamePad_Info(gin, vbutton);
}

const struct sGamePadDriver DualShockDriver = {
  DualShockInit,			// USB
  DualShockSetup,			// USB
  DualShockGetOutputReport,		// USB
  DualShockDecodeInputReport,		// USB and BT
  DualShockBtSetup,			// BT
  DualShockBtProcessCalibReport,	// BT
  DualShockReleaseACLBuffer,		// BT
  DualShockResetFusion,			// USB and BT
  DualShockBtDisconnect,		// BT
};
