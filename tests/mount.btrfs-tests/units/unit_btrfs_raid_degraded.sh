

setup() {

	. config.in
	. library.sh

	scan_disks

	mkfs.btrfs -mraid1 -draid1 $FIRSTDEV $REMOVABLEDISK || return 1
	mount $FIRSTDEV $MNTPNT
	touch $MNTPNT/empty
	umount $MNTPNT

	return 0
}

teardown() {
	umount $MNTPNT
	scan_disks
}


test_raid1_degraded() {


	remove_disk $REMOVABLEDISK

	time0=$(date +%s)
	out="$(mount -o degraded,device_timeout=4 $FIRSTDEV $MNTPNT 2>&1)"
	ret=$?
	time1=$(date +%s)
	echo -----------------
	echo $out
	echo -----------------

	[ $ret -eq 0 ] ||  return 1
	[ $(( $time1 - $time0 )) -gt 3 ] || return 1
	[ $(( $time1 - $time0 )) -lt 6 ] || return 1
	[ -e $MNTPNT/empty ] || return 1
	echo $out | grep -q "WARNING: mount in degraded mode" || return 1

	return 0
}

test_raid1_degraded_no_missing() {

	time0=$(date +%s)
	out="$(mount -o degraded,device_timeout=8 $FIRSTDEV $MNTPNT 2>&1)"
	ret=$?
	time1=$(date +%s)
	echo -----------------
	echo $out
	echo -----------------

	[ $ret -eq 0 ] ||  return 1
	[ $(( $time1 - $time0 )) -lt 3 ] || return 1
	[ -e $MNTPNT/empty ] || return 1
	echo $out | grep -q  "WARNING: mount in degraded mode" && return 1

	return 0
}


