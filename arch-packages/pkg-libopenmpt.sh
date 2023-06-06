name=libopenmpt
deps="flac gcc-libs glibc libogg libpulse libsndfile libvorbis mpg123 portaudio zlib"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
