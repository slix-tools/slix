name=llvm-libs
deps="gcc-libs zlib zstd libffi libedit ncurses libxml2"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
