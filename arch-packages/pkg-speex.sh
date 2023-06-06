name=speex
deps="gcc-libs libogg speexdsp"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
