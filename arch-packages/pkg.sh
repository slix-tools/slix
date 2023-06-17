set -Eeuo pipefail

if [ -z "${SLIX_INDEX}" ]; then
    echo "Set SLIX_INDEX to path of index.db"
    exit 1;
fi

if [ -e "allreadyBuild.txt" ] && [ $(cat allreadyBuild.txt | grep "^${archpkg}$" | wc -l) -gt 0 ]; then
    exit 0
fi

./preparePackage.sh ${name} ${archpkg} "${deps}"
./finalizePackage.sh ${name} ${archpkg}
