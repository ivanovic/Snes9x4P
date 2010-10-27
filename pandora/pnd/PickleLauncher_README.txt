PickleLauncher by Scott Smith (Pickle)

Summary:
PickleLauncher is meant to a simple frontend for autodetecting and configuring
frontless applications that might have different input files, like emulators.

How it works
PickleLauncher looks for its configuration from the config.txt and profile.txt.
profile.txt specfies location, file types, arguments, and entries.
config.txt contains options for the application gui and input.

Controls:
Keyboard:
PANDORA/PC
one up          : up
one down        : down
page up         : left
page down       : right
dir up          : left shoulder
dir down        : right shoulder
quit            : escape
launch/select   : enter

Joystick
GP2X/WIZ/CAANOO
one up          : up
one down        : down
page up         : left
page down       : right
dir up          : left shoulder
dir down        : right shoulder
quit            : select
launch/select   : start

Mouse and Touchscreen
All devices that support it can do all controls through the screen.
The buttons on the left do the following from top to bottom:
<<      : page up
<       : one up
>       : one down
>>      : page down
U       : one directory level up
D       : one directory level down
Sy/Sn   : current entries will be saved to profile.txt (note: any entries that have a argswap performed will save automatically)

Buttons on the right side from top to bottom:
Launch  : will launch the current item if an input file, if a directory is selected the selector will go down into the folder.
Quit    : exits back the to the menu

DETAILS for profile.txt

Global Settings
filepath=<path> : this the initial path where PickleLauncher should look for the input files
  i.e. filepath=/mnt/sd/roms or filepath=roms

Extension Settings
[ext] : this identifies the file type to look for. All following options will be associated with this extension only.
exepath=<path> : this is the path to the target applications binary file used by this extension
  i.e. exepath=/mnt/sd/games/mygame/superemu or exepath=./superapp
blacklist=<list> : this is a list of files that should not show up in the entry list (can be empty)
  i.e. blacklist=somerom.bin,anotherrom.bin
arg=<argument>,<default value> : is used to specify arguments for the extension.
  <argument> is the flag string.
  <default value> is a value assigned to each entry that is autodetected
      There are special strings for the default value:
        %filename% copies the entry filename to be used with the agrument flag
        %novalue% specifes that the argument shouldnt be used and expects the user to put an value for the entries that need it.
  i.e arg=-file,%filename%
      arg=-fs,1
      arg=-file2,%custom%
argswap=<path>.<arg number A>,<value A>,<arg number B>,<value B> : this will assign the values specfied to the 2 entries to all entries that are in the path.
  i.e argswap=./exps/are/here/,1,exp1.rom,2,%filename%

entry=<filename>,<alias>,<argvalue>,<argvalue+1>,... : the entries are normally configured and written to the profile by the PickleLauncher.
                                                       each entry should have the filename and alias (optional), and then followed by a value for each argument.
                                                       If a argument isnt needed for the entry put %custom%. Use %filename% if the argument
                                                       needs to have the filename as the argument value.

DETAILS for config.txt

NOTE: picklelauncher has built in defaults, it is suggested that you use these values. But if something isnt quite the way you want, like
color use this file to customize the gui.

# Settings
screenwidth=320                                 : the screen width in pixels (normally leave as default)
screenheight=240                                : the screen height in pixels (normally leave as default)
screendepth=16                                  : the screen depth in bits per byte (normally leave as default)
refreshdelay=100                                : the time delay after the screen is refreshed (if make it too small the application will run too fast)
fullscreen=1                                    : 1 show fullscreen 0 show in a window
showext=1                                       : 1/0 show/hide the extension at the end of the file
showhidden=0                                    : 1/0 show/hide hidden folders and files
showpointer=0                                   : 1/0 show/hide the mouse pointer (not needed for touchscreen)
showlabels=1                                    : 1/0 show/hide the text labels that overlay the buttons
unusedkeyslaunch=0                              : 1/0 use/ignore any unmapped buttons will default to launch/select
unusedbuttonslaunch=0                           : 1/0 use/ignore any unmapped keys will default to launch/select
fontsizesmall=8                                 : size of the small font
fontsizemedium=12                               : size of the medium font
fontsizelarge=14                                : size of the large font
maxentries=10                                   : maximum limit number of entries shown in the selector list
# Paths
fontpath=DejaVuSansMono-Bold.ttf                : following are paths to images that can be used to change the font, buttons, and skins.
image_background=images/background.png
image_pointer=images/pointer.png
image_upone=images/button_oneup.png
image_downone=images/button_onedn.png
image_pageup=images/button_pageup.png
image_pagedown=images/button_pagedn.png
image_dirup=images/button_dirup.png
image_dirdown=images/button_dirdn.png
image_edit=images/button_edit.png
image_options=images/button_options.png
image_launch=images/button_launch.png
image_quit=images/button_quit.png
# Colors
    # 0 white
    # 1 yellow
    # 2 fushsia
    # 3 red
    # 4 silver
    # 5 gray
    # 6 olive
    # 7 purple
    # 8 maroon
    # 9 aqua 
    # 10 lime
    # 11 teal
    # 12 green
    # 13 blue
    # 14 navy
    # 15 black
color_buttons=13                                : if no images are specifed the application falls back to simple rectangular gui. The colors 
color_fontbuttons=0                               for this mode can be set here. A list of indices and their color are above
color_background=0
color_fontfiles=15
color_fontfolders=3
# Controls
key_up=273                                      : key mappings, the numeric numbers are SDLKey sym's
key_down=274
key_left=276
key_right=275
key_dirup=306
key_dirdown=303
key_launch=13
key_quit=27
button_up=0                                     : button mappings
button_down=4
button_left=2
button_right=6
button_dirup=10
button_dirdown=11
button_launch=8
button_quit=9
deadzone=10000                                  : the deadzone for analog joysticks
