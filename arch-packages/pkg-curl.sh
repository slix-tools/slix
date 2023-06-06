name=curl
deps="ca-certificates brotli krb5 libidn2 libnghttp2 libpsl libssh2 openssl zlib zstd"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
