#ifndef	__SDL_KEYCODE_H__
#define	__SDL_KEYCODE_H__
#include "main.h"

#define SDL_SCANCODE_LCTRL	0
#define SDL_SCANCODE_RCTRL	1
#define SDL_SCANCODE_LSHIFT	2
#define SDL_SCANCODE_RSHIFT	3
#define SDL_SCANCODE_LALT	4
#define SDL_SCANCODE_RALT	5

#define	KMOD_SHIFT	0x01

#define	SDL_BUTTON_LEFT		0x01
#define	SDL_BUTTON_RIGHT	0x02
#define	SDL_BUTTON_MIDDLE	0x04

#define	SDLK_BACKSPACE	0x08
#define	SDLK_RETURN	0x0d

typedef struct s_sdl_Keysym {
  uint16_t scancode;
  int sym;
} SDL_Keysym;

typedef enum {
  SDL_TEXTINPUT = 0,
  SDL_KEYUP,
  SDL_KEYDOWN,
  SDL_MOUSEBUTTONDOWN,
  SDL_MOUSEBUTTONUP,
  SDL_MOUSEWHEEL,
} SDL_EVENT_T;

#define	SDL_PEEKEVENT	1
#define	SDL_FIRSTEVENT	2
#define	SDL_LASTEVENT	3

typedef struct s_sdl_Event {
  SDL_EVENT_T	type;
  struct {
    uint8_t button;
  } button;
  struct {
    char    text[2];
  } text;
  struct {
    SDL_Keysym keysym;
  } key;
  int wheel;
} SDL_Event;

typedef struct {
  int x, y;
  int ev;
} SDL_MouseWheelEvent;

#endif
