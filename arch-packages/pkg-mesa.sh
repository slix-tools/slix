name=mesa
deps="libdrm libelf libunwind libxdamage libxshmfence libxxf86vm llvm-libs lm_sensors vulkan-icd-loader wayland zstd libomxil-bellagio"
for d in $deps; do
    bash pkg-$d.sh
done
./createPackage.sh ${name} slix-ld ${deps}
