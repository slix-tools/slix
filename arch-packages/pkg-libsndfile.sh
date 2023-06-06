name=libsndfile
deps="glibc flac lame libogg libvorbis mpg123 opus"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
