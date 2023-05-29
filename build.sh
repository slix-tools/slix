#!/usr/bin/env bash

mkdir -p obj
FLAGS="-std=c++20 -D_FILE_OFFSET_BITS=64"
FLAGS="${FLAGS} -ggdb -O0"

g++ ${FLAGS} -c src/InteractiveProcess.cpp -o obj/InteractiveProcess.o

g++ ${FLAGS} -c src/slix-shell.cpp -o obj/slix-shell.cpp.o
g++ obj/slix-shell.cpp.o obj/InteractiveProcess.o -lfuse -o slix-shell

g++ ${FLAGS} -c src/slix-ld.cpp -o obj/slix-ld.cpp.o -static
g++ obj/slix-ld.cpp.o -o slix-ld -static

g++ ${FLAGS} -c src/archive.cpp -o obj/archive.cpp.o
g++ obj/archive.cpp.o -lfuse -o archive
