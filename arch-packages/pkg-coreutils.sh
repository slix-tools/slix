name=coreutils
deps="glibc acl attr gmp libcap openssl"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
