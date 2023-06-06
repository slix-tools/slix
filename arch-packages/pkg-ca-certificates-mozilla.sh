name=ca-certificates-mozilla
deps="ca-certificates-utils"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
