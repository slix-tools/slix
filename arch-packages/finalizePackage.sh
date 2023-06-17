#!/usr/bin/env bash

set -Eeuo pipefail

pkg=${1}
shift

if [ -z "${SLIX_INDEX}" ]; then
    echo "Set SLIX_INDEX to path of index.db"
    exit 1;
fi

version=$(pacman -Qi ${pkg} \
    | grep -P "^Version" \
    | tr '\n' ' ' \
    | awk '{print $3}')

latest=$(slix index info ${SLIX_INDEX} --name "${pkg}" | tail -n 1);
name="$(echo ${latest} | cut -d '#' -f 1)"
if [ "${name}" == "${pkg}@${version}" ]; then
    echo "${pkg} already build, known as ${latest}"
    exit 0
fi


target=${pkg}

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
