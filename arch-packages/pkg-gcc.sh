name=gcc
deps="slix-ld gcc-libs binutils libmpc zstd libisl"

./preparePackage.sh "${name}" "${deps}"

rm ${name}/rootfs/usr/bin/gcc
(
    echo "#!/usr/bin/env bash"
    echo "slix-ld-gcc --sysroot \${SLIX_ROOT} \"\$@\""
) > ${name}/rootfs/usr/bin/gcc
chmod +x ${name}/rootfs/usr/bin/gcc

rm ${name}/rootfs/usr/bin/g++
(
    echo "#!/usr/bin/env bash"
    echo "slix-ld-g++ --sysroot \${SLIX_ROOT} \"\$@\""
) > ${name}/rootfs/usr/bin/g++
chmod +x ${name}/rootfs/usr/bin/g++

./finalizePackage.sh "${name}"
