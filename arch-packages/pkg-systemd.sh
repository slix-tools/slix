name=systemd
deps="acl bash cryptsetup dbus iptables kbd kmod hwdata libcap libgcrypt libxcrypt systemd-libs libidn2 lz4 pam libelf libseccomp util-linux xz pcre2 audit openssl"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
