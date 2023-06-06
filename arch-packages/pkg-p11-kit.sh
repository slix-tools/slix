name=p11-kit
deps="libp11-kit coreutils systemd-libs"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
