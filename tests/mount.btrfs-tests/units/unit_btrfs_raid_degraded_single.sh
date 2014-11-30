

setup() {

	. config.in

	mkfs.btrfs $FIRSTDEV  || return 1
	return 0
}

teardown() {
	umount $MNTPNT
}

test_single_disk_with_degraded() {

	mount -o degraded $FIRSTDEV $MNTPNT
	ret=$?

	[ $ret -eq 0 ] || return 1
	return 0
}



