name=sdl2
deps="glibc libxext libxrender libx11 libglvnd libxcursor hidapi libusb"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
