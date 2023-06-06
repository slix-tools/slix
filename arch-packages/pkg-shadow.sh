name=shadow
deps="acl attr audit glibc libxcrypt pam"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
