name=libva
deps="libdrm libglvnd libx11 libxext libxfixes wayland"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
