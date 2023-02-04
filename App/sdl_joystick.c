/*
 * SDL joystick API support.
 */
#include <stdio.h>
#include <stdlib.h>
#include "DoomPlayer.h"

#include "SDL.h"
#include "SDL_joystick.h"
#include "dualsense_report.h"

static SDL_Joystick *active_stick;

SDL_Joystick *
SDL_JoystickOpen(int device_index)
{
  SDL_Joystick *joystick;

  joystick = malloc(sizeof(SDL_Joystick));
  memset(joystick, 0, sizeof(SDL_Joystick));
  joystick->name = "DualSence";
  joystick->naxes = 1;
  joystick->nhats = 1;
  joystick->nbuttons = 17;
  joystick->hats = 0;
  joystick->buttons = 0;;
  active_stick = joystick;
  return joystick;
}

/*
 * Checks to make sure the joystick is valid.
 */
int
SDL_PrivateJoystickValid(SDL_Joystick * joystick)
{
    int valid;

    if (joystick == NULL) {
        debug_printf("Joystick hasn't been opened yet.\n");
        valid = 0;
    } else {
        valid = 1;
    }

    return valid;
}

/*
 * Get the number of multi-dimensional axis controls on a joystick
 */
int
SDL_JoystickNumAxes(SDL_Joystick *joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        return -1;
    }
    return joystick->naxes;
}

/*
 * Get the number of hats on a joystick
 */
int
SDL_JoystickNumHats(SDL_Joystick *joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        return -1;
    }
    return joystick->nhats;
}

/*
 * Get the current state of a hat on a joystick
 */
uint8_t
SDL_JoystickGetHat(SDL_Joystick *joystick, int hat)
{
    uint8_t state;

    if (!SDL_PrivateJoystickValid(joystick)) {
        return 0;
    }
    if (hat < joystick->nhats) {
        state = joystick->hats;
    } else {
        printf("Joystick only has %d hats", joystick->nhats);
        state = 0;
    }
    return state;
}

/*
 * Get the current state of a button on a joystick
 */
uint8_t
SDL_JoystickGetButton(SDL_Joystick *joystick, int button)
{
    uint8_t state;

    if (!SDL_PrivateJoystickValid(joystick)) {
        return 0;
    }
    if (button < joystick->nbuttons) {
        state = (joystick->buttons & (1 << button))? 1 : 0;
    } else {
        //printf("Joystick only has %d buttons", joystick->nbuttons);
        state = 0;
    }
    return state;
}

/*
 * Get the current state of an axis control on a joystick
 */
int16_t
SDL_JoystickGetAxis(SDL_Joystick *joystick, int axis)
{
    int16_t state;

    if (!SDL_PrivateJoystickValid(joystick)) {
        return 0;
    }
    if (axis < joystick->naxes) {
        state = joystick->axes[axis].value;
    } else {
        printf("Joystick only has %d axes", joystick->naxes);
        state = 0;
    }
    return state;
}

/*
 * Get the friendly name of this joystick
 */
const char *
SDL_JoystickName(SDL_Joystick *joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        return NULL;
    }

    return joystick->name;
}

void SDL_JoystickClose(SDL_Joystick *joystick)
{
   active_stick = NULL;
   free(joystick);
}

static const uint8_t hatmap[16] = {
  SDL_HAT_UP,       SDL_HAT_RIGHTUP,   SDL_HAT_RIGHT,    SDL_HAT_RIGHTDOWN,
  SDL_HAT_DOWN,     SDL_HAT_LEFTDOWN,  SDL_HAT_LEFT,     SDL_HAT_LEFTUP,
  SDL_HAT_CENTERED, SDL_HAT_CENTERED,  SDL_HAT_CENTERED, SDL_HAT_CENTERED,
  SDL_HAT_CENTERED, SDL_HAT_CENTERED,  SDL_HAT_CENTERED, SDL_HAT_CENTERED,
};

void Generate_DOOM_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton)
{
  if (active_stick)
  {
    active_stick->hats = hatmap[hat];
    active_stick->buttons = vbutton & 0x7FFF;
  }
}
