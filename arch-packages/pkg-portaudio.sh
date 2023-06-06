name=portaudio
deps="gcc-libs glibc alsa-lib jack2"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
