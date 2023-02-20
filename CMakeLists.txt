cmake_minimum_required(VERSION 3.13)


set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER    arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER  arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER  arm-none-eabi-g++)
set(CMAKE_EXE_LINKER    arm-none-eabi-gcc)
set(CMAKE_RANLIB	arm-none-eabi-ranlib)
set(OBJCOPY             arm-none-eabi-objcopy)
set(SIZE                arm-none-eabi-size)

Project(DoomPlayer LANGUAGES C)

enable_language(ASM)

include(FetchContent)

if (${TARGET} STREQUAL "STM32H7B3")
  set(FAMILY "stm32h7xx")
  set(DEV_FAMILY "STM32H7xx")
  set(CPUDEV "STM32H7B3xxQ")
  set(HALDRIVER "STM32H7xx_HAL_DRIVER")
  set(STARTUP "${TARGET}/startup_stm32h7b3xxq.s")
  set(LINKERSCRIPT "STM32H7B3LIHxQ_FLASH.ld")
endif()
if (${TARGET} STREQUAL "STM32F769")
  set(FAMILY "stm32f7xx")
  set(DEV_FAMILY "STM32F7xx")
  set(CPUDEV "STM32F769xx")
  set(HALDRIVER "STM32F7xx_HAL_DRIVER")
  set(STARTUP "${TARGET}/startup_stm32f769xx.s")
  set(LINKERSCRIPT "STM32F769NIHx_FLASH.ld")
endif()

set(CMAKE_AS_FLAGS "-mcpu=cortex-m7 -x assembler-with-cpp -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -D${CPUDEV} -DARM_MATH_CM7")

set(CMAKE_C_FLAGS "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -DUSE_HAL_DRIVER -D${CPUDEV} -DARM_MATH_CM7 -O1 -Wall -fdata-sections -ffunction-sections -g -gdwarf-2")

set(CMAKE_CXX_FLAGS "-mcpu=cortex-m7 -std=gnu++14 -fno-use-cxa-atexit -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_FLAGS} -L ${CMAKE_SOURCE_DIR} -T ${TARGET}/${LINKERSCRIPT} -lc -lm -lnosys --specs=nano.specs --specs=nosys.specs  -Wl,--gc-sections")

set(LV_CONF_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}/Core/Inc/lv_conf.h
  CACHE STRING "" FORCE)

add_executable(DoomPlayer.elf
  ${TARGET}/Core/Src/main.c
  ${TARGET}/Core/Src/freertos.c
  ${TARGET}/Core/Src/${FAMILY}_hal_msp.c
  ${TARGET}/Core/Src/${FAMILY}_hal_timebase_tim.c
  ${TARGET}/Core/Src/${FAMILY}_it.c
  ${TARGET}/Core/Src/system_${FAMILY}.c
  ${TARGET}/Core/Src/usb_host.c
  ${TARGET}/Core/Src/usbh_conf.c
  RTT/SEGGER_RTT.c
  RTT/SEGGER_RTT_printf.c
  App/app_console.c
  App/app_main.c
  App/jpeg_if.c
  App/audio_output.c
  App/app_verify.c
  App/app_dualsense.c
  App/app_gui.c
  App/mix_player.c
  App/fatfs.c
  App/app_music_main.c
  App/app_music_list.c
  App/app_sound.c
  App/cover_if.c
  App/doom_helper.c
  App/sdl_joystick.c
  App/sdl_helper.c
  App/assets/Doom1_icon.c
  App/assets/dualsense2.c
  App/assets/img_lv_demo_music_icon_1.c
  App/assets/img_lv_demo_music_icon_2.c
  App/assets/img_lv_demo_music_icon_3.c
  App/assets/img_lv_demo_music_icon_4.c
  App/assets/img_lv_demo_music_corner_left.c
  App/assets/img_lv_demo_music_corner_right.c
  App/assets/img_lv_demo_music_btn_rnd.c
  App/assets/img_lv_demo_music_btn_loop.c
  App/assets/img_lv_demo_music_btn_prev.c
  App/assets/img_lv_demo_music_btn_next.c
  App/assets/img_lv_demo_music_btn_pause.c
  App/assets/img_lv_demo_music_btn_play.c
  App/assets/img_lv_demo_music_wave_top.c
  App/assets/img_lv_demo_music_wave_bottom.c
  App/assets/img_lv_demo_music_btn_list_play.c
  App/assets/img_lv_demo_music_btn_list_pause.c
  App/assets/img_lv_demo_music_logo.c
  App/assets/img_lv_demo_music_list_border.c
  App/assets/img_lv_demo_music_slider_knob.c
  ${STARTUP}
)

FetchContent_Declare(
  drlib
  GIT_REPOSITORY https://github.com/mackron/dr_libs.git
)

FetchContent_Declare(
  lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG f29514aa5c34d84f69a7ed30835bbffc5cbd515c
)

FetchContent_MakeAvailable(drlib lvgl)

set(DRIVERS_INCLUDE
  ${TARGET}/Drivers/BSP
  ${TARGET}/Drivers/BSP/Components/Common
  ${TARGET}/Drivers/${HALDRIVER}/Inc
  ${TARGET}/Drivers/CMSIS/Device/ST/${DEV_FAMILY}/Include
  ${TARGET}/Drivers/CMSIS/Include
)

set(MIDDLE_INCLUDE
  Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
  Middlewares/Third_Party/FreeRTOS/Source/include
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
  Middlewares/Third_Party/ff15/source
)

target_include_directories(DoomPlayer.elf PUBLIC
  App
  ${TARGET}/Core/Inc
  RTT
  Fusion
  SDL
  USB
  FATFS/App
  FATFS/Target
  ${lvgl_SOURCE_DIR}
  ${drlib_SOURCE_DIR}
  chocolate-doom
  ${DRIVERS_INCLUDE}
  ${TARGET}/Drivers/CMSIS/DSP/Include
  ${TARGET}/CMSIS/DSP/Include
  ${MIDDLE_INCLUDE}
)

add_subdirectory(${TARGET}/Drivers)
add_subdirectory(Middlewares)
add_subdirectory(USB)
add_subdirectory(Fusion)
add_subdirectory(chocolate-doom)

target_compile_options(lvgl PUBLIC -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -O1 -Wall -fdata-sections -ffunction-sections -g -gdwarf-2
)

target_include_directories(lvgl PUBLIC
  App
  ${DRIVERS_INCLUDE}
  ${MIDDLE_INCLUDE}
  RTT
)

target_include_directories(drivers PUBLIC
  App
  ${lvgl_SOURCE_DIR}
  ${DRIVERS_INCLUDE}
  ${TARGET}/Core/Inc
  ${DRIVERS_INCLUDE}
  ${MIDDLE_INCLUDE}
  RTT
  SDL
)

target_include_directories(doom PUBLIC 
  App
  ${TARGET}/Core/Inc
  ${DRIVERS_INCLUDE}
  ${lvgl_SOURCE_DIR}
  ${MIDDLE_INCLUDE}
  chocolate-doom
  RTT
  SDL
)

target_include_directories(usb PUBLIC 
  App
  ${TARGET}/Core/Inc
  ${DRIVERS_INCLUDE}
  ${lvgl_SOURCE_DIR}
  ${MIDDLE_INCLUDE}
  chocolate-doom
  RTT
  Fusion
  SDL
)

target_include_directories(middlewares PUBLIC 
  App
  ${TARGET}/Core/Inc
  ${DRIVERS_INCLUDE}
  ${MIDDLE_INCLUDE}
  RTT
)

target_compile_definitions(lvgl PUBLIC USE_HAL_DRIVER ${CPUDEV} ARM_MATH_CM7)

target_link_libraries(DoomPlayer.elf PRIVATE
  usb
  doom
  middlewares
  drivers
  lvgl::lvgl
  Fusion
  ${CMAKE_SOURCE_DIR}/${TARGET}/Drivers/CMSIS/DSP/Lib/GCC/libarm_cortexM7lfdp_math.a
)
