#!/usr/bin/env bash

mkdir -p obj
FLAGS="-std=c++20 -D_FILE_OFFSET_BITS=64"
FLAGS="${FLAGS} -ggdb"

g++ ${FLAGS} -c src/InteractiveProcess.cpp -o obj/InteractiveProcess.o

g++ ${FLAGS} -c src/fuse-union.cpp -o obj/fuse-union.cpp.o
g++ obj/fuse-union.cpp.o obj/InteractiveProcess.o -lfuse -larchive -o fuse-union

g++ ${FLAGS} -c src/archive.cpp -o obj/archive.cpp.o
g++ obj/archive.cpp.o -lfuse -larchive -o archive
