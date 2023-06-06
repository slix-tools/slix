name=v4l-utils
deps="hicolor-icon-theme gcc-libs libjpeg-turbo systemd-libs json-c"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
