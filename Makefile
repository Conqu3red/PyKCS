# CKCS makefile.
CC=gcc
BFLAGS= -v -Wall -g 
CFLAGS= -O3

TARGET=ckcs.exe
SRC=kcs.c


.DEFAULT_GOAL := default

default: $(SRC)
	$(CC) $(BFLAGS) $(SRC) -o $(TARGET) $(CFLAGS)
	@echo $(TARGET) Succesfully installed.