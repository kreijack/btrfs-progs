#!/bin/bash
#
# Test the --init-extent-tree option of btrfsck in various horrible ways
#

here=`pwd`
tests=$here/tests

. ./tests/common

# First with a plain fs
echo "     [TEST]    --init-extent-tree base"
echo "doing base --init-extent-tree test" >> $RESULTS
$here/btrfs-image -r $tests/base-fs.img test.img >> $RESULTS 2>&1 || \
	_fail "restore failed"
$here/btrfsck --init-extent-tree test.img >> $RESULTS 2>&1 || \
	_fail "init extent tree failed"
$here/btrfsck test.img >> $RESULTS 2>&1 || _fail "fs inconsistent after init"

# Now corrupt the extent tree root
echo "     [TEST]    --init-extent-tree corrupt extent root"
echo "corrupt extent root --init-extent-tree test" >> $RESULTS
$here/btrfs-image -r $tests/base-fs.img test.img >> $RESULTS 2>&1 || \
	_fail "restore failed"
$here/btrfs-corrupt-block -l 7135232 test.img >> $RESULTS 2>&1 || \
	_fail "corrupt failed"
$here/btrfsck --init-extent-tree test.img >> $RESULTS  2>&1 || \
	_fail "init extent tree failed"
$here/btrfsck test.img >> $RESULTS 2>&1 || _fail "fs inconsistent after init"
