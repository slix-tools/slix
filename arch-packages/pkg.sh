set -Eeuo pipefail

SLIX_PKG_PATHS=${SLIX_PKG_PATHS:-${HOME}/.config/slix/packages}
if [ -z ${SLIX_PKG_PATHS} ]; then
    echo "SLIX_PKG_PATHS not set"
    exit
fi

#!TODO SLIX_PKG_PATHS could be multiple packages
if find ${SLIX_PKG_PATHS} | grep "/${name}@" > /dev/null; then
    echo "${name} already build"
    exit
fi

for d in $deps; do
    if [ "$d" != slix-ld ]; then
        bash pkg-$d.sh
    fi
done

./preparePackage.sh ${name} "${deps}"
./finalizePackage.sh ${name}
