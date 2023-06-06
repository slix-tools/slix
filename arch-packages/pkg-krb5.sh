name=krb5
deps="glibc e2fsprogs libldap keyutils libverto"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
