name=gcc
deps="gcc-libs binutils libmpc zstd libisl"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
