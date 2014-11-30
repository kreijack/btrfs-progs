

setup() {

	. config.in
	. library.sh

	lv_vg_pv_clean_up
	pvcreate -y -ff $THIRDDEV || return 1
	vgcreate $VGNAME $THIRDDEV || return 1
	lvcreate -n $LVNAME -L $LVSIZE $VGNAME || return 1

	mkfs.btrfs $FIRSTDEV /dev/$VGNAME/$LVNAME || return 1
	lvcreate -s -n ${LVNAME}_snap -L $LVSIZE $VGNAME/$LVNAME
}

teardown() {
	# place here your cleanup
	lv_vg_pv_clean_up
	umount $MNTPNT
}

test_snapshot_fail() {

	out="$(mount $FIRSTDEV $MNTPNT  2>&1)"
	ret=$?
	[ $ret -eq 32 ] || return 1
	echo $out | grep -q "have the same disk uuid" || return 2

	return 0
}

test_snapshot_ok() {


	out="$(mount $FIRSTDEV -o device=/dev/$VGNAME/$LVNAME $MNTPNT 2>&1)"
	ret=$?
	[ $ret -eq 0 ] || return 1
	echo $out | grep -q "have the same disk uuid" && return 2

	return 0
}


