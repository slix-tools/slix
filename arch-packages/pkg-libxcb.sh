name=libxcb
deps="xcb-proto libxdmcp libxau"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
