make ARCH=arm CROSS_COMPILE=/data/linaro/android-toolchain-eabi/bin/arm-linux-androideabi- -j2
find drivers -name "*.ko" | xargs /data/linaro/android-toolchain-eabi/bin/arm-linux-androideabi-strip --strip-unneeded
find drivers -name "*.ko" | xargs -i cp {} zip/system/lib/modules/
cp arch/arm/boot/zImage kexec/kexec/kernel
cd lk.ramdisk
chmod 750 init*
chmod 644 default* uevent* MSM*
find . | cpio -o -H newc | gzip > ../kexec/kexec/ramdisk.img
cd ../kexec
zip -r kexec-boot.zip *
mv kexec-boot.zip ../zip/system/etc
cd ../zip
zip -r lk_tw_testv${1}.zip *
mv lk_tw_testv${1}.zip /tmp
/data/utils/s3_ftpupload.sh $1
