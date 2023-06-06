name=libsasl
deps="gdbm glibc openssl"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
