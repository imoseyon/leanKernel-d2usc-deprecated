#!/system/bin/sh

export PATH="/system/xbin:/sbin:/system/bin:${PATH}"

# passthrough if we're in kexec mode
if [ "$(getprop ro.bootmode)" == "kexec" ] ; then
	exit
else
	# copy update package to /cache
	cp /system/etc/kexec-boot.zip /cache

	# tell recovery to boot it
	mkdir -p /cache/recovery
	echo 'install_zip("/cache/kexec-boot.zip");' \
		> /cache/recovery/extendedcommand

	# remount all block filesystems read-only
	for i in $(mount | grep /dev/block | awk '{print $3}') ; do
		mount -o remount,ro ${i}
	done

	# sync
	sync

	# reboot
	reboot recovery
fi
