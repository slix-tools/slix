name=libjxl
deps="brotli giflib gperftools highway libjpeg-turbo libpng openexr"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
