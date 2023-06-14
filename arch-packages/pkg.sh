set -Eeuo pipefail

if [ -n "$(slix search "${name}@")" ]; then
    echo "${name} already build, known as $(slix search "${name}@")"
    exit 1
fi

./preparePackage.sh ${name} "${deps}"
./finalizePackage.sh ${name}
