#ifndef _GAMEPAD_H
#define	_GAMEPAD_H
#include "usbh_hid.h"
#include "lvgl.h"
#include "Fusion.h"

#define	VID_SONY	0x054C
#define	PID_DUALSHOCK	0x09CC
#define	PID_DUALSENSE	0x0CE6

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
#define	VBMASK_SHARE	(1<<8)
#define	VBMASK_OPTION	(1<<9)
#define	VBMASK_L3	(1<<10)
#define	VBMASK_R3	(1<<11)
#define	VBMASK_PS	(1<<12)
#define VBMASK_TOUCH    (1<<13)
#define VBMASK_MUTE     (1<<14)
#define VBMASK_UP       (1<<15)
#define VBMASK_LEFT     (1<<16)
#define VBMASK_RIGHT    (1<<17)
#define VBMASK_DOWN     (1<<18)

struct sGamePadDriver {
  USBH_StatusTypeDef (*Init)(USBH_ClassTypeDef *pclass, USBH_HandleTypeDef *phost);
  void (*Setup)(USBH_ClassTypeDef *pclass);
  void (*GetOutputReport)(uint8_t **ptr, int *plen, int hid_mode, uint8_t sofc);
  void (*DecodeInputReport)(HID_REPORT *report);
  void (*btSetup)(uint16_t cid);
  void (*btProcessGetReport)(const uint8_t *report, int len);
  void (*btReleaseBuffer)(uint8_t *bp);
  void (*ResetFusion)(void);
  void (*btDisconnect)(void);
};

typedef struct sGamePadInfo {
  char   *name;
  USBH_ClassTypeDef *pclass;
  int    hid_mode;
  const struct sGamePadDriver *padDriver;
} GAMEPAD_INFO;

typedef enum {
  HID_SOF_FLAG = 1<<8,
  HID_LVGL_BIT = 1<<9,
  HID_DOOM_BIT = 1<<10,
  HID_TEST_BIT = 1<<11,
} HID_EV_FLAG;

//#define	FLAG_TIMEOUT	5

typedef struct {
  uint32_t mask;
  lv_key_t lvkey;
} PADKEY_DATA;

struct sGamePad {
  uint16_t  vid;
  uint16_t  pid;
  char      *name;
  const struct sGamePadDriver *padDriver;
};

struct gamepad_touch_point {
  uint8_t  contact;
  uint16_t xpos;
  uint16_t ypos;
};

struct gamepad_inputs {
  uint8_t x, y;
  uint8_t rx, ry;
  uint8_t z, rz;
  uint32_t vbutton;
  /* Motion sensors */
  int16_t gyro[3];
  int16_t accel[3];
  struct  gamepad_touch_point points[2];
  uint8_t Temperature;
  uint8_t battery_level;
};

typedef struct {
  int16_t roll;
  int16_t pitch;
  int16_t yaw;
} FUSION_ANGLE;

extern FUSION_ANGLE ImuAngle;

extern const struct sGamePadDriver DualShockDriver;
extern const struct sGamePadDriver DualSenseDriver;

extern GAMEPAD_INFO *IsSupportedGamePad(uint16_t vid, uint16_t pid);
extern int get_bt_hid_mode();
extern void SDL_JoyStickSetButtons(uint8_t hat, uint32_t vbutton);

extern uint32_t bt_comp_crc(uint8_t *ptr, int len);

extern void setup_fusion(int sample_rate, const FusionAhrsSettings *psettings);
extern void gamepad_process_fusion(float sample_period, FusionVector gyroscope, FusionVector accelerometer);
extern void Display_GamePad_Info(struct gamepad_inputs *rp, uint32_t vbutton);
extern void GamepadHidMode(GAMEPAD_INFO *padInfo, int mode_bit);
extern void gamepad_reset_fusion(void);
#endif
