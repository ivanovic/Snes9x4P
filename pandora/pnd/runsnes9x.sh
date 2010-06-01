#!/bin/sh

FILENAME=`zenity --file-selection --title="Select a SNES ROM"`

./snes9x "$FILENAME"
