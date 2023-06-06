name=libnetfilter_conntrack
deps="libnfnetlink libmnl"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
