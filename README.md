# sample_replay
This repository will add some information about libkss (https://github.com/digital-sound-antiques/libkss.git)
and a command line kss player that uses libkss.

LIBKSS is a music player library for MSX music formats, forked from MSXplug.
Supported formats are .kss, .mgs, .bgm, .opx, .mpk, .mbm.

CMake would create a building environment on Windows OS (it requires VS) and on GNU/Linux OS (it requires GNU toolchain).

# How to build

The following step builds `libkss.a` library.

```
$ git clone --recursive https://github.com/digital-sound-antiques/libkss.git
$ cd libkss
$ mkdir build
$ cd build
$ cmake ..
$ make
```

You can also build the KSS to WAV converter binary as follows.

```
$ make kss2wav
```

You can also build a stand alone MSX music formats player.
In order to build sample_replay player, do as follows:
1 - clone this repository
$ git clone https://github.com/gzaffin/sample_replay

2 - add libkss's repository folders 'modules' and 'src' to sample_replay's repository
$ cd libkss
$ cp -r modules ./../sample_replay
$ cp -r src ./../sample_replay

3 - if You're going to build libkss with Microsoft Visual Studio 2015 or Microsoft Visual Studio 2017, which are recommended otherwise You'll face ISO/IEC 9899:1999 C code that do not compile and You need to re-factor it a bit, check that Your CMake version is >= 3.5, and change first row of CMakeLists.txt into e,g, 'cmake_minimum_required(VERSION 3.6)' for compiling with Microsoft Visual Studio 2017, then create a build folder, invoke cmake and compile with Visual Studio

4 - if You're going to build under GNU/Linux and You do not want to use libao remove -DUSE_LIBAO from line 6 of CMakeLists.txt

5 - under GNU/Linux create a build folder, invoke cmake and make

```
$ cd libkss
$ mkdir build
$ cd build
$ cmake ../
$ make sample_replay
```

If You want add a 'data' folder with following files You are welcomed to do so, but this is not necessary: 
KINROU5.DRV, OPX4KSS.BIN, FMPAC.ROM, MBR143.001, MPK.BIN and MPK103.BIN

These are MSX binaries files. Among other places they can be found here:

mbr143.001 MOONBLASTER REPLAY ROUTINE v1.43 by W. Brants (c) 1992/1993 see NOTE URL: http://nezplug.sourceforge.net/mbm2kssx3.zip

MPK.BIN & MPK103.BIN Music Player K-kaz system (c) 1993 K-KAZ URL: http://xray.delta-z.org/mpk.html 

KINROU5.DRV Kinrou 5th Version 2.00 Copyright (C) 1996,1997 Keiichi Kuroda/BTO(MuSICA Laboratory) URL: http://sakuramail.net/fswold/music/kin5200.lzh

OPX4KSS.BIN by Mikasen e.g. http://aminet.net/package/mus/play/tunemsx_plug.lha 

FMPAC.ROM of Panamusement FM PAC e.g. http://fms.komkon.org/fMSX/ 

NOTE - Inside mbm2kssx3.zip is another archive, called mbr143.lzh. Extract mbr143.001 from mbr143.lzh. 


