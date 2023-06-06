name=libbluray
deps="fontconfig freetype2 libxml2"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
