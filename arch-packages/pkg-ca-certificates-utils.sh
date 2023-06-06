name=ca-certificates-utils
deps="bash coreutils findutils p11-kit"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
