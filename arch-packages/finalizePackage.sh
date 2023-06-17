#!/usr/bin/env bash

set -Eeuo pipefail

name=${1}
shift

archpkg=${1}
shift

if [ -z "${SLIX_INDEX}" ]; then
    echo "Set SLIX_INDEX to path of index.db"
    exit 1;
fi

version=$(pacman -Qi ${archpkg} \
    | grep -P "^Version" \
    | tr '\n' ' ' \
    | awk '{print $3}')

latest=$(slix index info ${SLIX_INDEX} --name "${name}" | tail -n 1);
aname="$(echo ${latest} | cut -d '#' -f 1)"
if [ "${aname}" == "${name}@${version}" ]; then
    echo "${name} already build, known as ${latest}"
    exit 0
fi

target=${name}

slix archive --input ${target} --output ${target}.gar
if [ $(cat ${target}/dependencies.txt | grep Missing.gar | wc -l) -ge 1 ]; then
    echo "${target} is missing dependencies:"
    cat ${target}/dependencies.txt | grep Missing.gar
    rm ${target}.gar
else
    slix index add ${SLIX_INDEX} --package ${target}.gar --name "${target}" --version "${version}"
    rm ${target}.gar
fi

rm -rf ${target}
