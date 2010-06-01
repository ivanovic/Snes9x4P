#!/bin/sh
DATE=`date +%Y%m%d` # year month date
../../../../libpnd/testdata/scripts/pnd_make.sh -p snes9x4d4p_$DATE.pnd -d pnd -x pnd/PXML.xml -i pnd/icon.png
