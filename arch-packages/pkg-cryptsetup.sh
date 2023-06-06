name=cryptsetup
deps="device-mapper openssl popt util-linux-libs json-c argon2"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
