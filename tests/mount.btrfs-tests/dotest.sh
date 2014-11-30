#!/bin/bash

VERBOSE=""
NOCACN=""
FILTER_UNIT=""
FILTER_FUNC=""
LOGFILE="$( mktemp )"

while [ -n "$1" ]; do

	case "$1" in
		"-u")
			shift
			FILTER_UNIT="$1"
			;;
		"-f")
			shift
			FILTER_FUNC="$1"
			;;
		"-v")
			VERBOSE=1
			;;
		"-c")
			NOCANC=1
			;;
		"-h"|"--help")
			cat <<EOF
usage: dotest.sh [-f <filter-function>] [-u <filter-unit>] [-v]|[--help]|[-h]
EOF
			;;
		*)
			echo "Unknown option '$1'"
			exit 100
			;;
	esac

	shift

done

nrtests=0
nrfails=0

for unit in units/unit_*.sh; do

	if [ -n "$FILTER_UNIT" ]; then
		echo "$unit" | grep -q "$FILTER_UNIT" ||
			continue
	fi

	fs_test="$( 	. "$unit"
			declare -F | cut -d ' ' -f 3 | grep test_ | sort )"
	f_setup="$( 	. "$unit"
			declare -F | cut -d ' ' -f 3 | grep setup | head -1 )"
	f_teardown="$(	. "$unit"
			declare -F | cut -d ' ' -f 3 | grep teardown | head -1)"


	for f in $fs_test; do

		if [ -n "$FILTER_FUNC" ]; then
			echo "$f" | grep -q "$FILTER_FUNC" ||
				continue
		fi

		echo -n "$unit/$f..."
		(
			bash -x -c "
			. '$unit'
			[ -z '$f_setup' ] || $f_setup || exit 1
			echo '-------------------------------'
			$f
			ret=\$?
			echo '-------------------------------'
			[ -z '$f_teardown' ] || $f_teardown

			[ \$ret -eq 0 ] && exit 0
			exit 2
			"
		) >"$LOGFILE" 2>&1

		ret=$?

		nrtests=$(($nrtests + 1))
		if [ $ret -eq 0 ]; then
			echo OK
		else
			echo FAIL:
			nrfails=$(($nrfails +1 ))
		fi

		if [ $ret -ne 0 -o -n "$VERBOSE" ]; then
			echo --------------------------------
			cat $LOGFILE
			echo --------------------------------
			echo
		fi

		cat $LOGFILE >>$LOGFILE.all

	done

done

rm -f $LOGFILE
if [ -z "$NOCANC" ]; then
	rm -f $LOGFILE.all
else
	echo "The logs are in '$LOGFILE.all'"
fi
echo "$nrtests tests performed; $nrfails failed"
