.PHONY: debug clean

CC := gcc
CFLAGS := -Wall -Wextra -pedantic

main: main.c scanner.c
	$(CC) $^ -o $@ $(CFLAGS)

debug: CFLAGS += -g
debug: main

clean:
	rm -f main
