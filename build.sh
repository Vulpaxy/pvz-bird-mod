#!/bin/sh
# Builds birdmod.dll and birdmod_launcher.exe (32-bit) with llvm-mingw.
set -e
cd "$(dirname "$0")"
# Any 32-bit MinGW C++ compiler works; default is llvm-mingw's, from PATH or ./toolchain.
if [ -z "$CXX" ]; then
    CXX=i686-w64-mingw32-clang++
    for d in toolchain/llvm-mingw-*/bin; do [ -x "$d/$CXX" ] && CXX="$d/$CXX"; done
fi
FLAGS="-std=c++17 -O2 -Wall -static -municode"
mkdir -p dist
DLL="${DLL:-birdmod.dll}"   # DLL=birdmod_dbg.dll sh build.sh  -> builds while birdmod.dll is loaded in the game
"$CXX" $FLAGS -shared -o "dist/$DLL" src/birdmod.cpp src/assets.cpp \
    -lgdi32 -lmsimg32 -lwinmm -luser32 -lole32 -lshlwapi -lwindowscodecs -luuid
"$CXX" $FLAGS -mwindows -o dist/birdmod_launcher.exe src/launcher.cpp -lshell32 -luser32
echo built: dist/birdmod.dll dist/birdmod_launcher.exe
