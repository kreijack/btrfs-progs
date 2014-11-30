

setup() {

	. config.in
	. library.sh

	mkfs.btrfs -mraid1 -draid1 $FIRSTDEV $REMOVABLEDISK || return 1
	mount $FIRSTDEV $MNTPNT || return 1
	touch $MNTPNT/empty
	umount $MNTPNT

	remove_disk $REMOVABLEDISK
	mount -o degraded,device_timeout=1 $FIRSTDEV $MNTPNT || return 1
	touch $MNTPNT/empty2
	umount $MNTPNT

	scan_disks

	return 0
}

teardown() {
	# place here your cleanup
	umount $MNTPNT
}

test_generation_mismatch() {

	tmpfile=$(mktemp)
	mount -v $FIRSTDEV $MNTPNT >$tmpfile 2>&1
	cat $tmpfile
	grep -q "WARNING: generation numbers mismatch" $tmpfile
	ret=$?
	rm -f $tmpfile

	[ $ret -eq 0 ] ||  return 1
	return 0
}

