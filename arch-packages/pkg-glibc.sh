name=glibc
archpkg=${name}
defaultcmd=""
deps="slix-ld"

if [ -e "allreadyBuild.txt" ] && [ $(cat allreadyBuild.txt | grep "^${archpkg}$" | wc -l) -gt 0 ]; then
    exit 0
fi
./preparePackage.sh "${name}" "${archpkg}" "${deps}"

rm -r ${name}/rootfs/etc
rm -r ${name}/rootfs/var

./finalizePackage.sh "${name}" "${archpkg}" "${defaultcmd}"
