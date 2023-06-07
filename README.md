# Doom Player

![startup_LVGL](https://user-images.githubusercontent.com/12758516/218339900-31bd464d-3fbf-48eb-a38c-15aa8c6d7b69.jpg)

Doom Player is DOOM Music/SoundrGame player for STM32H7B3I-DK and STM32F769I-DISCO boards.
Game part is based on [Chocolate-doom 3.0.1](https://www.chocolate-doom.org/).

![music_LVGL](https://user-images.githubusercontent.com/12758516/218339999-62f67c08-b343-4a72-9c57-33ed63e5c785.jpg)

![sound_LVGL](https://user-images.githubusercontent.com/12758516/218340140-58879f46-37b1-4568-a8f7-49c40dc77419.jpg)

![sc004_DOOM](https://user-images.githubusercontent.com/12758516/218340020-90dcee30-518c-4d71-a515-76e0eeaa549a.jpg)

## Features
* [LVGL](https://github.com/lvgl/lvgl) based GUI operation.
* Supports USB connected keyboard, DUALSHOCK4 and DualSense controller for GUI and Game operation.
* Supports Bluetooth connection with DUALSHOCK4 and DualSense controllers.
* Supports DOOM1, DOOM2 and TNT WAD files. Selected WAD file is flashed to the SPI flash for the playing.
* Selectable Audio output -- audio jack on the Discovery board or headset jack on the USB connected DUALSHOCK4 and/or DualSense controllers.
* Plays FLAC format game music files stored on the SD card.
* Plays SFX PCM sounds found on the SPI flash. When DualSense is selected for audio output, SFX sound is also used to generate vibration effect.
* DualSense input demo which shows your button and stick operations on the LCD screen.
* Music player is based on LGVL demo code, but it performes realtime FFT for the fancy spectrum effect.
* Runs on the FreeRTOS. It allows us LVGL GUI runs while Chocolate-Doom is running and provide Cheat code screen feature.
* Sreenshot by User button on the Discovery board. Screen images are JPEG encoded and saved on the SD card.

Please refer [Wiki pages](https://github.com/sirius506/DoomPlayer/wiki) for more descriptions and screen shot images.

## Get started
* Download target .elf and gamedata.zip files from latest [release](https://github.com/sirius506/DoomPlayer/releases).
* Write the target .elf image to the target Disovery board's MCU flash. You can use STM32Prog for the operation.
* Prepare formated SD card. Unzip gamedata.zip and copy GameData directory to the SD card. You also need to copy FLAC format music pack files and supported DOOM game WAD files. Read 'GameData/README.md' for details.
* Insert prepared SD card into the target Discovery board.
* Connect DUALSHOCK4 or DualSense controller (if you have one) to the OTG USB port. Or, connect Bluetooth USB dongle for those controllers.
* Supply the power. It is recommended to avoid to use STLINK position on board power selection, since DualSense consumes the power and it may result as USB host port over current.
* When system is initialized, DOOM games available on the flash and SD cards are listed on the LCD. Select green colored flash game to start the player, or choose one of SD card game to re-write the flash contents.
* Check [wiki pages](https://github.com/sirius506/DoomPlayer/wiki) for more explanations and sample screen shots.

## TODO
* Support Bluetooth keyboard

