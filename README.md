# Doom Player
Doom Player is DOOM Music/SoundrGame player for STM32H7B3I-DK and 32F769IDiscovery boards.
Game part is based on [Chocolate-doom 3.0.1](https://www.chocolate-doom.org/).

## Features
* [LVGL](https://github.com/lvgl/lvgl) based GUI operation.
* Supports USB connected keyboard and PS5 Dualsense controller for GUI and Game operation.
* Supports DOOM1, DOOM2 and TNT WAD files. Selected WAD file is flashed to the SPI flash for the playing.
* Selectable Audio output -- audio jack on the Discovery board or headset connected to the DualSense.
* Plays FLAC format game music files stored on the SD card.
* Plays SFX PCM sounds found on the SPI flash. When DualSense is selected for audio output, SFX sound is also used to generate vibration effect.
* DualSense input demo which shows your button and stick operations on the LCD screen.
* Music player is based on LGVL demo code, but it performes realtime FFT for the fancy spectrum effect.
* Runs on the FreeRTOS. It allows us LVGL GUI runs while Chocolate-Doom is running and provide Cheat code screen feature.
* Sreenshot by User button on the Discovery board. Screen images are JPEG encoded and saved on the SD card.

## Binary Installation
* Write the DoomPlayer.elf image under the bin directory to the target Disovery board's MCU flash. You can use STM32Prog for the operation.
* Prepare formated SD card and create '/GameData' directory. Copy all files under the GameData directory in this repo to the SD card. You also need to copy FLAC format music pack files and supported DOOM game WAD files. Read 'GameData/README.md' for details.

## Starting up
* Insert prepared SD card into the target Discovery board.
* Connect DualSense controller (if you have one) to the OTG USB port. 
* Supply the power. It is recommended to avoid to use STLINK position on board power selection, since DualSense consumes the power and it may result as USB host port over current.
* When system is initialized, DOOM games available on the flash and SD cards are listed on the LCD. Select green colored flash game to start the player, or choose one of SD card game to re-write the flash contents.

## DualSense
DualSense controller can be used to control LVGL GUI and Doom Game.
It also supports features below.

* Music/Sound output through Headphone jack. Sound output also generates vibration.
* Light bar and player LEDs changes its color and blightness while music is playing.
* While DOOM game mode, Light bar indicates DOOM guy's health status.

For controler button operation, refer following tables.

### LVGL GUI

Buttons are mapped to keyboard input as shown below.

| Button | LVGL Key |
| ------ | -------- |
| Up | LV_KEY_UP |
| Down | LV_KEY_DOWN |
| Left | LV_KEY_PREV |
| Right | LV_KEY_NEXT |
| Circle | LV_KEY_ENTER |
| Square | LV_KEY_ENTER |
| Cross | LV_KEY_DEL |
| L1 | LV_KEY_LEFT |
| R1 | LV_KEY_RIGHT |

### DOOM

DualSense controller is treated as joystick device and buttons can be used
as show below table.

| Button | Function |
| ------ | -------- |
| Up | Move forward |
| Down | Move backward |
| Left | Turn left |
| Right | Turn right |
| Square | Fire |
| Circle | Use |
| L1 | Prev weapon |
| R1 | Next weapon |
| L2 | Strafe left |
| R2 | Strafe right |
| PS | Menu on/ff |
| Create | Map on/off |

## SD Card

SD card directory structure is shown in below.

	+- GameData -+-  All game config and WAD files are stored
	|            |
	|            +-  doom1-music	DOOM1 FLAC music files
	|            |
	|            +-  doom2-music  DOOM2 FLAC music files
	|            |
	|            +-  tnt-music    TNT FLC music files
	|
	+ Screen -- Screen capture files are stored
	|
	+ savegames -- Saved doom game data
