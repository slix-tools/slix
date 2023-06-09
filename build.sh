#!/usr/bin/env bash

mkdir -p build/{obj,bin}
FLAGS="-std=c++20 -D_FILE_OFFSET_BITS=64 -isystem libs/clice/src"
FLAGS="${FLAGS} -ggdb -O0"

g++ ${FLAGS} -c src/slix-archive.cpp -o build/obj/slix-archive.cpp.o
g++ ${FLAGS} -c src/slix-mount.cpp -o build/obj/slix-mount.cpp.o
g++ ${FLAGS} -c src/slix-search.cpp -o build/obj/slix-search.cpp.o
g++ ${FLAGS} -c src/slix-shell.cpp -o build/obj/slix-shell.cpp.o
g++ ${FLAGS} -c src/slix.cpp -o build/obj/slix.cpp.o

g++ build/obj/slix.cpp.o \
    build/obj/slix-archive.cpp.o \
    build/obj/slix-mount.cpp.o \
    build/obj/slix-search.cpp.o \
    build/obj/slix-shell.cpp.o \
    -lfuse -lfmt -o build/bin/slix


g++ ${FLAGS} -c src/slix-ld.cpp -o build/obj/slix-ld.cpp.o -static
g++ build/obj/slix-ld.cpp.o -o build/bin/slix-ld -static
