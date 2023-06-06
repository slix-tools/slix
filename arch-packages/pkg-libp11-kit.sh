name=libp11-kit
deps="glibc libtasn1 libffi"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
