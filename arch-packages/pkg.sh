set -Eeuo pipefail

if [ -z ${SLIX_ROOT} ]; then
    echo "SLIX_ROOT not set"
    exit
fi

if find ${SLIX_ROOT} | grep "/${name}@" > /dev/null; then
    exit
fi

for d in $deps; do
    if [ "$d" != slix-ld ]; then
        bash pkg-$d.sh
    fi
done
./createPackage.sh ${name} ${deps}
