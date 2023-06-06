name=systemd-libs
deps="glibc gcc-libs libcap libgcrypt lz4 xz zstd"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
