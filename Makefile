all: dcpu16emu dcpu16asm

dcpu16emu: emulator.o
	gcc -Wall -Wextra -ansi -pedantic emulator.o -o dcpu16emu -lncurses -g

dcpu16asm: assembler.o linked_list.o
	gcc -Wall -Wextra -ansi -pedantic assembler.o linked_list.o -o dcpu16asm -g

clean:
	find -name '*.o' -delete
	find -name '*.bin' -delete
