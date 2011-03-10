#!/bin/bash

help(){
cat >&2 <<ENDHELP
$0 [args] :
-s|--src <src dir>      : Define source directory (default: $SRCDIR)
-n|--name <name>        : Define the pnd base name (default: $PND_NAME)
-d|--dest <dest dir>    : Define destination directory (default: $DESTDIR)
-a|--author <name>      : programmers names (default: $AUTHOR)
-v|--version <version>  : Define the version (default: $VERSION)
-w|--website <url>      : Define the url (default: $WEBSITE)
-b|--build <build id>   : Define the build number (default: $BUILD)
-f|--force              : overide PXML.xml file if found
-h|--help               : show this screen
ENDHELP
}

DEBUG(){
	echo $*>&2
}

buildApplicationList(){
	#output "<appname> [<desktopfile>]" lines
	cd $DESTDIR
	DESTLST=$(find $SRCDIR -name "*desktop";find $DESTDIR -name "*desktop")
	if [ ! -z "$DESTLST" ];then
		{
		for d in $DESTLST;do
			EXE=$(awk -F= '/Exec/{print $2}'<$d|awk '{print $1}'|head -1)
			if [[ "x$EXE" != "x" ]];then
				BIN=$(find . -executable -type f -name $(basename $EXE)|head -1)
				echo ${BIN:-$EXE} $d
			fi
		done
		}|sort|awk 'BEGIN{P=""}$1!=P{print}{P=$1}'
	else
		find . -executable -type f \! -name "*sh"
	fi
}

getPATH(){
	cd $DESTDIR
	L=$(find . -type d -name bin)
	echo $L|sed "s#\./#$(pwd)/#g;s# #:#g"
}
getLIBPATH(){
	cd $DESTDIR
	L=$(find . -type d -name lib)
	echo $L|sed "s#\./#$(pwd)/#g;s# #:#g"
}

genLaunchScript() {
	S="$DESTDIR/scripts/$(basename $1).sh"
	if [ -e $S ];then
		mv $S ${S}.old
	fi
	cat>$S<<ENDSCRIPT
#!/bin/sh
export PATH="$(getPATH):\${PATH:-"/usr/bin:/bin:/usr/local/bin"}"
export LD_LIBRARY_PATH="$(getLIBPATH):\${LD_LIBRARY_PATH:-"/usr/lib:/lib"}"
export HOME="/mnt/utmp/$PND_NAME" XDG_CONFIG_HOME="/mnt/utmp/$PND_NAME"

if [ -d /mnt/utmp/$PND_NAME/share ];then
	export XDG_DATA_DIRS=/mnt/utmp/$PND_NAME/share:\$XDG_DATA_DIRS:/usr/share
fi
export SDL_AUDIODRIVER="alsa"
cd \$HOME
[ -e "\$HOME/scripts/pre_script.sh" ] && . \$HOME/scripts/pre_script.sh
if [ -e "\$HOME/scripts/post_script.sh" ];then
	$1 \$*
	. \$HOME/scripts/post_script.sh
else
	exec $1 \$*
fi
ENDSCRIPT
	chmod 755 $S
}

desktop2application(){ #generate PXML application info out of a desktop file passed as parameter
	DESK=$1
	if [ ! -z "$DESK" ];then
		DVERSION=$(awk -F= '$1=="Version"{print $2}'<$1)
		if [ -z "$DVERSION" ] || [[ "$DVERSION" = "1.0" ]];then
			DVERSION=$VERSION
		fi
	else
		DVERSION=$VERSION
	fi
	MAJOR=$(echo $DVERSION|awk -F. '{print $1}')
	MINOR=$(echo $DVERSION|awk -F. '{print $2}')
	REL=$(echo $DVERSION|awk -F. '{print $3}')
	MINOR=${MINOR:-"0"}
	REL=${REL:-"0"}

	# Exec --------------------------
	echo "    <exec command=\"scripts/${BASEB}.sh\"/>"

	# title -------------------------
	if [ ! -z "$DESK" ];then
		#awk -F= '($1~/^Name/)||$1=="Name"{print $1" "$2}'<$1|while read DN DNV;do
		awk -F= '($1~/^Name/&&/en_US/)||$1=="Name"{print $1" "$2}'<$1|while read DN DNV;do
			l=$(echo $DN|sed 's/Name//;s/\[//;s/\]//')
			echo "    <title lang=\"${l:-"en_US"}\">$DNV</title>"
		done
	else
		echo "    <title lang=\"en_US\">$PND_NAME</title>"
	fi
	echo

	# Author ------------------------
	echo "    <author name=\"$AUTHOR\" website=\"$WEBSITE\"/>"

	# Version -----------------------
	echo "    <version major=\"$MAJOR\" minor=\"$MINOR\" release=\"$REL\" build=\"$BUILD\"/>	      <!--This programs version-->"

	# OS Version --------------------
	echo "    <osversion major=\"1\" minor=\"0\" release=\"0\" build=\"0\"/>		<!--The minimum OS version required-->"
	echo

	# Description -------------------
	if [ ! -z "$DESK" ];then
		#awk -F= '($1~/^Comment/)||$1=="Comment"{print $1" "$2}'<$1|while read DN DNV;do
		awk -F= '($1~/^Comment/&&/en_US/)||$1=="Comment"{print $1" "$2}'<$1|while read DN DNV;do
			l=$(echo $DN|sed 's/Comment//;s/\[//;s/\]//')
			echo "    <description lang=\"${l:-"en_US"}\">$DNV</description>"
		done
	else
		echo "    <description lang=\"en_US\">Automatically generated description from $(pwd) for PND=$PND_NAME</description>"
	fi
	echo

	# Icon --------------------------
	ICON=""
	if [ ! -z "$DESK" ];then
		ICON=$(awk -F= '$1~/^Icon/{print $2}'<$1)
	fi
	if [ ! -z "$ICON" ];then
		F=$(find $DESTDIR -name ${ICON}.png|head -1)
		if [ -z "$F" ];then
			F=$DESTDIR/icon.png
		fi
		echo "    <icon src="'"'$F'"/>'
		if [ ! -e "$DESTDIR/icon.png" ] && [ ! -z "$(find $DESTDIR -name ${ICON}.png|head -1)" ];then
			cp $(find $DESTDIR -name ${ICON}.png|head -1) $DESTDIR/icon.png
		fi
	elif [ -e "$DESTDIR/icon.png" ];then
		echo "    <icon src=\"$DESTDIR/icon.png\"/>"
	else
		echo "    <!--<icon src=\"path/to/icon.pnd\"/>-->"
	fi
	echo

	# Preview pics ------------------
	if [ ! -z "$(find $DESTDIR/previews -name "$BASEB*")" ];then
		echo "    <previewpics>"
		for i in $(find $DESTDIR/previews -name "$BASEB*");do
			echo "      <pic src=\"previews/$(basename $i)\"/>"
		done
		echo "    </previewpics>"
	else
		cat <<ENDASSO
<!--
    <previewpics>
      <pic src="previews/${BASEB}.png"/>
    </previewpics>
-->
ENDASSO
	fi
	echo

	# Documentation -----------------
	HTML=$(find $DESTDIR -type d -name index.html|head -1)
	DOC=$DESTDIR/readme.txt
	if [ ! -z "$HTML" ];then
		for i in $(find $DESTDIR -type d -name index.html);do
			echo "    <info name=\"${BASEB} documentation\" type=\"text/html\" src=\"$i\"/>"
		done
	elif [ -e "$DOC" ];then
		echo "    <info name=\"${BASEB} documentation\" type=\"text/plain\" src=\"$DOC\"/>"
	else
		echo "    <!--<info name=\"${BASEB} documentation\" type=\"text/plain\" src=\"$DOC\"/>-->"
	fi

	# Categories --------------------
	cat <<ENDCATEGORIES

    <categories>
      <!-- http://standards.freedesktop.org/menu-spec/latest/apa.html -->
ENDCATEGORIES

	if [ ! -z "$DESK" ];then
		DCAT=$(awk -F= '$1=="Categories"{print $2}'<$1)
	else
		DCAT=""
	fi
	CATCNT=$(($(echo $DCAT|sed "s/;/ /g"|wc -w) / 2))
	if [ $CATCNT -gt 0 ];then
		for i in $(seq 1 $CATCNT);do
			DCATMAJ=$(echo $DCAT|awk -F\; "{print \$$(($i*2-1))}")
			DCATMIN=$(echo $DCAT|awk -F\; "{print \$$(($i*2))}")
			cat <<ENDCATEGORIES
      <category name="$DCATMAJ">
	<subcategory name="$DCATMIN"/>
      </category>
ENDCATEGORIES
		done
	else
		cat <<ENDCATEGORIES
      <category name="Game">
	<subcategory name="ArcadeGame"/>
      </category>
ENDCATEGORIES
	fi
	cat <<ENDCATEGORIES
    </categories>

ENDCATEGORIES

	# Associations ------------------
	cat <<ENDASSO
<!--
    <associations>
      <association name="Deinterlaced Bitmap Image" filetype="image/bmp" exec="-f %s"/>
      <association name="Style sheet system crasher" filetype="text/css" exec="-f %s"/>
    </associations>
-->
ENDASSO

	# ClockSpeed --------------------
	echo "    <!--<clockspeed frequency=\"600\"/>-->"
}

genPxml(){
	# output the PXML.xml file
	if [ -e $DESTDIR/PXML.xml ];then
		mv $DESTDIR/PXML.xml $DESTDIR/PXML.xml.old
	fi
	cat >$DESTDIR/PXML.xml <<ENDHEAD
<?xml version="1.0" encoding="UTF-8"?>
<PXML xmlns="http://openpandora.org/namespaces/PXML">
<!-- please see http://pandorawiki.org/PXML_specification for more information before editing, and remember the order does matter -->

ENDHEAD
	if [ ! -d $DESTDIR/previews ];then
		mkdir -p $DESTDIR/previews
	fi
	if [ ! -d $DESTDIR/scripts ];then
		mkdir $DESTDIR/scripts
	fi
	buildApplicationList|while read BIN DESK;do
		BASEB=$(basename $BIN)
		genLaunchScript $BIN
		cat >>$DESTDIR/PXML.xml <<ENDAPP
  <application id="$PND_NAME-$(basename $BIN)-$RND" appdata="$PND_NAME">
ENDAPP
		desktop2application $DESK >>$DESTDIR/PXML.xml
		cat >>$DESTDIR/PXML.xml <<ENDINFO
  </application>

ENDINFO
	done
	echo "</PXML>" >>$DESTDIR/PXML.xml
}


#####################
### Script main :
##

FORCE=0
BUILD=1
AUTHOR=sebt3
WEBSITE=${WEBSITE:-"http://www..openpandora.org"}
SRCDIR=${SRCDIR:-$(pwd)}
PND_NAME=$PRJ
PND_NAME=${PND_NAME:-$(basename $SRCDIR|awk -F- '{print $1}')}
VERSION=${VERSION:-$(basename $SRCDIR|awk -F- '{print $2}')}
DESTDIR=${DESTDIR:-"/mnt/utmp/$PND_NAME"}
RND=$RANDOM
# Parse arguments
while [ $# -gt 0 ];do
	case $1 in
	-s|--src)       SRCDIR=$2;shift;;
	-d|--dest)      DESTDIR=$2;shift;;
	-b|--build)     BUILD=$2;shift;;
	-a|--author)    AUTHOR=$2;shift;;
	-n|--name)      PND_NAME=$2;shift;;
	-v|--version)   VERSION=$2;shift;;
	-w|--website)   WEBSITE=$2;shift;;
	-f|--force)     FORCE=1;;
	-h|--help)      help;exit 1;;
	*)	      echo "'$1' unknown argument">&2;help;exit 2;;
	esac
	shift;
done

# Validate arguments
if [ ! -d $SRCDIR ];then
	echo "$SRCDIR don't exist" >&2
	help
	exit 3
fi
if [ ! -d $DESTDIR ] && [ $FORCE -eq 0 ];then
	echo "$DESTDIR don't exist" >&2
	help
	exit 4
elif [ ! -d $DESTDIR ];then
	mkdir -p $DESTDIR
	if [ $? -ne 0 ];then
		echo "$DESTDIR don\'t exist and cannot be created" >&2
		help
		exit 5
	fi
fi
if [ $(buildApplicationList|wc -l) -le 0 ];then
	echo "No applications found">&2
	help
	exit 6
fi
if [ -e $DESTDIR/PXML.xml ] && [ $FORCE -eq 0 ];then
	echo "PXML file exist and force disabled." >&2
	help 
	exit 7
fi
genPxml

