# CKCS makefile.
CC=gcc
FLAGS= -O3 -g

TARGET=ckcs.exe
INCLUDE=.
SRC=kcs.c bpf.c


.DEFAULT_GOAL := default

default: $(SRC)
	$(CC) $(FLAGS) $(SRC) -I${INCLUDE} -o $(TARGET) 
	@echo $(TARGET) Succesfully installed.
