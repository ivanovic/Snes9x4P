#!/bin/bash
if [ $1 ]; then cd $1; fi
 
for x in $(pwd)/*
do
if [ -x $x ] && [ ! -d $x ] && [ ! $(echo $x | cut -d . -f 2 -s) ] ; then exe=$x; break; fi
done
BASENAMEnoex=$(basename "$exe" | cut -d'.' -f1)
BASENAME=$(basename "$exe")
rnd=$RANDOM;
loc=$(dirname "$0")
 
echo '
<?xml version="1.0" encoding="UTF-8"?>
<PXML xmlns="http://openpandora.org/namespaces/PXML" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="'$BASENAME-$rnd'" xsi:noNamespaceSchemaLocation="PXML_schema.xsd">
  <title lang="en_US">'$BASENAMEnoex'</title>
  <title lang="de_DE">'$BASENAMEnoex' - German (lol!)</title>
 
  <exec command="'$BASENAME'"/>
 
  <description lang="en_US">en_US Automatically generated pxml from'$(pwd)' exe='$BASENAME'</description>
  <description lang="de_DE">de_DE Automatisch generiertes pxml aus'$(pwd)' exe='$BASENAME'</description>
 
  <previewpics>'
#add all images in the folder as preview pics
for image in $(file -i -0 * | grep -a image | cut -d" " -f1)
do
echo "<pic src="$image"/>"
done
echo '  </previewpics>
 
  <author name="'$USERNAME'" website="http://www.openpandora.org"/><!--Optional email and website, name required-->
 
  <version major="1" minor="1" release="1" build="2"/><!--This programs version-->
  <osversion major="1" minor="0" release="0" build="0"/><!--The minimum OS version required-->
 
  <categories>
    <category name="Main category"><!--category like "Games", "Graphics", "Internet" etc-->
    <subcategory name="Subcategory 1"/><!--subcategory, like "Board Games", "Strategy", "First Person Shooters"-->
    <subcategory name="Subcategory 2"/>
    </category>
    <category name="Alternative category">
      <subcategory name="Alternative Subcategory 1"/>
    </category>
  </categories>
 
  <clockspeed frequency="600"/><!--Frequency in Hz-->
</PXML>
'