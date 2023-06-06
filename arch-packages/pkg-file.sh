name=file
deps="glibc zlib xz bzip2 libseccomp zstd"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
