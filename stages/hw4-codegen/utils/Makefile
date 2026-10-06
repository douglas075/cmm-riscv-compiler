CC := riscv64-unknown-elf-gcc
CFLAGS := -Wno-implicit-function-declaration \
		  -Wno-builtin-declaration-mismatch

all:
	$(CC) $(CFLAGS) -o main.S -S main.c
	@echo '#include "output.s"' >> main.S

clean:
	rm -f main.S
