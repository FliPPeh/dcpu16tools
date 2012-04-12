all: dcpu16emu dcpu16asm

dcpu16emu: emulator.o hexdump.o
	gcc -Wall -Wextra -ansi -pedantic emulator.o hexdump.o -o dcpu16emu -lncurses -g

dcpu16asm: assembler.o linked_list.o hexdump.o
	gcc -Wall -Wextra -ansi -pedantic assembler.o hexdump.o linked_list.o -o dcpu16asm -g

clean:
	find -name '*.o' -delete
	find -name '*.bin' -delete
