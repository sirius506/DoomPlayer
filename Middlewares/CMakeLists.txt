set(MIDSRCS
  Third_Party/ff15/source/diskio.c
  Third_Party/ff15/source/ffsystem.c
  Third_Party/ff15/source/ff.c
  Third_Party/ff15/source/ff_gen_drv.c
  Third_Party/ff15/source/ffunicode.c
  Third_Party/FreeRTOS/Source/croutine.c
  Third_Party/FreeRTOS/Source/event_groups.c
  Third_Party/FreeRTOS/Source/list.c
  Third_Party/FreeRTOS/Source/queue.c
  Third_Party/FreeRTOS/Source/stream_buffer.c
  Third_Party/FreeRTOS/Source/tasks.c
  Third_Party/FreeRTOS/Source/timers.c
  Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
  Third_Party/FreeRTOS/Source/portable/MemMang/heap_5.c
  Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
)

add_library(middlewares STATIC ${MIDSRCS})
