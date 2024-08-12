cmake_minimum_required(VERSION 3.5)
project(sample_replay)

# add_compile_definitions(EXCLUDE_DRIVER_ALL)

if(MSVC)
  set(CMAKE_C_FLAGS "/Ox /W3 /wd4996 /DWIN32")
else()
  set(CMAKE_C_FLAGS "-O3 -Wall")
endif()

if(NOT TARGET emu2149) 
	add_subdirectory(modules/libkss/modules/emu2149) 
endif()

if(NOT TARGET emu2212)
	add_subdirectory(modules/libkss/modules/emu2212)
endif()

if(NOT TARGET emu2413)
	add_subdirectory(modules/libkss/modules/emu2413)
endif()

if(NOT TARGET emu8950)
	add_subdirectory(modules/libkss/modules/emu8950)
endif()

if(NOT TARGET emu76489)
	add_subdirectory(modules/libkss/modules/emu76489)
endif()

if(NOT TARGET kmz80)
	add_subdirectory(modules/libkss/modules/kmz80)
endif()

file(GLOB SOURCES modules/libkss/src/filters/*.c modules/libkss/src/kss/*.c modules/libkss/src/rconv/*.c modules/libkss/src/vm/*.c  modules/libkss/src/kssplay.c)

include_directories(modules/libkss/modules modules/libkss/src)

add_library(kss STATIC ${SOURCES})
target_link_libraries(kss emu2149 emu2212 emu2413 emu8950 emu76489 kmz80)

add_executable(kss2wav EXCLUDE_FROM_ALL modules/libkss/example/kss2wav.c)
if(NOT CMAKE_COMPILER_IS_GNUCC)
	target_link_libraries(kss2wav PRIVATE kss)
else()
	target_link_libraries(kss2wav PRIVATE kss m)
endif()

add_executable(kss2vgm EXCLUDE_FROM_ALL modules/libkss/src/kss2vgm.c)
if(NOT CMAKE_COMPILER_IS_GNUCC)
	target_link_libraries(kss2vgm PRIVATE kss)
else()
	target_link_libraries(kss2vgm PRIVATE kss m)
endif()

add_executable(sample_replay EXCLUDE_FROM_ALL src/sample_replay.c)
if (MSVC)
	target_link_libraries(sample_replay PRIVATE kss kernel32 winmm)
else()
	target_link_libraries(sample_replay PRIVATE kss m)
endif()
