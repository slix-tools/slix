name=audit
deps="glibc krb5 libcap-ng"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
