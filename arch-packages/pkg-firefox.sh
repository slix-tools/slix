name=firefox
deps="slix-ld dbus-glib ffmpeg gtk3 libpulse libxt mime-types nss gnu-free-fonts sed bash util-linux coreutils"
./preparePackage.sh "${name}" "${deps}"

cp --remove-destination data/firefox/firefox ${name}/rootfs/usr/bin/firefox
./finalizePackage.sh "${name}"
