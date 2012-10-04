#!/bin/bash
prefix="lk_tw_ics"
device="usc"
filename="${prefix}_${device}-v${1}.zip"

[[ `diff arch/arm/configs/lk_defconfig .config ` ]] && \
        { echo "Unmatched defconfig!"; exit -1; }
[[ $2 == "compile" ]] &&
  sed -i s/CONFIG_LOCALVERSION=\".*\"/CONFIG_LOCALVERSION=\"-leanKernel-${device}-${1}-\"/ .config &&
  make ARCH=arm CROSS_COMPILE=/data/linaro/android-toolchain-eabi/bin/arm-linux-androideabi- -j2 && cp .config arch/arm/configs/lk_defconfig
find drivers -name "*.ko" | xargs /data/linaro/android-toolchain-eabi/bin/arm-linux-androideabi-strip --strip-unneeded
find drivers -name "*.ko" | xargs -i cp {} zip/system/lib/modules/
cd lk.ramdisk
chmod 750 init*
chmod 644 default* uevent* MSM*
chmod 755 sbin
chmod 700 sbin/lkflash sbin/lkconfig
find . | cpio -o -H newc | gzip > /tmp/ramdisk.img
cd ../

/data/unpack-mkbootimg/mkbootimg --cmdline 'console=null androidboot.hardware=qcom user_debug=31' --base 0x80200000 --ramdiskaddr 0x81500000 --kernel arch/arm/boot/zImage --ramdisk /tmp/ramdisk.img -o zip/boot.img

cd zip
zip -r $filename *
mv $filename /tmp
if [[ $2 == "upload" ]]; then
  git log --pretty=format:"%aN: %s" -n 200 > /tmp/s3_commit.log
  lkflash="http://imoseyon.host4droid.com/s3/$filename `md5sum /tmp/$filename | awk '{ print \$1}'` ${1}"
  echo $lkflash > /tmp/${device}-tw
  /data/utils/s3_ftpupload.sh $1 $device
  /bin/rm -f /tmp/${device}-tw
fi  
echo
echo $lkflash
