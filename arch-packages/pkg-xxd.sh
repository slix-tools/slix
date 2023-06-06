name=xxd
deps="vim-runtime gpm acl glibc libgcrypt zlib perl"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
