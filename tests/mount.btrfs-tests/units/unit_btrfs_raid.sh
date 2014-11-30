

setup() {

	. config.in
	. library.sh

	lv_vg_pv_clean_up
	scan_disks
	mkfs.btrfs $FIRSTDEV $REMOVABLEDISK || return 1
	remove_disk $REMOVABLEDISK

	return 0
}

teardown() {
	umount $MNTPNY
	lv_vg_pv_clean_up
	scan_disks
}

test_raid_timeout() {

	time0=$(date +%s)
	mount -o device_timeout=4 $FIRSTDEV $MNTPNT
	ret=$?
	time1=$(date +%s)

	[ $ret -eq 32 ] ||  return 1
	[ $(( $time1 - $time0 )) -gt 3 ] || return 1
	[ $(( $time1 - $time0 )) -lt 6 ] || return 1

	return 0
}


test_raid_wait_devices() {

	# blkid seems to help the device discovery process
	( sleep 4s
	  scan_disks ) &

	time0=$(date +%s)
	mount -o device_timeout=8 $FIRSTDEV $MNTPNT
	ret=$?
	time1=$(date +%s)

	mount | grep $MNTPNT || return 1
	[ $ret -eq 0 ] ||  return 1
	[ $(( $time1 - $time0 )) -gt 3 ] || return 1
	[ $(( $time1 - $time0 )) -lt 7 ] || return 1

	return 0
}



