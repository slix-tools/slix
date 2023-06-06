name=libass
deps="fontconfig fribidi glib2 glibc harfbuzz freetype2"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
