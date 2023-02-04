TARGET=STM32H7B3
#TARGET=STM32F769

DoomPlayer.elf:
	rm -rf build
	cmake -B build -DTARGET=${TARGET}
	(cd build; make DoomPlayer.elf)
