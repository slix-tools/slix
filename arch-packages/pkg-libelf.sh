name=libelf
deps="bzip2 curl gcc-libs glibc xz zlib zstd"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
