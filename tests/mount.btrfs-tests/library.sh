lv_vg_pv_clean_up() {
	umount $MNTPNT
	lvremove -f  $VGNAME/$LVNAME
	vgremove -f  $VGNAME
	pvremove -ff -y $THIRDDEV
}

remove_disk() {

	disk=$1
	id=$(lsscsi | grep $1 | sed -e 's/\[//' -e 's/\].*//')
	host=$(echo $id | sed -e 's/:.*$//')
	devid=$(echo $id | sed -e 's/^[^:]*://')
	prefix=$(echo $id | sed -e 's/:[^:]*$//')


	#echo $host $devid

	path="/sys/class/scsi_host/host${host}/device/target${prefix}/${id}/delete"
	echo 1 >"$path"

	echo $host $prefix $devid

}

scan_disks() {
	for i in /sys/class/scsi_host/host*/scan ; do
		echo "- - -" >"$i"
	done
}
