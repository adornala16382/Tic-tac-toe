CC = gcc
CFlags = -Wall -fsanitize=address,undefined -std=c99

ttt: ttt.c
	$(CC) $(CFlags) -o $@ $^

ttts: ttts.c
	$(CC) $(CFlags) -pthread -o $@ $^
