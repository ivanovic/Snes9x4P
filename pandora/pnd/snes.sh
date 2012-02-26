#!/bin/bash

# This part of the script is meant to test for the available OS version. It does allow
# checking for the exact version number of the currently installed OS and this
# way some filtering for an available feature in an installed lib (like libSDL)
# can be checked more easily.

REQUIRED_MAJOR=1
REQUIRED_MINOR=7
REQUIRED_RELEASE=0
REQUIRED_BUILD=0

REQUIRED_VERSION_FOUND="no"

OS_MAJOR=`cat /etc/op-version | grep -e "^Version" | cut -d " " -f 2 | cut -d "." -f 1`
OS_MINOR=`cat /etc/op-version | grep -e "^Version" | cut -d " " -f 2 | cut -d "." -f 2`
OS_RELEASE=`cat /etc/op-version | grep -e "^Version" | cut -d " " -f 2 | cut -d "." -f 3`
OS_BUILD=`cat /etc/op-version | grep -e "^Version" | cut -d " " -f 2 | cut -d "." -f 4`

if [ "$REQUIRED_MAJOR" -lt "$OS_MAJOR" ]
then
    REQUIRED_VERSION_FOUND="yes"
elif [ "$REQUIRED_MAJOR" -eq "$OS_MAJOR" ] && [ $REQUIRED_MINOR -lt $OS_MINOR ]
then
    REQUIRED_VERSION_FOUND="yes"
elif [ "$REQUIRED_MAJOR" -eq "$OS_MAJOR" ] && [ $REQUIRED_MINOR -eq $OS_MINOR ] && [ $REQUIRED_RELEASE -lt $OS_RELEASE ]
then
    REQUIRED_VERSION_FOUND="yes"
elif [ "$REQUIRED_MAJOR" -eq "$OS_MAJOR" ] && [ $REQUIRED_MINOR -eq $OS_MINOR ] && [ $REQUIRED_RELEASE -eq $OS_RELEASE ] && [ $REQUIRED_BUILD -lt $OS_BUILD ]
then
    REQUIRED_VERSION_FOUND="yes"
fi

echo "found at least HF7: "$REQUIRED_VERSION_FOUND

#
# Everything below this is the normal snes9x startup script (which uses the var
# from above).
#

echo "ROM filename is $1"

PWD=`pwd`
echo "PWD pre-run $PWD"

# set HOME so SRAM saves go to the right place
HOME=.
export HOME

export SDL_VIDEODRIVER=omapdss
export SDL_OMAP_VSYNC=1

# enable higher quality audio
#0 - off, 1 - 8192, 2 - 11025, 3 - 16000, 4 - 22050, 5 - 32000 (default), 6 - 44100, 7 - 48000
#ARGS='-soundquality 7'
ARGS=''

if [ -f "args.txt" ]
then
	PICKUPARGS=`cat args.txt`
	if [ ! -z "$PICKUPARGS" ]
	then
		# http://wiki.arcadecontrols.com/wiki/Snes9x#Command_Line_Parameters
		ARGS=$PICKUPARGS
	fi
fi

# run it!
file $1 | grep -q "7-zip archive data"
if [ "$?" -eq "0" ];
then
	FILEITEM=$(eval zenity --width=800 --height=400 --list --title="Which\ ROM?" --column="Name" `./util/7za l -slt "$1" | grep ^Path | sed -e's/^Path = /"/g' -e's/$/"/' | sed '1d'`)
	if [ $? = 0 ]; then
		zenity --info --title="$1" --text="Extracting ROM from 7z file, please be patient after hitting 'OK'..."
		./util/7za e -y -o"/tmp/" "$1" "$FILEITEM"
		#the extracted name is without the internal PATH of the archive, so strip away this part!
		FILENAME=`echo $FILEITEM | sed -e "s,^.*/,,g"`
		FILENAME="/tmp/$FILENAME"
		if [ "$REQUIRED_VERSION_FOUND" = "yes" ]
		then
			./snes9x $ARGS "$FILENAME"
		else
#			LD_LIBRARY_PATH=lib/:$LD_LIBRARY_PATH ./snes9x $ARGS "$FILENAME"
			LD_PRELOAD=lib/libSDL-1.2.so.0 ./snes9x $ARGS "$FILENAME"
		fi
		rm "$FILENAME"
	fi
else
	if [ "$REQUIRED_VERSION_FOUND" = "yes" ]
	then
		./snes9x $ARGS "$1"
	else
#		LD_LIBRARY_PATH=lib/:$LD_LIBRARY_PATH ./snes9x $ARGS "$1"
		LD_PRELOAD=lib/libSDL-1.2.so.0 ./snes9x $ARGS "$1"
	fi
fi
