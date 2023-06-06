#!/usr/bin/env bash

set -Eeuo pipefail

pkg=${1}
target=${1}
shift

if [ -z ${SLIX_ROOT} ]; then
    echo "SLIX_ROOT not set"
    exit
fi

# check if the package itself exists
if [ $(find ${SLIX_ROOT} | grep "/${pkg}@" | wc -l) -gt 0 ]; then
    exit
fi

root=${target}/rootfs
mkdir -p ${root}
mkdir -p ${root}/slix-bin

# check dependencies
echo -n "" > ${target}/dependencies.txt
while [ ${#@} -gt 0 ]; do
    d=$1
    d=$(realpath --relative-to ${SLIX_ROOT} $(find ${SLIX_ROOT} | grep $d'@'))
    echo $d >> ${target}/dependencies.txt
    shift
done

pacman -Ql ${pkg} | awk '{ print $2; }' | (
    while IFS='$\n' read -r line; do
        if [ -d $line ] && [ ! -h $line ]; then
            mkdir -p ${root}/${line}
        elif [ -e $line ]; then
            cp -a ${line} ${root}/${line}

            ############################
            # sanity check of every file
            ############################

            # if absolute sym link, change to relative
            if [ -L ${root}/${line} ]; then
                l=$(readlink ${root}/${line})
                if [ ${l:0:1} == "/" ]; then
                    ln -rsf ${root}/${l} ${root}/${line}
                fi
            fi

            if [ ! -L ${root}/${line} ] && [ -x ${root}/${line} ]; then
                t=$(file -b -h --mime-type ${root}/${line})

                # patch ld-linux.so.2 (interpreter of binaries)
                if [ "${t}" == "application/x-executable" ] \
                     || [ "${t}" == "application/x-pie-executable" ]; then
                    if [[ ${line} =~ ^/usr/bin/[^/]*$ ]]; then
                        file=$(basename ${line})
                        mv ${root}/usr/bin/${file} ${root}/slix-bin
                        ln -sr ${root}/usr/bin/slix-ld ${root}/usr/bin/${file}
                    fi

                # patch shell scripts
                elif [ "${t}" == "text/x-shellscript" ]; then
                    inter=$(head -n 1 ${root}/${line})
                    if [ "${inter}" == "#!/bin/sh" ] \
                        || [ "${inter}" == "#! /bin/sh" ] \
                        || [ "${inter}" == "#!/bin/sh -" ]; then
                        sed -i '1s#.*#/usr/bin/env sh#' ${root}/${line}
                    elif [ "${inter}" == "#!/bin/bash" ]; then
                        sed -i '1s#.*#/usr/bin/env bash#' ${root}/${line}
                    else
                        echo "${root}/${line} needs fixing, unexpected shell interpreter: ${inter}"
                    fi
                fi
            fi
        fi
    done
)

version=$(pacman -Qi ${pkg} \
    | grep -P "^Version" \
    | tr '\n' ' ' \
    | awk '{print $3}')

../build/bin/archive ${target}
hash=$(sha256sum -b ${target}.gar | awk '{print $1}')
destFile="${target}@${version}-$hash.gar"
if [ ! -e "${destFile}" ]; then
    if [ $(cat ${target}/dependencies.txt | grep Missing.gar | wc -l) -ge 1 ]; then
        echo "${target} is missing dependencies:"
        cat ${target}/dependencies.txt | grep Missing.gar
        rm ${target}.gar
    else
        mv ${target}.gar ${SLIX_ROOT}/${destFile}
        echo "created ${destFile}"
    fi
else
    echo "${destFile} already existed"
    rm ${target}.gar
fi

rm -rf ${target}
