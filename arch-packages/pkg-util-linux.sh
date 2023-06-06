name=util-linux
deps="pam shadow coreutils systemd-libs libcap-ng libxcrypt util-linux-libs file ncurses"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
