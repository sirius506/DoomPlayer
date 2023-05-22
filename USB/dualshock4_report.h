#ifndef _DSHOCK4_REPORT_H
#define _DSHOCK4_REPORT_H

#define	DS4_INPUT_REPORT_USB		0x01
#define	DS4_INPUT_REPORT_USB_SIZE	64
#define	DS4_INPUT_REPORT_BT		0x11
#define	DS4_INPUT_REPORT_BT_SIZE	78
#define	DS4_OUTPUT_REPORT_USB	0x05
#define	DS4_OUTPUT_REPORT_BT	0x11

#define DS4_OUTPUT_VALID_FLAG0_MORTOR   0x01
#define DS4_OUTPUT_VALID_FLAG0_LED      0x02
#define DS4_OUTPUT_VALID_FLAG0_BLINK    0x04

#define DS4_FEATURE_REPORT_CALIBRATION           0x02
#define DS4_FEATURE_REPORT_CALIBRATION_SIZE      37
#define DS4_FEATURE_REPORT_CALIBRATION_BT        0x05
#define DS4_FEATURE_REPORT_CALIBRATION_BT_SIZE   41
#define DS4_FEATURE_REPORT_FIRMWARE_INFO         0x20
#define DS4_FEATURE_REPORT_FIRMWARE_INFO_SIZE    64

#define	DS4_OUTPUT_HWCTL_HID	0x80
#define	DS4_OUTPUT_HWCTL_CRC32	0x40

/* DualSense hardware limits */
#define DS4_ACC_RES_PER_G	8192.0f
#define DS4_GYRO_RES_PER_DEG_S	1024.0f
#define DS4_ACC_RANGE		(4*DS4_ACC_RES_PER_G)
#define DS4_GYRO_RANGE		(2048*DS4_GYRO_RES_PER_DEG_S)
#define DS4_TOUCHPAD_WIDTH	1920
#define DS4_TOUCHPAD_HEIGHT	1080

struct ds4_touch_point {
  uint8_t contact;
  uint8_t x_lo;
  uint8_t x_hi:4, y_lo:4;
  uint8_t y_hi;
} __attribute__((packed));

struct ds4_touch_report {
  uint8_t time_stamp;
  struct ds4_touch_point points[2];
} __attribute__((packed));

struct ds4_input_report {
  uint8_t x, y;
  uint8_t rx, ry;
  uint8_t buttons[3];
  uint8_t z, rz;

  uint16_t timestamp;
  uint8_t temp;
  /* Motion sensors */
  int16_t gyro[3];
  int16_t accel[3];

  uint8_t reserved2[5];
  uint8_t status[2];
  uint8_t reserved3;
} __attribute__((packed));

typedef struct ds4_input_report  DS4_INPUT_REPORT;

struct ds4_usb_input_report {
  uint8_t report_id;
  DS4_INPUT_REPORT in_report;
  uint8_t num_touch_reports;
  struct ds4_touch_report reports[3];
  uint8_t reserved[3];
} __attribute__((packed));

struct ds4_bt_input_report {
  uint8_t report_id;
  uint8_t reserved0[2];
  DS4_INPUT_REPORT in_report;
  uint8_t num_touch_reports;
  struct ds4_touch_report reports[4];
  uint8_t reserved1[2];
  uint16_t crc;
} __attribute__((packed));

struct ds4_output_report {
  uint8_t valid_flag0;		// 0
  uint8_t valid_flag1;		// 1
  uint8_t reserved;
  uint8_t motor_right;
  uint8_t motor_left;
  uint8_t lightbar_red;
  uint8_t lightbar_green;
  uint8_t lightbar_blue;
  uint8_t lightbar_blink_on;
  uint8_t lightbar_blink_off;
} __attribute__((packed));

typedef struct ds4_output_report DS4_OUTPUT_REPORT;

struct ds4_usb_output_report {
  uint8_t report_id;
  DS4_OUTPUT_REPORT out_report;
  uint8_t reserved2[21];
} __attribute__((packed));

struct ds4_bt_output_report {
  uint8_t report_id;
  uint8_t hw_control;
  uint8_t audio_control;
  DS4_OUTPUT_REPORT out_report;
  uint8_t reserved2[61];
  uint32_t crc;
} __attribute__((packed));

/* Calibration data for playstation motion sensors. */
struct imu_calibration_data {
    int16_t bias;
    float sensitivity;
};

struct ds_data {
    /* Calibration data for accelerometer and gyroscope. */
    struct imu_calibration_data accel_calib_data[3];
    struct imu_calibration_data gyro_calib_data[3];
};

#endif
