# sample_replay
This repository will add a command line player to libkss (URL: https://github.com/digital-sound-antiques/libkss.git) .  

LIBKSS is a music player library for MSX music formats, forked from MSXplug.  
Supported formats are .kss, .mgs, .bgm, .opx, .mpk, .mbm.  

CMake would create a building environment on Windows OS (it requires Microsoft Visual Studio) and on GNU/Linux OS (it requires GNU toolchain).  

## How to build
The following steps build `libkss` library, build `sample_replay` application statically linked with `libkss`.  

```
$ git clone https://github.com/gzaffin/sample_replay.git
$ cd sample_replay
$ git submodule update --init --recursive
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build . --config Release --target sample_replay
```

These are MSX binaries files: KINROU5.DRV, OPX4KSS.BIN, FMPAC.ROM, MBR143.001, MPK.BIN and MPK103.BIN.  
Among other places they can be found here:  
 mbr143.001 MOONBLASTER REPLAY ROUTINE v1.43 by W. Brants (c) 1992/1993 see NOTE URL: http://nezplug.sourceforge.net/mbm2kssx3.zip  
 MPK.BIN & MPK103.BIN Music Player K-kaz system (c) 1993 K-KAZ URL: http://xray.delta-z.org/mpk.html  
 KINROU5.DRV Kinrou 5th Version 2.00 Copyright (C) 1996,1997 Keiichi Kuroda/BTO(MuSICA Laboratory) URL: http://sakuramail.net/fswold/music/kin5200.lzh  
 OPX4KSS.BIN by Mikasen e.g. http://aminet.net/package/mus/play/tunemsx_plug.lha  
 FMPAC.ROM of Panamusement FM PAC e.g. http://fms.komkon.org/fMSX/  
NOTE - Inside mbm2kssx3.zip is another archive, called mbr143.lzh. Extract mbr143.001 from mbr143.lzh.  

