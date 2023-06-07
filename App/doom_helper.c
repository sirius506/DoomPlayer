#include "main.h"
#include "doom/d_player.h"
#include "doom/doomstat.h"
#include "usbh_hid.h"
#include "app_task.h"
#include "doomkeys.h"
#include "lvgl.h"

static const uint8_t scancode_translate_table[] = SCANCODE_TO_KEYS_ARRAY;

extern osMessageQueueId_t kbdqId;

extern void send_padkey(lv_indev_data_t *pdata);

/**
 * @brief Returns RGB color based on current health level.
 * @arg cval pointer to RGB array.
 */
void GetPlayerHealthColor(uint8_t *cval)
{
  int hval;
#ifdef HEALTH_DEBUG
  static int hcount;
#endif

  hval = players[consoleplayer].mo->health;
  if (hval > 300)
    hval = 300;
  if (hval < 0)
    hval = 0;
#ifdef HEALTH_DEBUG
  if (++hcount % 50 == 0)
  {
   debug_printf("hval (%d) = %d\n", consoleplayer, hval);
  }
#endif

  cval[0] = cval[1] = cval[2] = 0;

  if (hval >= 80)		// Completey healthy
  {
    cval[1] = 255;		// Green
  }
  else if (hval >= 60)
  {
    cval[0] = 128;
    cval[1] = 255;
  }
  else if (hval >= 40)
  {
    cval[0] = 255;
    cval[1] = 255;
  }
  else if (hval >= 20)
  {
    cval[0] = 255;
    cval[1] = 130;
  }
  else if (hval > 0)
  {
    cval[0] = 255;
  }
}

static KBDEVENT keyev;

static const uint8_t modcode[8] = {
 KEY_RCTRL, KEY_RSHIFT, KEY_RALT, 0,
 KEY_RCTRL, KEY_RSHIFT, KEY_RALT, 0,
};

void send_modkey_event(HID_HandleTypeDef *HID_Handle, int evcode, int bpos)
{
  if (HID_Handle->report.hid_mode == HID_MODE_DOOM)
  {
    if (modcode[bpos])
    {
      keyev.evcode = evcode;
      keyev.asciicode = keyev.doomcode = modcode[bpos];
      osMessageQueuePut(HID_Handle->kbdqId, &keyev, 0, 0);
    }
  }
}

/**
 * @brief Convert keyboard event into appropriate code, then send it to application task.
 */
void send_key_event(HID_HandleTypeDef *HID_Handle, int evcode, int kbdcode, int modkey)
{
  static lv_indev_data_t pad_data;

  switch (HID_Handle->report.hid_mode)
  {
  case HID_MODE_DOOM:
    keyev.evcode = evcode;
    keyev.asciicode = keyev.doomcode = scancode_translate_table[kbdcode];
    osMessageQueuePut(HID_Handle->kbdqId, &keyev, 0, 0);
    break;
  case HID_MODE_LVGL:
    pad_data.key = 0;
    switch (kbdcode)
    {
    case 40:
      pad_data.key = LV_KEY_ENTER;
      break;
    case 41:
      pad_data.key = LV_KEY_ESC;
      break;
    case 42:
      pad_data.key = LV_KEY_BACKSPACE;
      break;
    case 43:	/* TAB */
      if (modkey & 0x22)		/* Shift state? */
        pad_data.key = LV_KEY_PREV;
      else
        pad_data.key = LV_KEY_NEXT;
      break;
    case 76:
      pad_data.key = LV_KEY_DEL;
      break;
    case 79:	/* Right arrow */
      pad_data.key = LV_KEY_RIGHT;
      break;
    case 80:	/* Left arrow */
      pad_data.key = LV_KEY_LEFT;
      break;
    case 81:	/* Down arrow */
      pad_data.key = LV_KEY_DOWN;
      break;
    case 82:	/* Up arrow */
      pad_data.key = LV_KEY_UP;
      break;
    default:
      break;
    }
    if (pad_data.key)
    {
      pad_data.state = (evcode == KBDEVENT_DOWN)? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
      pad_data.continue_reading = false;
      send_padkey(&pad_data);
    }
    break;
  default:
    break;
  }
}

/*
 * @brief Send cheat text character to DoomTask.
 */
void doom_send_cheat_key(char ch)
{
  static KBDEVENT ckevent;

  ckevent.evcode = KBDEVENT_DOWN;
  ckevent.asciicode = ckevent.doomcode = ch;
  osMessageQueuePut(kbdqId, &ckevent, 0, 0);
}

KBDEVENT *kbd_get_event()
{
  static KBDEVENT kevent;
  osStatus st;

  if (kbdqId)
  {
    st = osMessageQueueGet(kbdqId, &kevent, NULL, 0);
    if (st == osOK)
    {
      return &kevent;
    }
  }
  return NULL;
}
