#!/bin/bash
#
# loop through all of our bad images and make sure fsck repairs them properly
#
# It's GPL, same as everything else in this tree.
#

here=`pwd`

. ./tests/common

rm -f $RESULTS

for i in $(find $here/tests/fsck-tests -name '*.img')
do
	echo "     [TEST]    $(basename $i)"
	echo "testing image $i" >> $RESULTS
	$here/btrfs-image -r $i test.img >> $RESULTS 2>&1 || \
		_fail "restore failed"
	$here/btrfsck test.img >> $RESULTS 2>&1
	[ $? -eq 0 ] && _fail "btrfsck should have detected corruption"

	$here/btrfsck --repair test.img >> $RESULTS 2>&1 || \
		_fail "btrfsck should have repaired the image"

	$here/btrfsck test.img >> $RESULTS 2>&1 || \
		_fail "btrfsck did not correct corruption"
done

$here/tests/test-init-extent-tree.sh
