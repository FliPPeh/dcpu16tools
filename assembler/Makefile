dcpu16asm: assembler.o ../common/hexdump.o ../common/linked_list.o parse.o \
           util.o ../common/dcpu16.o label.o
	$(CC) -o dcpu16asm ../common/hexdump.o ../common/linked_list.o \
                       parse.o util.o ../common/dcpu16.o label.o assembler.o \
		  $(CFLAGS) $(LDFLAGS)
