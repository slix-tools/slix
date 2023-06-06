name=freetype2
deps="brotli bzip2 harfbuzz libpng bash zlib"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
