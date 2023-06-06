name=glib2
deps="libffi libsysprof-capture pcre2 util-linux-libs zlib"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
