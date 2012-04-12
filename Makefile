export CFLAGS=-Wall -Wextra -std=c99 -g -I../common/
export LDFLAGS=-lncurses

all: dcpu16emu dcpu16asm
	ln -fs emulator/dcpu16emu
	ln -fs assembler/dcpu16asm

dcpu16emu:
	make -C emulator/

dcpu16asm:
	make -C assembler/

clean:
	find -name '*.o' -delete
	find -name '*.bin' -delete
