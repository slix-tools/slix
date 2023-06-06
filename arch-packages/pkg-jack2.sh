name=jack2
deps="alsa-lib db5.3 dbus gcc-libs glibc libsamplerate opus systemd-libs"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
