#ifndef __SDL_JOYSTICK_H__
#define __SDL_JOYSTICK_H__

/* The SDL joystick structure */
typedef struct _SDL_JoystickAxisInfo
{
    int16_t initial_value;       /* Initial axis state */
    int16_t value;               /* Current axis state */
    int16_t zero;                /* Zero point on the axis (-32768 for triggers) */
    SDL_bool has_initial_value; /* Whether we've seen a value on the axis yet */
    SDL_bool has_second_value;  /* Whether we've seen a second value on the axis yet */
    SDL_bool sent_initial_value; /* Whether we've sent the initial axis value */
    SDL_bool sending_initial_value; /* Whether we are sending the initial axis value */
} SDL_JoystickAxisInfo;

struct _SDL_Joystick
{
    char     *name;
    int      naxes;                  /* Number of axis controls on the joystick */
    SDL_JoystickAxisInfo axes[2];

    int      nhats;                  /* Number of hats on the joystick */
    uint8_t  hats;                /* Current hat states */

    int      nbuttons;               /* Number of buttons on the joystick */
    uint16_t buttons;             /* Current button states */
};

typedef struct _SDL_Joystick SDL_Joystick;

#define	SDL_HAT_CENTERED	0x00
#define	SDL_HAT_UP		0x01
#define	SDL_HAT_RIGHT		0x02
#define	SDL_HAT_DOWN		0x04
#define	SDL_HAT_LEFT		0x08
#define	SDL_HAT_RIGHTUP		(SDL_HAT_RIGHT|SDL_HAT_UP)
#define	SDL_HAT_RIGHTDOWN	(SDL_HAT_RIGHT|SDL_HAT_DOWN)
#define	SDL_HAT_LEFTUP		(SDL_HAT_LEFT|SDL_HAT_UP)
#define	SDL_HAT_LEFTDOWN	(SDL_HAT_LEFT|SDL_HAT_DOWN)

extern SDL_Joystick *SDL_JoystickOpen(int device_index);
extern void SDL_JoystickClose(SDL_Joystick *joystick);
extern uint8_t SDL_JoystickGetHat(SDL_Joystick *joystick, int hat);
extern int SDL_JoystickNumHats(SDL_Joystick *joystick);
extern int SDL_JoystickNumAxes(SDL_Joystick *joystick);
extern uint8_t SDL_JoystickGetButton(SDL_Joystick *joystick, int button);
extern const char *SDL_JoystickName(SDL_Joystick *joystick);
extern int16_t SDL_JoystickGetAxis(SDL_Joystick *joystick, int axis);

#endif
