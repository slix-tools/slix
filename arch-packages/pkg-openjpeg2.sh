name=openjpeg2
deps="zlib libpng libtiff lcms2"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
