#!/usr/bin/env bash

mkdir -p build/{obj,bin}
FLAGS="-std=c++20 -D_FILE_OFFSET_BITS=64"
FLAGS="${FLAGS} -ggdb -O0"

g++ ${FLAGS} -c src/InteractiveProcess.cpp -o build/obj/InteractiveProcess.o

g++ ${FLAGS} -c src/slix-shell.cpp -o build/obj/slix-shell.cpp.o
g++ build/obj/slix-shell.cpp.o build/obj/InteractiveProcess.o -lfuse -o build/bin/slix-shell

g++ ${FLAGS} -c src/slix-ld.cpp -o build/obj/slix-ld.cpp.o -static
g++ build/obj/slix-ld.cpp.o -o build/bin/slix-ld -static

g++ ${FLAGS} -c src/archive.cpp -o build/obj/archive.cpp.o
g++ build/obj/archive.cpp.o -lfuse -o build/bin/archive

g++ ${FLAGS} -c src/packageAnalyzer.cpp -o build/obj/packageAnalyzer.cpp.o
g++ build/obj/packageAnalyzer.cpp.o -lfuse -o build/bin/packageAnalyzer
