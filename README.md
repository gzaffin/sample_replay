# sample_replay
This repository attach a command line player to [LIBKSS](https://github.com/digital-sound-antiques/libkss.git) library .  

LIBKSS is a music player library for MSX music formats, forked from MSXplug.  
Supported formats are .kss, .mgs, .bgm, .opx, .mpk, .mbm.  

Here the focus is building from source with Visual Studio's MSVC + vcpkg + CMake in Windows 10/Windows 11.  

## How to build
Use Microsoft's Visual Studio with Microsoft Windows o.s . [Install Visual Studio](https://learn.microsoft.com/en-us/visualstudio/install/install-visual-studio?view=vs-2022) .
Use vcpkg cross-platform C/C++ package manager with Microsoft Windows o.s . [Install and use packages with vcpkg](https://learn.microsoft.com/en-us/vcpkg/commands/install) .

As it can be seen from CMakeLists.txt, the following two packages are required and in Windows they are managed by vcpkg:  
1. libiconv  
2. SDL3  

Install packages with vcpkg with [vcpkg install <package>...](https://learn.microsoft.com/en-us/vcpkg/commands/install) command  
```
PS C:\<PATH>\vcpkg>vcpkg install libiconv:x64-windows sdl3:x64-windows
```
Clone repository and compile sources  
```
PS C:\<PATH>>git clone https://github.com/gzaffin/sample_replay.git
PS C:\<PATH>>cd sample_replay
PS C:\<PATH>>git submodule update --init --recursive
PS C:\<PATH>\libvgm>mkdir build
PS C:\<PATH>\libvgm>cd build
PS C:\<PATH>\libvgm\build>cmake -G "Visual Studio 17 2022" -A x64 -T host=x64 -D CMAKE_TOOLCHAIN_FILE=C:/<PATH>/vcpkg/scripts/buildsystems/vcpkg.cmake ..
PS C:\<PATH>\libvgm\build>cmake --build . --config Release --target sample_replay
```

For Linux, and Mac, CMakeLists.txt should almost work. Many GNU/Linux ditributions have in-built GNU libc iconv, and SDL3 package should be installed.
```
$ git clone https://github.com/gzaffin/sample_replay.git
$ cd sample_replay
$ git submodule update --init --recursive
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build . --config Release --target sample_replay
```

## Infos
These are MSX binaries files Libkss uses: KINROU5.DRV, OPX4KSS.BIN, FMPAC.ROM, MBR143.001, MPK.BIN and MPK103.BIN.  
Among other places, they can be found here:  
 mbr143.001 MOONBLASTER REPLAY ROUTINE v1.43 by W. Brants (c) 1992/1993 see NOTE URL: http://nezplug.sourceforge.net/mbm2kssx3.zip  
 MPK.BIN & MPK103.BIN Music Player K-kaz system (c) 1993 K-KAZ URL: http://xray.delta-z.org/mpk.html  
 KINROU5.DRV Kinrou 5th Version 2.00 Copyright (C) 1996,1997 Keiichi Kuroda/BTO(MuSICA Laboratory) URL: http://sakuramail.net/fswold/music/kin5200.lzh  
 OPX4KSS.BIN by Mikasen e.g. http://aminet.net/package/mus/play/tunemsx_plug.lha  
 FMPAC.ROM of Panamusement FM PAC e.g. http://fms.komkon.org/fMSX/  
NOTE - Inside mbm2kssx3.zip is another archive, called mbr143.lzh. Extract mbr143.001 from mbr143.lzh.  

