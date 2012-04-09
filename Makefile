all: dcpu16emu dcpu16asm

dcpu16emu: emulator.o
	gcc -Wall emulator.o -o dcpu16emu

dcpu16asm: assembler.o linked_list.o
	gcc -Wall assembler.o linked_list.o -o dcpu16asm
