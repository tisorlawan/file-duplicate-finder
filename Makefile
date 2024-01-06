CC=gcc
CFLAGS=-Wall -Wextra -std=gnu11
OFLAGS=-O2

duplicate-finder: main.c
	$(CC) $(CFLAGS) $(OFLAGS) main.c -o duplicate-finder

debug: clean main.c
	$(CC) $(CFLAGS) -ggdb main.c -o duplicate-finder

run: file-deduper
	@./duplicate-finder

clean:
	@$(RM) duplicate-finder
