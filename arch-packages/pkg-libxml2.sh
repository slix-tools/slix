name=libxml2
deps="icu ncurses readline xz zlib"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
