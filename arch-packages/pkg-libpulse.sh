name=libpulse
deps="dbus libasyncns libsndfile libxcb systemd"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
