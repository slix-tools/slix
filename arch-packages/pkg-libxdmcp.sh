name=libxdmcp
deps="glibc xorgproto"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
