set -Eeuo pipefail

if [ -z "${SLIX_INDEX}" ]; then
    echo "Set SLIX_INDEX to path of index.db"
    exit 1;
fi
version=$(pacman -Qi ${archpkg} \
    | grep -P "^Version" \
    | tr '\n' ' ' \
    | awk '{print $3}')

latest=$(slix index info ${SLIX_INDEX} --name "${name}" | tail -n 1);
pkgname="$(echo ${latest} | cut -d '#' -f 1)"
if [ "${pkgname}" == "${name}@${version}" ]; then
    echo "${name} already build, known as ${latest}"
    exit 0
fi

./preparePackage.sh ${name} ${archpkg} "${deps}"
./finalizePackage.sh ${name} ${archpkg}
