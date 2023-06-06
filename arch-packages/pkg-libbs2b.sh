name=libbs2b
deps="libsndfile gcc-libs"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
