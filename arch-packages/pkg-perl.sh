name=perl
deps="gdbm db5.3 glibc libxcrypt"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
