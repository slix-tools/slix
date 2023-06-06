name=ca-certificates
deps="ca-certificates-mozilla"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
