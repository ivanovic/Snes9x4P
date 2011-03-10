#!/bin/bash

cp --no-clobber -r defconf/* ./

#old versions of snes9x came with a picklelauncher with different options
#if such an option is found, the config file has to be replaced!
if [ ! -z "$(grep "fontpath" ./config.txt)" ];
then
	cp defconf/config.txt ./config.txt
	cp defconf/profile.txt ./profile.txt
fi

./picklelauncher
