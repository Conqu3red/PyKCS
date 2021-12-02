# CKCS makefile.
CC=gcc
FLAGS= -O3 -g

TARGET=ckcs.exe
SRC=kcs.c


.DEFAULT_GOAL := default

default: $(SRC)
	$(CC) $(FLAGS) $(SRC) -o $(TARGET) 
	@echo $(TARGET) Succesfully installed.
