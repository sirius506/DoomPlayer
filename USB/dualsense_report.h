#ifndef __DUALSENSE_REPORT_H__
#define __DUALSENSE_REPORT_H__

#define	DS_INPUT_REPORT_USB	0x01
#define	DS_OUTPUT_REPORT_USB	0x02

#define	VALID_FLAG0_RIGHT_TRIGGER	(1<<2)
#define	VALID_FLAG0_LEFT_TRIGGER	(1<<3)
#define	DS_OUTPUT_VALID_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE (1<<1)
#define	DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE	(1<<2)
#define DS_OUTPUT_VALID_FLAG1_PLAYER_INDICATOR_CONTROL_ENABLE (1<<4)
#define	DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT (1<<1)

#define DS_FEATURE_REPORT_CALIBRATION           0x05
#define DS_FEATURE_REPORT_CALIBRATION_SIZE      41
#define DS_FEATURE_REPORT_FIRMWARE_INFO         0x20
#define DS_FEATURE_REPORT_FIRMWARE_INFO_SIZE    64

/* DualSense hardware limits */
#define DS_ACC_RES_PER_G	8192.0f
#define DS_GYRO_RES_PER_DEG_S	1024.0f
#define DS_ACC_RANGE		(4*DS_ACC_RES_PER_G)
#define DS_GYRO_RANGE		(2048*DS_GYRO_RES_PER_DEG_S)
#define DS_TOUCHPAD_WIDTH	1920
#define DS_TOUCHPAD_HEIGHT	1080

struct dualsense_touch_point {
  uint8_t contact;
  uint8_t x_lo;
  uint8_t x_hi:4, y_lo:4;
  uint8_t y_hi;
} __attribute__((packed));

struct dualsense_input_report {
  uint8_t report_id;
  uint8_t x, y;
  uint8_t rx, ry;
  uint8_t z, rz;
  uint8_t seq_number;
  uint8_t buttons[4];
  uint8_t reserved[4];
  /* Motion sensors */
  int16_t gyro[3];
  int16_t accel[3];
  int32_t timestamp;
  uint8_t Temperature;
  struct dualsense_touch_point points[2];
  uint8_t reserved3[12];
  uint8_t status;
  uint8_t reserved4[10];
} __attribute__((packed));

struct dualsense_output_report {
  uint8_t report_id;
  uint8_t valid_flag0;		// 0
  uint8_t valid_flag1;		// 1
  uint8_t motor_right;		// 2
  uint8_t motor_left;		// 3
  uint8_t reserved[4];		// 4
  uint8_t mute_button_led;	// 8
  uint8_t power_save_control;	// 9
  uint8_t RightTriggerFFB[11];	// 10
  uint8_t LeftTriggerFFB[11];	// 21
  uint32_t HostTimeStamp;	// 32
  uint8_t PowerReduction;	// 36
  uint8_t AudioControl2;	// 37
  uint8_t valid_flag2;		// 38
  uint8_t HapticLPF;		// 39
  uint8_t UNKBYTE_HPF;		// 40
  uint8_t lightbar_setup;	// 41
  uint8_t led_brightness;	// 42
  uint8_t player_leds;		// 43
  uint8_t lightbar_red;
  uint8_t lightbar_green;
  uint8_t lightbar_blue;
  uint8_t reserved4[15];
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

typedef struct {
  int16_t roll;
  int16_t pitch;
  int16_t yaw;
} FUSION_ANGLE;

extern FUSION_ANGLE ImuAngle;

#endif
