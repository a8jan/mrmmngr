# MRM Manager

[![Build](https://github.com/a8jan/mrmmngr/actions/workflows/build.yml/badge.svg)](https://github.com/a8jan/mrmmngr/actions/workflows/build.yml)

MRM Manager (`mrmmngr`) is a console utility for managing MRM images: files
that are burned into the EPROM/FLASH memory of the
[ROM MEGAmodule](https://pmd85.borik.net/wiki/ROM_MEGAmodul) for the PMD 85
family of computers. An image holds the boot code, a directory and the stored
files. PMD 85 emulators with ROM MEGAmodule support, such as
[PMD 85 Emulator](https://pmd85.borik.net/wiki/Emulator) and
[GPMD85Emulator](https://github.com/mborik/GPMD85emulator), can load the image
file directly, which is a convenient way to load programs or games.

The utility was written by Roman Bórik (RM-TEAM). Its home page, with the
original documentation (in Slovak) and downloads, is
<https://pmd85.borik.net/wiki/MRM_Manager>.

This repository is based on the original v1.3 sources (06/2022) and adds:

- a build that works on Linux and macOS, and on Windows with both MinGW-w64
  and MSVC,
- `make install` / `make uninstall` with `PREFIX` and `DESTDIR` support,
- an automated build and smoke test on all of these platforms,
- a build without compiler warnings, and fixes for two small bugs (the value
  shown in the `-sa` error message, and validation of `#HH` escapes in complex
  file names).

## Building

### Linux, macOS

```sh
make
sudo make install     # optional, installs to /usr/local
```

`make install` puts the executable in `$(PREFIX)/bin` and the data files in
`$(PREFIX)/share/mrmmngr`. `PREFIX` defaults to `/usr/local`; pass the same
`PREFIX` to both `make` and `make install` if you change it.

### Windows, MSYS2 (MinGW-w64)

In an MSYS2 UCRT64 shell with `make` and `mingw-w64-ucrt-x86_64-gcc`
installed:

```sh
make
```

### Windows, MSVC

In a Visual Studio "x64 Native Tools" command prompt:

```bat
cl /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS /Isrc src\utils.c src\file.c src\mrm.c src\main.c /Fe:mrmmngr.exe
```

## Data files

MRM Manager needs three binary files that are part of this repository:

| File           | Purpose                                                      |
|----------------|--------------------------------------------------------------|
| `mrm-boot.bin` | boot code placed at the start of every MRM image             |
| `mrm-ldr.bin`  | universal loader, used by `-ml`                              |
| `unpckpsc.bin` | unpacking routine for packed screens, inserted by `-psc`     |

Where they are looked up:

- **Linux:** next to the executable, otherwise in `$(PREFIX)/share/mrmmngr`.
- **macOS:** in `$(PREFIX)/share/mrmmngr`, so run `make install` first.
- **Windows:** in the directory given in the path used to start the program,
  otherwise in the current directory.

On any platform `-ib path/mrm-boot.bin` selects the boot file explicitly; the
other two files are then taken from the same directory.

## Usage

Run `mrmmngr` without arguments, or see [usage.txt](usage.txt), for the full
list of options. A short example:

```sh
# create a new 512 kB image
mrmmngr game.mrm -ic

# add a program as GAME.? with a long name, then show the catalogue;
# type "?" is the convention for monitor (MGLD) files, such as binaries
# produced by z88dk, and -al lists the program in the boot menu
mrmmngr game.mrm -fa game.bin -fn "GAME.?" -ln "My Game" -al
mrmmngr game.mrm -c

# show image info, extract all files
mrmmngr game.mrm -ii
mrmmngr game.mrm -fx "*"
```

## License

MIT, see [LICENSE.txt](LICENSE.txt). Copyright (c) 2022 RM-TEAM (Roman &
Martin Bórik). The changes made in this repository are provided under the same
license.
