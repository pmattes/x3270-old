#! /usr/local/bin/bash

# Copyright 1995, 2000 by Paul Mattes.
#  Permission to use, copy, modify, and distribute this software and its
#  documentation for any purpose and without fee is hereby granted,
#  provided that the above copyright notice appear in all copies and that
#  both that copyright notice and this permission notice appear in
#  supporting documentation.

# VM login script, which runs as a peer of x3270.
# bash version

set -x
me=${0##*/}

# Set up login parameters
tcp_host=${1-ibmsys}
userid=${2-USERID}
password=${3-PASSWORD}

# Verbose flag for x3270if
#v="-v"

# Define some handly local functions.

# x3270 interface function
function xi
{
	X3270OUTPUT=6 X3270INPUT=5 x3270if 5>$ip 6<$op $v "$@"
}

# 'xi' function, with space-to-comma translation
function xic
{
	typeset sep cmd="$1("

	shift
	while [ $# -gt 0 ]
	do	cmd="$cmd$sep\"$1\""
		sep=","
		shift
	done
	cmd="$cmd)"
	xi "$cmd"
}

# Common x3270 Ascii function
function ascii
{
	xic Ascii $@
}

# Common x3270 String function
function string
{
	xic String "$@"
}

# x3270 connection status
function cstatus
{
	xi -s 4
}

# x3270 rows
function rows
{
	xi -s 7
}

# x3270 columns
function cols
{
	xi -s 8
}

# x3270 Snap function
function snap
{
	xic Snap $@
}

# Failure.
function die
{
	xic Info "$me error: $@"
	xic CloseScript 1
	exit 1
}

# Wait for a READ prompt.
function waitread
{
	snap
	while [ "$(snap Ascii $(expr $(snap Rows) - 1) $(expr $(snap Cols) - 17) 4)" != "READ" ]
	do	xic Wait Output
		snap
	done
}

# Set up pipes for x3270 I/O
ip=/tmp/ip.$$
op=/tmp/op.$$
rm -f $ip $op
trap "rm -f $ip $op" EXIT
trap "exit" INT QUIT HUP TERM
mkfifo $ip $op

# Start x3270
x3270 -script -model 2 <$ip >$op &
xp=$!
exec 5>$ip	# hold the pipe open
xi -s 0 >/dev/null || exit 1

# Connect to host
xic Connect $tcp_host || die "Connection failed."

# Make sure we're connected.
xic Wait InputField
[ "$(cstatus)" = N ] && die "Not connected."

# Log in.
string "$userid"
xic Tab
string "$password"
xic Enter
waitread
if [ "$(ascii 1 11 7)" = "Already" ]
then	die "Can't run -- already logged in."
	exit 1
fi

# Boot CMS, if we have to.
if [ "$(ascii $(expr $(rows) - 1) $(expr $(cols) - 20) 2)" = "CP" ]
then	xic Clear
	string "i cms"
	xic Enter
	waitread
fi
	
# Done.
xic CloseScript
exit 0
