name=gcc13
archpkg=gcc
defaultcmd=""
deps="glibc gcc-libs binutils libmpc zstd libisl"

./preparePackage.sh "${name}" "${archpkg}" "${deps}"

cp -a --remove-destination data/gcc/gcc ${name}/rootfs/usr/bin/gcc
cp -a --remove-destination data/gcc/g++ ${name}/rootfs/usr/bin/g++

./finalizePackage.sh "${name}" "${archpkg}" "${defaultcmd}"
