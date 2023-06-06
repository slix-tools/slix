name=libtiff
deps="glibc libjpeg-turbo zlib xz zstd"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
