

setup() {

	. config.in

	return 0
}

teardown() {
	# place here your cleanup
	umount $MNTPNT
}

test_single_disk() {
	mkfs.btrfs $FIRSTDEV || return 1
	mount $FIRSTDEV $MNTPNT || return 1
	return 0
}

test_double_disk() {
	mkfs.btrfs $FIRSTDEV $SECONDDEV || return 1
	mount $FIRSTDEV $MNTPNT || return 1
	return 0
}

test_double_disk_explicit() {
	mkfs.btrfs $FIRSTDEV $SECONDDEV || return 1
	mount $FIRSTDEV -o device=$SECONDDEV $MNTPNT
	return 0
}
