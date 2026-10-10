# Makefile for MrmMngr v1.0 - RM-TEAM 04/2022

GCC=gcc
CC=$(GCC)

PREFIX=/usr/local
INSTALL=install -c
RM=rm -vf

EXE=mrmmngr
SUBDIR_BASE=src

DATADIR=$(PREFIX)/share/$(EXE)
DATAFILES=mrm-boot.bin mrm-ldr.bin unpckpsc.bin

CFLAGS=-O2 -Wall -I$(SUBDIR_BASE) -DMRM_DATA_DIR='"$(DATADIR)"'
CXXFLAGS=$(CFLAGS)
# macOS linker: `-s` is obsolete
ifneq ($(shell uname -s),Darwin)
LDFLAGS=-s
endif

# object files
OBJS=\
	$(SUBDIR_BASE)/utils.o \
	$(SUBDIR_BASE)/file.o \
	$(SUBDIR_BASE)/mrm.o \
	$(SUBDIR_BASE)/main.o

all: $(OBJS)
	$(GCC) -o $(EXE) $(CXXFLAGS) $(OBJS) $(LDFLAGS)

install:
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(DATADIR)
	$(INSTALL) $(EXE) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 644 $(DATAFILES) $(DESTDIR)$(DATADIR)

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(EXE)
	$(RM) -r $(DESTDIR)$(DATADIR)

.c.o:
	$(GCC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) \
		$(SUBDIR_BASE)/*.o \
		$(EXE) *~
