name=kmod
deps="glibc zlib openssl xz zstd"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
