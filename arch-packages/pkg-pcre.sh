name=pcre
deps="gcc-libs readline zlib bzip2 bash"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
