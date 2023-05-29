#!/usr/bin/env bash


pkg=${1}
target=${1}
root=${target}/rootfs
mkdir -p ${root}


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
                    inter=$(patchelf --print-interpreter ${root}/${line} 2>/dev/null)
                    if [ "${inter}" == "/lib64/ld-linux-x86-64.so.2" ]; then
                        ld=$(realpath -m --relative-to $(dirname ${root}/${line}) ${root}/usr/lib/ld-linux-x86-64.so.2)
                        patchelf --set-interpreter ${ld} ${root}/${line}
                    else
                        echo "${root}/${line} unexpected elf interpreter, needs fixing"
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

pacman -Qi ${pkg} \
    | grep "Depends On" \
    | cut -d ':' -f 2 \
    | tr ' ' '\n' \
    | grep -v None \
    | grep . \
    | sort -u \
    | xargs -n 1 ./translateName.sh \
    | sort -u \
    > ${target}/dependencies.txt
../archive ${target}
hash=$(sha256sum -b ${target}.gar | awk '{print $1}')
destFile="${target}@${version}-$hash.gar"
if [ ! -e "${destFile}" ]; then
    if [ $(cat ${target}/dependencies.txt | grep Missing.gar | wc -l) -ge 1 ]; then
        echo "missing dependencies:"
        cat ${target}/dependencies.txt | grep Missing.gar
        destFile="${target}@${version}-defect.gar"
        rm ${target}.gar
    else
       mv ${target}.gar ${destFile}
        echo "created ${destFile}"
    fi
else
    echo "${destFile} already existed"
    rm ${target}.gar
fi

rm -rf ${target}
