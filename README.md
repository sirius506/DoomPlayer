# Doom Player
Doom Player is DOOM Music/Sound/Game player for STM32H7B3I and STM32F769 Discovery boards.
Game part is based on Chocolate-doom 3.0.1 (https://www.chocolate-doom.org/).

## Features
* [LVGL](https://github.com/lvgl/lvgl) based GUI operation.
* Supports USB connected keyboard and PS5 Dualsense controller for GUI and Game operation.
* Supports DOOM1, DOOM2 and TNT WAD files. Selected WAD file is flashed to the SPI flash for the playing.
* Selectable Audio output -- audio jack on the Discovery board or headset connected to the DualSense.
* Plays FLAC format game music files stored on the SD card.
* Plays SFX PCM sounds found on the SPI flash. When DualSense is selected for audio output, SFX sound is also used to generate vibration effect.
* DualSense input demo which shows your botton and stick operations on the LCD screen.
* Music player is based on LGVL demo code, but it performes realtime FFT for the fancy spectrum effect.
* Runs on the FreeRTOS. It allows us LVGL GUI runs while Chocolate-Doom is running and provide Cheat code screen feature.
* Sreenshot by User button on the Discovery board. Screen images are JPEG encoded and saved on the SD card.

## Binary Installation
* Write the DoomPlayer.elf image to the target Disovery board's MCU flash. You can use STM32Prog for the operation.
* Prepare formated SD card and create "GameData" directory. Copy all files under the GameData directory in this repo to the SD card. You also need to FLAC format music pack files and supported DOOM game WAD files. Read 'GameData/README.md' for details.

## How to play
### Starting up
* Insert prepared SD card into the target Discovery board.
* Connect DualSense controller (if you have one) to the OTG USB port. 
* Supply the power. It is recommended to avoid to use STLINK position on board power selection, since DualSense consumes the power and it may result as USB host port over current.
## Start screen
