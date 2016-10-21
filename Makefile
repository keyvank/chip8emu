CC=g++
CFLAGS=-std=c++11 -pthread -lSDL2

all:
	$(CC) chip8.cpp $(CFLAGS) -o chip8
