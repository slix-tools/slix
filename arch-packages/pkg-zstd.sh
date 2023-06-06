name=zstd
deps="glibc gcc-libs zlib xz lz4"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
