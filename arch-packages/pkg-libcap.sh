name=libcap
deps="gcc-libs glibc pam"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
