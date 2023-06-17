#!/usr/bin/env bash

set -Eeuo pipefail

pkg=${1}
shift

deps="${1}"
shift

target=${pkg}

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

for d in $deps; do
    if [ -e pkg-$d.sh ]; then
        bash pkg-$d.sh
    fi
done
root=${target}/rootfs
if [ -e ${root} ]; then
    rm -rf "${root}"
fi
mkdir -p ${root}

# check dependencies
echo -n "" > ${target}/dependencies.txt
for d in $deps; do
    latest="$(slix index info ${SLIX_INDEX} --name ${d} | tail -n 1 || true)"
    if [ -z "${latest}" ]; then
        echo "$pkg dependency $d is missing"
        exit 1
    fi
    echo ${latest} >> ${target}/dependencies_unsorted.txt
    slix index info ${SLIX_INDEX} --name ${d} --dependencies >> ${target}/dependencies_unsorted.txt
done
cat ${target}/dependencies_unsorted.txt | sort | uniq > ${target}/dependencies.txt
rm ${target}/dependencies_unsorted.txt


export requiresSlixLD=0
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
                        export requiresSlixLD=1
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

hasLdD=0
for d in $deps; do
    if [ "$d" == "slix-ld" ]; then
        hasLdD=1
    fi
done

if [ ${hasLdD} -ne ${requiresSlixLD} ]; then
#    echo "requirement of slix-ld for ${pkg} unclear: ${hasLdD} and ${requiresSlixLD}"
fi
