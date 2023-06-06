name=harfbuzz
# Broken, actually also depends on freefont2
deps="glib2"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
