#! /usr/local/bin/ksh
# TSO login script

tcp_host=ibm3172
dial_user=VTAM
sna_host=MOON
userid=JFR
password=jfr

typeset xs	# status
typeset xp=""
me=${0##*/}

# x3270 interface function
function xi
{
	typeset ln pln
	typeset any=""

	# Send command
	print -p -R "$@" || return 1

	# Get output
	while :
	do	read -p -r ln || return 1
		if [ "X$ln" = "Xerror" ]
		then	set -A xs $pln
			return 1
		elif [ "X$ln" = "Xok" ]
		then	set -A xs $pln
			return 0
		else	[[ "$ln" = data:* ]] && ln="${ln#data: }"
			[ -n "$any" ] && print -R "$pln"
			pln="$ln"
			any=1
		fi
	done
}

# Common x3270 Ascii function
function ascii
{
	xi 'Ascii('$1')'
}

# Common x3270 String function
function string
{
	xi 'String("'"$@"'")'
}

# Failure.
function die
{
	if [ "$xp" ] && kill -0 $xp
	then	xi "Info(\"$me error: $@\")"
		xi CloseScript
	else	print -u2 "$me: $@"
	fi
	exit 1
}

# Start x3270 and sync up
XFILESEARCHPATH=X3270.ad ./x3270 -script -model 2 2>&1 |&
xp=$!
xi || exit 1

#set -x

# Connect to host
xi "Connect($tcp_host)" || die "Connection failed."

# Wait for a proper data-entry screen to appear
xi Wait || exit 1

# Get to a VM command screen
xi Enter

# Wait for VM's prompt
while [ "$(ascii 1,0,5)" != "Enter" ]
do	sleep 2
done

# Dial out to VTAM
string "DIAL $dial_user"
xi Enter

# No proper way I can think of to do this: let the DIAL command digest
sleep 2

# "DIALED TO xxx" may momentarily flash
integer sl=10+"${#dial_user}"
while [ "$(ascii 8,0,$sl)" = "DIALED TO $dial_user" ]
do	sleep 2
done

# Make sure we are dialed to VTAM
[ "$(ascii 0,21,5)" = "ENTER" ] || die "Couldn't get to VTAM"

# Get to the SNA host
string "$sna_host $userid"
xi Enter

# TSO login takes two screens.  Make sure the first is:
#  "LOGON/LOGOFF in progress"
[ "$(ascii 0,19,24)" = "LOGON/LOGOFF in progress" ] ||
    die "Couldn't get to SNA host '$sna_host'"

# Wait until that screen disappears
while [ "$(ascii 0,19,24)" = "LOGON/LOGOFF in progress" ]
do	sleep 2
done

# Make sure the next one is "TSO/E LOGON"
[ "$(ascii 0,33,11)" = "TSO/E LOGON" ] ||
    die "Couldn't get to TSO logon screen"

# Pump in the password
string "$password"
xi Enter

# Now look for "LOGON IN PROGRESS"
[ "$(ascii 0,15,17)" = "LOGON IN PROGRESS" ] || die "Couldn't log on"

# Make sure TSO is waiting for a '***' enter
xi	# empty command to get status information
[ ${xs[9]} = 5 ] || die "Don't understand logon screen"

# Off to ISPF
xi Enter

# We're in; exit the script and let the user interact.
xi CloseScript
exit 0
