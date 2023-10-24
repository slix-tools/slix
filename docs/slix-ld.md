<!--
    SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
    SPDX-License-Identifier: CC-BY-4.0
-->
# Slix-ld

## The Issue
Every dynamic executable has a hard coded link to an absolute path like `/usr/lib/ld-linux-x86-64.so.2`. It is not possible to link to relative paths inside of ELF-files.
For slix we need to make sure that we call our own `ld-linux-x86-64.so.2`, to keep compatibility.

## Our Solution
We move executables to a different location. E.g.: the program `/usr/bin/ls` is moved to `/usr/bin/.slix-ld-ls`, instead
we place at `/usr/bin/ls` a symlink to `/usr/bin/slix-ld` which will detect it is called via a symlink and call
`usr/bin/.slix-ld-ls` with the help of our own `ld-linux-x86-64.so.2`.

## Scenarios
- Call via absolute path `${ROOT}/usr/bin/ls`
- Call via relative path `./bin/ls`
- Call via PATH variable `ls`
- Call to a executable not located in bin `${ROOT}/usr/lib/libraryX/14.4/ls`
- Call to `slix-ld ls /usr/bin/ls` "$@"
- Call to `slix-ld --root` should print the current system root
