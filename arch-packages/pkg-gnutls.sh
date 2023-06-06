name=gnutls
deps="gcc-libs libtasn1 readline zlib nettle p11-kit libidn2 zstd libunistring brotli"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
