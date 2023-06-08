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

# check dependencies
echo -n "" > ${target}/dependencies.txt
while [ ${#@} -gt 0 ]; do
    d="$1"
    p="$(find ${SLIX_ROOT} | grep $d'@' || true)"
    if [ -z "${p}" ]; then
        echo "$pkg dependency $d is missing"
        exit 1
    fi
    d=$(realpath --relative-to ${SLIX_ROOT} $p)
    echo $d >> ${target}/dependencies.txt
    shift
done

pacman -Ql ${pkg} | awk '{ print $2; }' | (
    while IFS='$' read -r line; do
        if [ -d $line ] && [ ! -h $line ]; then
            mkdir -p ${root}/${line:1}
        elif [ -e $line ]; then
            if [ ! -r $line ]; then
                echo "no read access for ${line}"
                continue
            fi
            cp -a ${line} ${root}/${line:1}

            ############################
            # sanity check of every file
            ############################

            # if absolute sym link, change to relative
            if [ -L ${root}/${line:1} ]; then
                l=$(readlink ${root}/${line:1})
                if [ ${l:0:1} == "/" ]; then
                    l=${l:1}
                    echo ${root}/${l} to ${root}/${line:1}
                    ln -rsf ${root}/${l} ${root}/${line:1}
                fi
            fi

            # Fix non sym link executables
            if [ ! -L ${root}/${line:1} ] && [ -x ${root}/${line:1} ]; then
                t=$(file -b -h --mime-type ${root}/${line:1})

                # patch ld-linux.so.2 (interpreter of binaries)
                if [ "${t}" == "application/x-executable" ] \
                     || [ "${t}" == "application/x-pie-executable" ]; then
                    if [[ ${line} =~ ^/usr/bin/[^/]*$ ]]; then
                        file=$(basename ${line})
                        mv ${root}/usr/bin/${file} ${root}/usr/bin/slix-ld-${file}
                        ln -sr ${root}/usr/bin/slix-ld ${root}/usr/bin/${file}
                    fi

                # patch shell scripts
                elif [ "${t}" == "text/x-shellscript" ]; then
                    inter=$(head -n 1 ${root}/${line:1})
                    if [ "${inter}" == "#!/bin/sh" ] \
                        || [ "${inter}" == "#! /bin/sh" ] \
                        || [ "${inter}" == "#!/bin/sh -" ]; then
                        sed -i '1s#.*#\#!/usr/bin/env sh#' ${root}/${line:1}
                    elif [ "${inter}" == "#!/bin/bash" ]; then
                        sed -i '1s#.*#\#!/usr/bin/env bash#' ${root}/${line:1}
                    elif [ "${inter}" == "#!/bin/zsh" ] \
                        || [ "${inter}" == "#!/usr/local/bin/zsh" ]; then
                        sed -i '1s#.*#\#!/usr/bin/env zsh#' ${root}/${line:1}
                    else
                        echo "${root}/${line:1} needs fixing, unexpected shell interpreter: ${inter}"
                    fi
                fi
            fi
        else
            echo "what is this? ${line}"
        fi
    done
)

version=$(pacman -Qi ${pkg} \
    | grep -P "^Version" \
    | tr '\n' ' ' \
    | awk '{print $3}')

../build/bin/slix archive --input ${target} --output ${target}.gar
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
