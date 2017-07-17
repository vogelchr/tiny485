#!/bin/sh

echo "**** NOTE: You have to move this script outside of the git"     >&2
echo "**** build directory, otherwise it will be overwritten during"  >&2
echo "**** the builds."                                               >&2

git checkout master

set -e

rm_tmpfiles() {
	if [ "x$git_commit_file" != "x" ] ; then
		echo "Removing $git_commit_file."
		rm -f -- $git_commit_file
	fi
}

trap rm_tmpfiles EXIT

git_commit_file=`mktemp /tmp/git_commits_XXXXXX`
git log | awk '/^commit /{ print $2 }' | tac >$git_commit_file
N_COMMITS="`wc -l $git_commit_file`"
N=0

exec 5<$git_commit_file
while read commit_id <&5 ; do
	N=$(( $N + 1 ))
	echo "Commit $N / $N_COMMITS ($commit_id)." >&2

	git clean -x -f -d

	exitcode=0
	git checkout $commit_id || exitcode="$?"
	if [ $exitcode != 0 ] ; then
		echo "$commit_id: cannot checkout!"
		continue
	fi

	exitcode=0
	make || exitcode="$?"
	if [ $exitcode != 0 ] ; then
		echo "$commit_id: cannot build"
		continue
	fi

	if ! [ -e avr-stepper-iface.bin ] ; then
		echo "$commit_id: no built file found."
		continue
	fi

	bytes_flash=$(avr-size -C --mcu=attiny2313 avr-stepper-iface.bin | awk '/^Program:/{ print $2 }')
	echo "$N $commit_id $bytes_flash" >>../commit_size_log.txt
done

