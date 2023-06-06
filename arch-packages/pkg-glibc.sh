name=glibc
deps="linux-api-headers tzdata filesystem"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
