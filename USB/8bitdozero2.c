/**
 * @brief 8BitDo Zero 2 Controller driver
 *
 * This controller supports BT interface only.
 * Does NOT support USB connection.
 */
#include "DoomPlayer.h"
#include "lvgl.h"
#include "app_gui.h"
#include "app_task.h"
#include "gamepad.h"
#include "classic/hid_host.h"
#include "SDL.h"
#include "SDL_joystick.h"

/*
 * button[0] : Left & Right
 *             0x00: Left
 *             0x80: Not pressed
 *             0xFF: Right
 *
 * button[1] : Up & Down
 *             0x00: Up
 *             0x80: Not pressed
 *             0xFF: Down
 *
 * button[4] : A, B, X, Y
 *             0x08: Not pressed
 *             0x18: Y
 *             0x28: B
 *             0x48: A
 *             0x88: X
 *
 * button[5] : L1, R1, Select, Start
 *             0x00: Not pressed
 *             0x01: L1
 *             0x02: R1
 *             0x10: Select
 *             0x20: Start
 */

struct zero2_input_report {
  uint8_t buttons[8];
} __attribute__((packed));

typedef struct zero2_input_report ZERO2_INPUT_REPORT;

#define	REPORT_SIZE	sizeof(ZERO2_INPUT_REPORT)

SECTION_USBSRAM ZERO2_INPUT_REPORT zero2_cur_report;
SECTION_USBSRAM ZERO2_INPUT_REPORT zero2_prev_report;

static void Zero2_LVGL_Keycode(struct zero2_input_report *rp, uint32_t vbutton, HID_REPORT *rep);
static void Zero2_DOOM_Keycode(struct zero2_input_report *rp, uint32_t vbutton, HID_REPORT *rep);
static void Zero2_Display_Status(struct zero2_input_report *rp, uint32_t vbutton, HID_REPORT *rep);

static const void (*zero2HidProcTable[])(struct zero2_input_report *rp, uint32_t vbutton, HID_REPORT *rep) = {
      Zero2_LVGL_Keycode,
      Zero2_Display_Status,
      Zero2_DOOM_Keycode,
};

extern SDL_Joystick *SDL_GetJoystickPtr();

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

static void set_joystick_params()
{
  SDL_Joystick *joystick = SDL_GetJoystickPtr();

  joystick->name = "8BitDo Zero2";
  joystick->naxes = 1;
  joystick->nhats = 1;
  joystick->nbuttons = 8;
  joystick->hats = 0;
  joystick->buttons = 0;;
}

static void Zero2ResetFusion()
{
}

static void Zero2BtDisconnect()
{
}

/*
 * Decode Zero2 Input report
 */
static void Zero2DecodeInputReport(HID_REPORT *report)
{
  ZERO2_INPUT_REPORT *rp;

  report->ptr++;

  if (report->ptr[0] == 0x01)
  {

    SCB_InvalidateDCache_by_Addr((uint32_t *)report->ptr, REPORT_SIZE+2);
    memcpy(&zero2_cur_report, report->ptr + 1, REPORT_SIZE);

    rp = &zero2_cur_report;
 
    if (memcmp(&zero2_prev_report, &zero2_cur_report, REPORT_SIZE))
    {
      uint32_t vbutton = 0;

#ifdef DEBUG_8BIT
      debug_printf("Buttons: %02x %02x %02x %02x", rp->buttons[0], rp->buttons[1], rp->buttons[2], rp->buttons[3]);
      debug_printf(" %02x %02x %02x %02x\n", rp->buttons[4], rp->buttons[5], rp->buttons[6], rp->buttons[7]);
#endif
      memcpy(&zero2_prev_report, &zero2_cur_report, REPORT_SIZE);

      if (rp->buttons[0] == 0x00)
        vbutton |= VBMASK_LEFT;
      else if (rp->buttons[0] == 0xff)
        vbutton |= VBMASK_RIGHT;

      if (rp->buttons[1] == 0x00)
        vbutton |= VBMASK_UP;
      else if (rp->buttons[1] == 0xff)
        vbutton |= VBMASK_DOWN;

      if (rp->buttons[4] & 0x80)
        vbutton |= VBMASK_TRIANGLE;
      if (rp->buttons[4] & 0x40)
        vbutton |= VBMASK_SQUARE;
      if (rp->buttons[4] & 0x20)
        vbutton |= VBMASK_CROSS;
      if (rp->buttons[4] & 0x10)
        vbutton |= VBMASK_CIRCLE;

      if (rp->buttons[5] & 0x01)
        vbutton |= VBMASK_L1;
      if (rp->buttons[5] & 0x02)
        vbutton |= VBMASK_R1;
      if (rp->buttons[5] & 0x10)
        vbutton |= VBMASK_SHARE;
      if (rp->buttons[5] & 0x20)
        vbutton |= VBMASK_PS;

      zero2HidProcTable[report->hid_mode](rp, vbutton, report);
    }
  }
}

static uint32_t last_button;
static int pad_timer;

/**
 * @brief Convert HID input report to LVGL kaycode
 */
static void Zero2_LVGL_Keycode(struct zero2_input_report *rp, uint32_t vbutton, HID_REPORT *rep)
{
  static lv_indev_data_t pad_data;

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
}

static const uint8_t sdl_hatmap[16] = {
  SDL_HAT_CENTERED, SDL_HAT_LEFTDOWN,  SDL_HAT_RIGHTDOWN, SDL_HAT_CENTERED,
  SDL_HAT_UP,       SDL_HAT_LEFTUP,    SDL_HAT_RIGHTUP,   SDL_HAT_UP,
  SDL_HAT_DOWN,     SDL_HAT_LEFTDOWN,  SDL_HAT_RIGHTDOWN, SDL_HAT_DOWN,
  SDL_HAT_CENTERED, SDL_HAT_LEFT,      SDL_HAT_RIGHT,     SDL_HAT_CENTERED
};

static void Zero2_DOOM_Keycode(struct zero2_input_report *rp, uint32_t vbutton, HID_REPORT *rep)
{
  uint8_t hatcode;

  hatcode =  ((rp->buttons[0] & 0xC0) ^ 0x40) >> 6;
  hatcode |= ((rp->buttons[1] & 0xC0) ^ 0x40) >> 4;
  hatcode &= 0x0F;
/*
00 -> 01
10 -> 11
11 -> 10
 *             0x01: Left
 *             0x03: Not pressed
 *             0x02: Right
 *             0x04: Up
 *             0x0C: Not pressed
 *             0x08: Down
 */
  SDL_JoyStickSetButtons(sdl_hatmap[hatcode], vbutton & 0x7FFF);
}

void Zero2BtSetup(uint16_t hid_host_cid)
{
  set_joystick_params();
}

void Zero2BtProcessCalibReport(const uint8_t *bp, int len)
{
}

void Zero2ReleaseACLBuffer()
{
}

static void Zero2_Display_Status(struct zero2_input_report *rp, uint32_t vbutton, HID_REPORT *rep)
{
}

const struct sGamePadDriver Zero2Driver = {
  NULL,
  NULL,
  NULL,
  Zero2DecodeInputReport,		// USB and BT
  Zero2BtSetup,			// BT
  Zero2BtProcessCalibReport,	// BT
  Zero2ReleaseACLBuffer,		// BT
  Zero2ResetFusion,			// USB and BT
  Zero2BtDisconnect,		// BT
};
