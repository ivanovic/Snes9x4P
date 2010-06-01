#!/bin/bash

PWD=`pwd`
echo "PWD at top $PWD"

# pick up last path
DIRPATH=`cat last-rom-path.txt`
if [ $DIRPATH ]
then
    echo "Found stored dirpath $DIRPATH"
    pushd $DIRPATH
else
    echo "No stored dirpath, using /media"
    pushd /media
fi

# pick up ROM filename
FILENAME=`zenity --file-selection --title="Select a SNES ROM"`

echo "ROM filename is $FILENAME"
popd

echo `dirname "$FILENAME"` > last-rom-path.txt

PWD=`pwd`
echo "PWD pre-run $PWD"

# set HOME so SRAM saves go to the right place
HOME=.
export HOME

# enable higher quality audio
ARGS='-sq 2'
#ARGS=''

# run it!
if [ $(echo "$FILENAME" | cut -d'.' -f2) = "7z" ] 
then
	FILEITEM=$(eval zenity --width=800 --height=400 --list --title="Which\ ROM?" --column="Name" `./util/7za l -slt "$FILENAME" | grep ^Path | sed -e's/^Path = /"/g' -e's/$/"/' | sed '1d'`)
	if [ $? = 0 ]; then
 		zenity --info --title="$FILENAME" --text="Extracting ROM from 7z file, please wait..."
		./util/7za e -y -o"/tmp/" "$FILENAME" "$FILEITEM"
		FILENAME="/tmp/$FILEITEM"
		./snes9x $ARGS "$FILENAME"
		rm "$FILENAME"
	fi
else
	./snes9x $ARGS "$FILENAME"
fi


