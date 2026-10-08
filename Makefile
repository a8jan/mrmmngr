# Makefile for MrmMngr v1.0 - RM-TEAM 04/2022

GCC=gcc
CC=$(GCC)

PREFIX=/usr/local
INSTALL=install -c
RM=rm -vf

EXE=mrmmngr
SUBDIR_BASE=src

CFLAGS=-O2 -Wall -I$(SUBDIR_BASE)
CXXFLAGS=$(CFLAGS)
LDFLAGS=-s

# object files
OBJS=\
    $(SUBDIR_BASE)/utils.o \
    $(SUBDIR_BASE)/file.o \
    $(SUBDIR_BASE)/mrm.o \
    $(SUBDIR_BASE)/main.o

all: $(OBJS)
    $(GCC) -o $(EXE) $(CXXFLAGS) $(OBJS) $(LDFLAGS)

install:
    $(INSTALL) $(EXE) $(PREFIX)/bin

uninstall:
    $(RM) $(PREFIX)/bin/$(EXE)

.c.o:
    $(GCC) $(CFLAGS) -o $@ -c $<

clean:
    $(RM) \
        $(SUBDIR_BASE)/*.o \
        $(EXE) *~
