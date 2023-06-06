name=binutils
deps="glibc jansson libelf zlib zstd"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
