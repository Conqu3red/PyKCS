# CKCS makefile.
CC=gcc
CCFLAGS= -O3 -g -lm

ifeq ($(OS),Windows_NT)
    uname_S := Windows
else
    uname_S := $(shell uname -s)
endif

ifeq ($(uname_S), Windows)
    TARGET = ckcs.exe
else
    TARGET = ckcs
endif

INCLUDE=.
SRC=kcs.c bpf.c


.DEFAULT_GOAL := default

default: $(SRC)
	$(CC) $(SRC) $(CCFLAGS) -I${INCLUDE} -o $(TARGET) 
	@echo $(TARGET) Succesfully installed.
