#! /usr/local/bin/ksh
#set -x

typeset xs	# status
typeset xp=""
me=${0##*/}

# x3270 interface function
function xi
{
	typeset ln pln
	typeset any=""
	#set -x

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
x3270 -script -color -efont 3270-12 -once localhost 2>&1 |&
xp=$!
xi || exit 1

# Wait for login
xi Wait

# Send username/password
xi "Expect(login:)"
string x3270
xi Enter
xi "Expect(ssword:)"
string rwpl8

# Wait for shell prompt
xi Enter
xi "Expect(\".\033[0m \")" || exit 1

# Send a command
string "vd; cd 3.0.3"
xi Enter

# Exit the script and let the user interact.
xi CloseScript
exit 0
