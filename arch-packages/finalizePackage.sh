#!/usr/bin/env bash

set -Eeuo pipefail

pkg=${1}
shift

SLIX_PKG_PATHS=${SLIX_PKG_PATHS:-${HOME}/.config/slix/packages}
if [ -z ${SLIX_PKG_PATHS} ]; then
    echo "SLIX_PKG_PATHS not set"
    exit
fi

# check if the package itself exists
# !TODO SLIX_PKG_PATHS could be multiple packages
if [ $(find ${SLIX_PKG_PATHS} | grep "/${pkg}@" | wc -l) -gt 0 ]; then
    exit
fi


target=${pkg}
version=$(pacman -Qi ${pkg} \
    | grep -P "^Version" \
    | tr '\n' ' ' \
    | awk '{print $3}')

slix archive --input ${target} --output ${target}.gar
hash=$(sha256sum -b ${target}.gar | awk '{print $1}')
destFile="${target}@${version}-$hash.gar"
if [ ! -e "${destFile}" ]; then
    if [ $(cat ${target}/dependencies.txt | grep Missing.gar | wc -l) -ge 1 ]; then
        echo "${target} is missing dependencies:"
        cat ${target}/dependencies.txt | grep Missing.gar
        rm ${target}.gar
    else
        # !TODO SLIX_PKG_PATHS could be multiple paths....
        mv ${target}.gar ${SLIX_PKG_PATHS}/${destFile}
        echo "created ${destFile}"
    fi
else
    echo "${destFile} already existed"
    rm ${target}.gar
fi

rm -rf ${target}
