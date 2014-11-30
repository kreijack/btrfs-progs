

setup() {

	. config.in
	. library.sh

	umount $MNTPNT
	mkfs.btrfs $FIRSTDEV || return 1

	return 0
}

teardown() {
	umount $MNTPNT
}

test_option_v() {


	out="$(mount -v $FIRSTDEV $MNTPNT 2>&1)"
	ret=$?

	[ $ret -eq 0 ] || return 1
	echo $out | grep -q "INFO: scan the first device" || return 1

	return 0
}

test_option_f() {


	out="$(mount -f $FIRSTDEV $MNTPNT 2>&1)"
	ret=$?

	[ $ret -eq 0 ] || return 1
	mount | grep $MNTPNT && return 1

	return 0
}


test_no_option() {

	out="$(mount $FIRSTDEV $MNTPNT 2>&1)"
	ret=$?

	[ $ret -eq 0 ] || return 1
	echo $out | grep -q "INFO: scan the first device" && return 1

	return 0
}


