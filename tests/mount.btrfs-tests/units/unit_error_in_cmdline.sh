

setup() {

	. config.in

	return 0
}

teardown() {
	# place here your cleanup
	umount $MNTPNT
}

test_device_timeout_error() {
	mkfs.btrfs $FIRSTDEV || return 1

	mount -o device_timeout=xxxxx $FIRSTDEV $MNTPNT
	ret=$?
	[ $ret -eq 1 ] || return 1
	return 0
}

