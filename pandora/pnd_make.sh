#!/bin/bash

######adjust path of genpxml.sh if you want to use that "feture"#####

PXML_schema="PXML_schema.xsd"
GENPXML_PATH="genpxml.sh"

# useful functions ...
black='\E[30m'
red='\E[31m'
green='\E[32m'
yellow='\E[33m'
blue='\E[34m'
magenta='\E[35m'
cyan='\E[36m'
white='\E[37m'


cecho ()	# Color-echo. Argument $1 = message, Argument $2 = color
{
	local default_msg="No message passed."   # Doesn't really need to be a local variable.
	message=${1:-$default_msg}               # Defaults to default message.
	color=${2:-$black}                       # Defaults to black, if not specified.
	echo -e "$color$message"
	tput sgr0                                # Reset to normal.
	return
} 


print_help()
{
	cat << EOSTREAM
pnd_make.sh - A script to package "something" into a PND.

Usage:
  $(basename ${0}) {--directory|-d} <folder> {--pndname|-p} <file> [{--compress-squashfs|-c}]
                   [{--genpxml} <file>] [{--icon|-i} <file>] [{--pxml|-x} <file>]
                   [{--schema|-s} <file>] [{--help|-h}]


Switches:
  --compress-squashfs / -c  Define wether or not the pnd should be compressed using
                            squashfs. If this parameter is selected, a compressed pnd
                            will be created.

  --directory / -d          Sets the folder that is to be used for the resulting pnd
                            to <folder>. This option is mandatory for the script to
                            function correctly.

  --genpxml                 

  --help / -h               Displays this help text.

  --icon / -i               Sets the icon that will be appended in the pnd to <file>.

  --pndname / -p            Sets the output filename of the resulting pnd to <file>.
                            This option is mandatory for the script to function
                            correctly.

  --pxml / -x               Sets the PXML file that is to be used to <file>. If you
                            neither provide a PXML file or set this entry to 'guess',
                            an existing 'PXML.xml' in your selected '--directory'
                            will be used, or the script $GENPXML_PATH will be called
                            to try to generate a basic PXML file for your.

  --schema / -s             Sets the schema file, that is to be used for validation,
                            to <file. If this is not defined, the script will try to
                            use the file 'PXML_schema.xsd'. If this fails, a warning
                            is issued.

If you select the option to create a compressed squashfs, a version >=4.0 of squashfs
is required to be available in your PATH.
EOSTREAM
}


# Parse command line parameters
while [ "${1}" != "" ]; do
	if [ "${1}" = "--compress-squashfs" ] || [ "${1}" = "-c" ];
	then
		SQUASH=1
		shift 1
	elif [ "${1}" = "--directory" ] || [ "${1}" = "-d" ];
	then
		FOLDER=$2
		shift 2
	elif [ "${1}" = "--genpxml" ];
	then
		GENPXML_PATH=$2
		shift 2
	elif [ "${1}" = "--help" ] || [ "${1}" = "-h" ];
	then
		print_help
		exit 0
	elif [ "${1}" = "--icon" ] || [ "${1}" = "-i" ];
	then
		ICON=$2
		shift 2
	elif [ "${1}" = "--pndname" ] || [ "${1}" = "-p" ];
	then
		PNDNAME=$2
		shift 2
	elif [ "${1}" = "--pxml" ] || [ "${1}" = "-x" ];
	then
		PXML=$2
		shift 2
	elif [ "${1}" = "--schema" ] || [ "${1}" = "-f" ]
	then
		PXML_schema=$2
		shift 2
	else
		cecho "ERROR: '$1' is not a known argument. Printing --help and aborting." $red
		print_help
		exit 1;
	fi
done


# Generate a PXML if the param is set to Guess or it is empty.
#TODO: make sure this does still work nicely with the latest genpxml that sebt3 is working on!
if [ ! $PXML ] || [ $PXML = "guess" ] && [ $PNDNAME ] && [ $FOLDER ];
then
	if [ -f $FOLDER/PXML.xml ]; # use the already existing PXML.xml file if there is one...
	then
		PXML=$FOLDER/PXML.xml
		PWML_ALREADY_EXISTING="true"
	else
		PXMLtxt=$($GENPXML_PATH $FOLDER $ICON)
		PXML=$FOLDER/PXML.xml
		echo "$PXMLtxt" > $FOLDER/PXML.xml
		PXML_GENERATED="true"
	fi
fi


# Probe if required variables were set
echo -e
cecho "Checking if all required variables were set." $green
if [ ! $PNDNAME ] || [ ! $FOLDER ] || [ ! $PXML ];
then
	echo -e
	cecho "ERROR: Not all required options were set! Please see the --help information below." $red
	echo -e
	print_help
	exit 1
else
	echo "PNDNAME set to '$PNDNAME'."
fi
# Check if the selected folder actually exists
if [ ! -d $FOLDER ];
then
	echo -e
	cecho "ERROR: '$FOLDER' doesnt exist or is not a folder." $red
	exit 1
else
	echo "FOLDER set to '$FOLDER'."
fi
# Check if the selected PXML file actually exists
if [ ! -f $PXML ];
then
	echo -e
	cecho "ERROR: '$PXML' doesnt exist or is not a file." $red
	exit 1
else
	if [ $PWML_ALREADY_EXISTING ];
	then
		echo "You have not explicitly specified a PXML to use, but an existing file was
fount. Will be using this one."
	elif [ $PXML_GENERATED ];
	then
		echo "A PXML file was generated for you using '$GENPXML_PATH'. This file will
not be removed at the end of this script because you might want to review it, adjust
single entries and rerun the script to generate a pnd with a PXML file with all the
information you want to have listed."
	fi
	echo "PXML set to '$PXML'."
fi

# Print the other variables:
if [ $ICON ];
then
	if [ ! -f $ICON ]
	then
		cecho "'$ICON' doesn't exist, will not append the selected icon to the pnd." $red
	else
		echo "ICON set to '$ICON'."
		USE_ICON="true"
	fi
fi
if [ $SQUASH ];
then
	echo "Will use a squashfs for '$PNDNAME'."
fi


# Validate the PXML file (if xmllint is available)
# Errors and problems in this section will be shown but are not fatal.
echo -e
cecho "Trying to validate '$PXML' now. Will be using '$PXML_schema' to do so." $green
which xmllint &> /dev/null
if [ "$?" -ne "0" ];
then
	VALIDATED=false
	cecho "WARNING: Could not find 'xmllint'. Validity check of '$PXML' is not possible!" $red
else
	if [ ! -f "$PXML_schema" ];
	then
		VALIDATED=false
		cecho "WARNING: Could not find '$PXML_schema'. If you want to validate your PXML file
please make sure to provide a schema using the --schema option." $red
	else
		xmllint --noout --schema $PXML_schema $PXML
		if [ "$?" -ne "0" ]; then VALIDATED=false; else VALIDATED=true; fi
	fi
fi
# Print some message at the end about the validation in case the user missed the output above
if [ $VALIDATED = "false" ]
then
	cecho "WARNING: Could not successfully validate '$PXML'. Please check the output
above. This does not mean that your pnd will be broken. Either you are not following the strict
syntax required for validation or you don't have all files/programs required for validating." $red
else
	cecho "Your file '$PXML' was validated successfully. The resulting pnd should
work nicely with libpnd." $green
fi


# Make iso from folder
echo -e
cecho "Creating an iso file based on '$FOLDER'." $green
if [ $SQUASH ];
then
	if [ $(mksquashfs -version | awk 'BEGIN{r=0} $3>=4{r=1} END{print r}') -eq 0 ];
	then
		cecho "ERROR: Your squashfs version is older then version 4, pleas upgrade to 4.0 or later" $red
		exit 1
	fi
	mksquashfs $FOLDER $PNDNAME.iso -nopad -no-recovery
else
	mkisofs -o $PNDNAME.iso -R $FOLDER
fi


# Append pxml to iso
echo -e
cecho "Appending '$PXML' to the created iso file." $green
cat $PNDNAME.iso $PXML > $PNDNAME
rm $PNDNAME.iso #cleanup
 
 
# Append icon if specified and available
if [ $USE_ICON ];
then
	echo -e
	cecho "Appending the icon '$ICON' to the pnd." $green
	mv $PNDNAME $PNDNAME.tmp
	cat $PNDNAME.tmp $ICON > $PNDNAME # append icon
	rm $PNDNAME.tmp #cleanup
fi


# Final message
echo -e
if [ -f $PNDNAME ];
then
	cecho "Finished creating the pnd '$PNDNAME'." $green
else
	cecho "There seems to have been a problem and '$PNDNAME' was not created. Please check
the output above for any error messages." $red
fi


#if [ $PXML = "guess" ];then rm $FOLDER/PXML.xml; fi #cleanup
