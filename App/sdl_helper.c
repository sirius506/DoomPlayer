#include "DoomPlayer.h"
#include "SDL.h"
#include "app_task.h"

int SDL_Init(uint32_t flags)
{
  return 0;
}

int SDL_Quit()
{
  debug_printf("%s called.!\n", __FUNCTION__);
  NVIC_SystemReset();
  postMainRequest(REQ_END_DOOM, NULL, 0);
  while (1)
   osDelay(100);
}

uint32_t SDL_GetTicks(void)
{
  return osKernelGetTickCount();
}

void SDL_Delay(uint32_t ms)
{
  osDelay(ms);
}

int SDL_ShowSimpleMessageBox(uint32_t flags, const char *title, const char *message, void *window)
{
  debug_printf("%s called.!\n", __FUNCTION__);
  return 0;
}

char *SDL_GetPrefPath(const char *org, const char *app)
{
  return "/";
}
