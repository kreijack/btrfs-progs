

setup() {

	. config.in

	umount $MNTPNT
	mkfs.btrfs $FIRSTDEV || return 1
	mount $FIRSTDEV $MNTPNT || return 1
	umount $MNTPNT

	return 0
}

teardown() {
	umount $MNTPNT
}

test_corrupt_magic() {



	echo -n "01234567" |
		dd bs=1 seek=$((64*1024+64)) of=$FIRSTDEV count=8

	out="$(mount $FIRSTDEV $MNTPNT 2>&1)"
	ret=$?
	echo $out

	[ $ret -eq 32 ] || return 1
	echo $out | grep -q "ERROR: Failed check of BTRFS_MAGIC" || return 1

	return 0
}

test_corrupt_cksum() {


	echo -n "01234567" |
		dd bs=1 seek=$((64*1024)) of=$FIRSTDEV count=8

	out="$(mount $FIRSTDEV $MNTPNT 2>&1)"
	ret=$?
	echo $out

	[ $ret -eq 32 ] || return 1
	echo $out | grep -q "ERROR: Failed check of checksum" || return 1

	return 0
}
