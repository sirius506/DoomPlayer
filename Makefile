TARGET=STM32H7B3
#TARGET=STM32F769

all: build/DoomPlayer.elf

build/DoomPlayer.elf:
	if ! [ -e build ]; then \
	  cmake -B build -DTARGET=${TARGET}; \
	fi
	(cd build; make DoomPlayer.elf)

install: build/DoomPlayer.elf
	-mkdir bin/${TARGET}
	install build/DoomPlayer.elf bin/${TARGET}

clean:	
	rm -rf build
