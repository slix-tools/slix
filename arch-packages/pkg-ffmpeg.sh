name=ffmpeg
deps="alsa-lib aom bzip2 fontconfig fribidi gmp gnutls gsm jack2 lame libass libavc1394 libbluray libbs2b dav1d libdrm freetype2 libglvnd libiec61883 libjxl libmfx libmodplug libopenmpt libpulse rav1e libraw1394 librsvg libsoxr libssh libtheora libva libvdpau vid.stab libvorbis libvpx libwebp libx11 x264 x265 libxcb libxext libxml2 libxv xvidcore zimg ocl-icd opencore-amr openjpeg2 opus sdl2 speex srt svt-av1 v4l-utils vmaf vulkan-icd-loader xz zlib"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
