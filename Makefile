TARGET=STM32H7B3
#TARGET=STM32F769

all: build${TARGET}/DoomPlayer.elf

build${TARGET}/DoomPlayer.elf:
	if ! [ -e build${TARGET} ]; then \
	  cmake -B build${TARGET} -DTARGET=${TARGET}; \
	fi
	(cd build${TARGET}; make DoomPlayer.elf)

install: build${TARGET}/DoomPlayer.elf
	-mkdir release
	install build${TARGET}/DoomPlayer.elf release/DoomPlayer_${TARGET}.elf

clean:	
	rm -rf build${TARGET}

cleanelf:
	rm build*/DoomPlayer.elf
