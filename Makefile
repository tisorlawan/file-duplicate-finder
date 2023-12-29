CC=gcc
CFLAGS=-Wall -Wextra -std=gnu11
OFLAGS=-O2

file-deduper: main.c
	@$(CC) $(CFLAGS) $(OFLAGS) main.c -o file-deduper

debug: clean main.c
	$(CC) $(CFLAGS) -ggdb main.c -o file-deduper

run: file-deduper
	@./file-deduper

clean:
	@$(RM) file-deduper
