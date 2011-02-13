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
<PXML xmlns="http://openpandora.org/namespaces/PXML" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="PXML_schema.xsd">

  <application id="'$BASENAME-$rnd'">

    <title lang="en_US">'$BASENAMEnoex'</title>
    <title lang="de_DE">'$BASENAMEnoex' - German (lol!)</title>
 
    <exec command="'$BASENAME'"/>
 
    <!-- if you want to provide a manual, uncomment the following line and set correct values! -->
    <!-- <info name="'$BASENAMEnoex' manual" type="html" src="manual.html"/> -->
 
    <description lang="en_US">en_US Automatically generated pxml from'$(pwd)' exe='$BASENAME'</description>
    <description lang="de_DE">de_DE Automatisch generiertes pxml aus'$(pwd)' exe='$BASENAME'</description>
 
    <previewpics>'
#add all images in the folder as preview pics
for image in $(file -i -0 * | grep -a image | cut -d" " -f1)
do
echo "      <pic src="$image"/>"
done
echo '    </previewpics>
 
    <author name="'$USERNAME'" website="http://www.openpandora.org"/><!--Optional email and website, name required-->
 
    <version major="1" minor="1" release="1" build="2"/><!--This programs version-->
    <osversion major="1" minor="0" release="0" build="0"/><!--The minimum OS version required-->
 
    <categories>
    <!-- for information about valid categories and explainations of them, please have a look at this website:
         http://standards.freedesktop.org/menu-spec/menu-spec-latest.html#category-registry
         entries with invalid category/subcategory combinations will appear under "others" in the menus! -->
      <category name="Main category">
      <!--valid values for "cateory Name": "AudioVideo", "Audio", "Video", "Development", "Education", "Game", "Graphics", "Network", "Office", "Settings", "System", "Utility"-->
        <subcategory name="Subcategory 1"/>
        <!--valid Values for "subcategory name": "Building", "Debugger", "IDE", "GUIDesigner", "Profiling", "RevisionControl", "Translation", "Calendar", "ContactManagement", "Database", "Dictionary", "Chart", "Email", "Finance", "FlowChart", "PDA", "ProjectManagement", "Presentation", "Spreadsheet", "WordProcessor", "2DGraphics", "VectorGraphics", "RasterGraphics", "3DGraphics", "Scanning", "OCR", "Photography", "Publishing", "Viewer", "TextTools", "DesktopSettings", "HardwareSettings", "Printing", "PackageManager", "Dialup", "InstantMessaging", "Chat", "IRCClient", "FileTransfer", "HamRadio", "News", "P2P", "RemoteAccess", "Telephony", "TelephonyTools", "VideoConference", "WebBrowser", "WebDevelopment", "Midi", "Mixer", "Sequencer", "Tuner", "TV", "AudioVideoEditing", "Player", "Recorder", "DiscBurning", "ActionGame", "AdventureGame", "ArcadeGame", "BoardGame", "BlocksGame", "CardGame", "KidsGame", "LogicGame", "RolePlaying", "Simulation", "SportsGame", "StrategyGame", "Art", "Construction", "Music", "Languages", "Science", "ArtificialIntelligence", "Astronomy", "Biology", "Chemistry", "ComputerScience", "DataVisualization", "Economy", "Electricity", "Geography", "Geology", "Geoscience", "History", "ImageProcessing", "Literature", "Math", "NumericalAnalysis", "MedicalSoftware", "Physics", "Robotics", "Sports", "ParallelComputing", "Amusement", "Archiving", "Compression", "Electronics", "Emulator", "Engineering", "FileTools", "FileManager", "TerminalEmulator", "Filesystem", "Monitor", "Security", "Accessibility", "Calculator", "Clock", "TextEditor", "Documentation", "Core", "KDE", "GNOME", "GTK", "Qt", "Motif", "Java", "ConsoleOnly"-->
        <subcategory name="Subcategory 2"/>
      </category>
      <category name="Alternative category">
        <subcategory name="Alternative Subcategory 1"/>
      </category>
    </categories>
 
    <clockspeed frequency="500"/><!--Frequency in Hz; default is 500, keep in mind that overclocking above 600 might not work for every user!-->

  </application>

</PXML>
'
