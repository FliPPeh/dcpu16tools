CFLAGS=-Wall -Wextra -std=c99 -g -I../common/
LDFLAGS=-lncurses

export CFLAGS
export LDFLAGS

all: dcpu16emu dcpu16asm
	ln -fs emulator/dcpu16emu
	ln -fs assembler/dcpu16asm

dcpu16emu: .PHONY
	make -C emulator/

dcpu16asm: .PHONY
	make -C assembler/

clean:
	find -name '*.o' -delete
	find -name '*.bin' -delete

.PHONY:
