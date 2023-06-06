name=tmux
deps="ncurses libevent libutempter systemd-libs"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
