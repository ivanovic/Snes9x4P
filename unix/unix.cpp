/*
 * Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
 *
 * (c) Copyright 1996 - 2001 Gary Henderson (gary.henderson@ntlworld.com) and
 *                           Jerremy Koot (jkoot@snes9x.com)
 *
 * Super FX C emulator code
 * (c) Copyright 1997 - 1999 Ivar (ivar@snes9x.com) and
 *                           Gary Henderson.
 * Super FX assembler emulator code (c) Copyright 1998 zsKnight and _Demo_.
 *
 * DSP1 emulator code (c) Copyright 1998 Ivar, _Demo_ and Gary Henderson.
 * C4 asm and some C emulation code (c) Copyright 2000 zsKnight and _Demo_.
 * C4 C code (c) Copyright 2001 Gary Henderson (gary.henderson@ntlworld.com).
 *
 * DOS port code contains the works of other authors. See headers in
 * individual files.
 *
 * Snes9x homepage: http://www.snes9x.com
 *
 * Permission to use, copy, modify and distribute Snes9x in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Snes9x is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for Snes9x or software derived from Snes9x.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so everyone can benefit from the modifications
 * in future versions.
 *
 * Super NES and Super Nintendo Entertainment System are trademarks of
 * Nintendo Co., Limited and its subsidiary companies.
 */
#include <signal.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <SDL/SDL.h>
#include <time.h>
#include "unix/menu.h"
#include "keydef.h"

#ifdef NETPLAY_SUPPORT
	#include "../netplay.h"
#endif

#ifdef CAANOO
	#include "caanoo.h"
#else
	#include "dingoo.h"
#endif

#ifdef PANDORA
#include "font.h"
#include <linux/fb.h>
#include "unix/pandora_scaling/simple_noAA_scaler.h"
extern "C" {
#include "unix/pandora_scaling/hqx/hqx.h"
#include "unix/pandora_scaling/scale2x/scalebit.h"
}
#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, __u32)
#endif
#endif

#undef USE_THREADS
#define USE_THREADS
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#ifdef USE_THREADS
#include <pthread.h>
#include <sched.h>

pthread_t thread;
pthread_mutex_t mutex;
#endif

#include <sys/soundcard.h>
#include <sys/mman.h>

#include "snes9x.h"
#include "memmap.h"
#include "debug.h"
#include "cpuexec.h"
#include "ppu.h"
#include "snapshot.h"
#include "apu.h"
#include "display.h"
#include "gfx.h"
#include "soundux.h"
#include "spc700.h"

#include "unix/extra_defines.h"

#ifdef PANDORA
	#include "pandora_scaling/blitscale.h"
	blit_scaler_option_t blit_scalers[] = {
		// KEEP IN SYNC TO BLIT_SCALER_E or Earth Crashes Into The Sun
		{ bs_error,            bs_invalid, 0, 0,     0, 0, "          Error", "800x480" },
		{ bs_1to1,             bs_invalid, 800, 480, 0, 0, "         1 to 1", "800x480" },
		{ bs_fs_4to3,          bs_valid,   800, 480, 1, 1, "        HW: 4:3", "640x480" },
		{ bs_fs_always,        bs_valid,   800, 480, 1, 1, " HW: Fullscreen", "800x480" },
		{ bs_fs_aspect_ntsc,   bs_valid,   800, 480, 1, 1, "HW: Aspect NTSC", "585x480" },
		{ bs_fs_aspect_pal,    bs_valid,   800, 480, 1, 1, " HW: Aspect PAL", "512x480" },
		{ bs_1to2_double,      bs_valid,   800, 480, 0, 1, "      2x2 no-AA", "800x480" },
		{ bs_1to2_scale2x,     bs_valid,   800, 480, 0, 0, "    2x2 Scale2x", "800x480" },
		{ bs_1to2_smooth,      bs_valid,   800, 480, 0, 0, "   2x2 Smoothed", "800x480" },
		{ bs_1to32_multiplied, bs_valid,   800, 480, 0, 1, "      3x2 no-AA", "800x480" },
// 		{ bs_1to32_smooth,         bs_invalid, 3, 2, 0,       "3x2 Smoothed", "800x480" },
// 		{ bs_fs_aspect_multiplied, bs_invalid, 0xFF, 0xFF, 0, "Fullscreen (aspect) (unsmoothed)", "800x480" },
// 		{ bs_fs_aspect_smooth,     bs_invalid, 0xFF, 0xFF, 0, "Fullscreen (aspect) (smoothed)", "800x480" },
// 		{ bs_fs_always_multiplied, bs_invalid, 0xFF, 0xFF, 0, "Fullscreen (unsmoothed)", "800x480" },
// 		{ bs_fs_always_smooth,     bs_invalid, 0xFF, 0xFF, 0, "Fullscreen (smoothed)", "800x480" },
		{ bs_fs_always_smooth,     bs_invalid, 0xFF, 0xFF, 0, "Fullscreen (smoothed)", "800x480" },  //fake used to make sure that there is no segfault when scrolling through, don't delete!
	};

	blit_scaler_e g_scale = bs_1to2_double;
	//blit_scaler_e g_scale = bs_1to32_multiplied;
	unsigned char g_scanline = 0; // pixel doubling, but skipping the vertical alternate lines
	unsigned char g_vsync = 1; // if >0, do vsync
	int g_fb = -1; // fb for framebuffer
	int cut_top = 0;
	int cut_bottom = 0;
	int cut_left = 0;
	int cut_right = 0;
#endif

#ifdef CAANOO
	SDL_Joystick* keyssnes;
#else
	uint8 *keyssnes;
#endif

#ifdef NETPLAY_SUPPORT
	static uint32	joypads[8];
	static uint32	old_joypads[8];
#endif

// SaveSlotNumber
int SaveSlotNum = 0;

bool8_32 Scale = FALSE;
char msg[256];
short vol=50;
static int mixerdev = 0;
clock_t start;

int OldSkipFrame;
void InitTimer ();
void *S9xProcessSound (void *);
void OutOfMemory();

#ifdef DINGOO
	void gp2x_sound_volume(int l, int r);
#endif

extern void S9xDisplayFrameRate (uint8 *, uint32);
extern void S9xDisplayString (const char *string, uint8 *, uint32, int);
extern SDL_Surface *screen,*gfxscreen;

static uint32 ffc = 0;
uint32 xs = 320; //width
uint32 ys = 240; //height
uint32 cl = 12; //ypos in highres mode
uint32 cs = 0;
uint32 mfs = 10; //skippedframes

char *rom_filename = NULL;
char *snapshot_filename = NULL;

#ifndef _ZAURUS
#if defined(__linux) || defined(__sun)
static void sigbrkhandler(int)
{
#ifdef DEBUGGER
    CPU.Flags |= DEBUG_MODE_FLAG;
    signal(SIGINT, (SIG_PF) sigbrkhandler);
#endif
}
#endif
#endif

void S9xParseArg (char **argv, int &i, int argc)
{

    if (strcasecmp (argv [i], "-b") == 0 ||
	     strcasecmp (argv [i], "-bs") == 0 ||
	     strcasecmp (argv [i], "-buffersize") == 0)
    {
	if (i + 1 < argc)
	    Settings.SoundBufferSize = atoi (argv [++i]);
	else
	    S9xUsage ();
    }
    else if (strcmp (argv [i], "-l") == 0 ||
	     strcasecmp (argv [i], "-loadsnapshot") == 0)
    {
	if (i + 1 < argc)
	    snapshot_filename = argv [++i];
	else
	    S9xUsage ();
    }
    else if (strcmp (argv [i], "-xs") == 0)
    {
	if (i + 1 < argc)
	    xs = atoi(argv [++i]);
	else
	    S9xUsage ();
	}
    else if (strcmp (argv [i], "-ys") == 0)
    {
	if (i + 1 < argc)
	    ys = atoi(argv [++i]);
	else
	    S9xUsage ();
	}
    else if (strcmp (argv [i], "-cl") == 0)
    {
	if (i + 1 < argc)
	    cl = atoi(argv [++i]);
	else
	    S9xUsage ();
	}
    else if (strcmp (argv [i], "-cs") == 0)
    {
	if (i + 1 < argc)
	    cs = atoi(argv [++i]);
	else
	    S9xUsage ();
	}
    else if (strcmp (argv [i], "-mfs") == 0)
    {
	if (i + 1 < argc)
	    mfs = atoi(argv [++i]);
	else
	    S9xUsage ();
	}
    else
	    S9xUsage ();
}

extern "C"
int main (int argc, char **argv)
{
    start = clock();
    if (argc < 2)
		S9xUsage ();

    ZeroMemory (&Settings, sizeof (Settings));

    Settings.JoystickEnabled = FALSE;	//unused
#ifdef DINGOO
    Settings.SoundPlaybackRate = 2;
#else
    Settings.SoundPlaybackRate = 5; //default would be '2', use 32000Hz as the genuine hardware does
#endif
    Settings.Stereo = TRUE;
	Settings.SoundBufferSize = 512; //256 //2048
    Settings.CyclesPercentage = 100;
    Settings.DisableSoundEcho = FALSE;
    Settings.AltSampleDecode = 0;
    Settings.APUEnabled = Settings.NextAPUEnabled = TRUE;
    Settings.H_Max = SNES_CYCLES_PER_SCANLINE;
#ifdef PANDORA
    Settings.SkipFrames = 1;
    Settings.DisplayFrameRate = FALSE;
#else
    Settings.SkipFrames = AUTO_FRAMERATE;
    Settings.DisplayFrameRate = TRUE;
#endif
    Settings.ShutdownMaster = TRUE;
    Settings.FrameTimePAL = 20000;
    Settings.FrameTimeNTSC = 16667;
    Settings.FrameTime = Settings.FrameTimeNTSC;
    Settings.DisableSampleCaching = FALSE;
    Settings.DisableMasterVolume = FALSE;
    Settings.Mouse = FALSE;	//TRUE
    Settings.SuperScope = FALSE;
    Settings.MultiPlayer5 = FALSE;
    Settings.ControllerOption = SNES_MULTIPLAYER5;
    Settings.ControllerOption = 0;
#ifdef PANDORA
    Settings.Transparency = TRUE;
#else
    Settings.Transparency = FALSE; //TRUE;
#endif
    Settings.SixteenBit = TRUE;
    Settings.SupportHiRes = FALSE; //autodetected for known highres roms
    Settings.NetPlay = FALSE;
    Settings.ServerName [0] = 0;
    Settings.ThreadSound = TRUE;
    Settings.AutoSaveDelay = 30;
    Settings.ApplyCheats = TRUE;
    Settings.TurboMode = FALSE;
    Settings.TurboSkipFrames = 15;
    if (Settings.ForceNoTransparency)
		Settings.Transparency = FALSE;
    if (Settings.Transparency)
		Settings.SixteenBit = TRUE;

	//parse commandline arguments for ROM filename
	rom_filename = S9xParseArgs (argv, argc);

	//printf( "Playbackrate1: %02d\n",Settings.SoundPlaybackRate );

    Settings.HBlankStart = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;

    if (!Memory.Init () || !S9xInitAPU())
    {
		OutOfMemory ();
	}

   (void) S9xInitSound (Settings.SoundPlaybackRate, Settings.Stereo, Settings.SoundBufferSize);

    if (!Settings.APUEnabled)
		S9xSetSoundMute (TRUE);

    uint32 saved_flags = CPU.Flags;

#ifdef GFX_MULTI_FORMAT
    S9xSetRenderPixelFormat (RGB565);
#endif

#ifndef DINGOO
    // ROM selector if no rom filename is available!!!!!!!!!!!!!!
    if (!rom_filename)
    {
	    S9xInitDisplay (argc, argv);
	    if (!S9xGraphicsInit ())
	    {
			OutOfMemory ();
		}
	    S9xInitInputDevices ();

	    // just to init Font here for ROM selector
	    S9xReset ();

	    do
	    {
			rom_filename = menu_romselector();
		}while(rom_filename==NULL);

		S9xDeinitDisplay();
		printf ("Romfile selected: %s\n", rom_filename);
	}
#endif

    if (rom_filename)
    {
		if (!Memory.LoadROM (rom_filename))
		{
		    char dir [_MAX_DIR + 1];
		    char drive [_MAX_DRIVE + 1];
		    char name [_MAX_FNAME + 1];
		    char ext [_MAX_EXT + 1];
		    char fname [_MAX_PATH + 1];

		    _splitpath (rom_filename, drive, dir, name, ext);
		    _makepath (fname, drive, dir, name, ext);

		    strcpy (fname, S9xGetROMDirectory ());
		    strcat (fname, SLASH_STR);
		    strcat (fname, name);

		    if (ext [0])
		    {
				strcat (fname, ".");
				strcat (fname, ext);
		    }
		    _splitpath (fname, drive, dir, name, ext);
		    _makepath (fname, drive, dir, name, ext);

		    if (!Memory.LoadROM (fname))
		    {
				printf ("Error opening: %s\n", rom_filename);
				OutOfMemory();
		    }
		}
		Memory.LoadSRAM (S9xGetFilename (".srm"));
		
#ifdef PANDORA
		// load last preferences if available
		std::string this_line;
		std::string romSettingsFileName = std::string(S9xGetFilename (".scfg"));
		std::ifstream ifs ( romSettingsFileName.c_str() , std::ifstream::in );

		while (std::getline(ifs,this_line))
		{
			if (this_line != "")
			{
				if (this_line.find("#") != 0) // skip all lines that have a # at the *very* first position
				{
					if (this_line.find("alternative_sample_decoding=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						if (this_line == "1")
							Settings.AltSampleDecode = 1;
						else
							Settings.AltSampleDecode = 0;
					}
					else if (this_line.find("cut_top=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						cut_top = atoi ( this_line.c_str() );
					}
					else if (this_line.find("cut_bottom=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						cut_bottom = atoi ( this_line.c_str() );
					}
					else if (this_line.find("cut_left=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						cut_left = atoi ( this_line.c_str() );
					}
					else if (this_line.find("cut_right=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						cut_right = atoi ( this_line.c_str() );
					}
					else if (this_line.find("display_mode=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						std::string current_scaler = blit_scalers [ g_scale ].desc_en;
						int safe_abort = 0; // used to be able to break out of the loop in case something goes *really* bad!
						while ( ( current_scaler != this_line || blit_scalers [ g_scale ].valid == bs_invalid ) && safe_abort < 20 )
						{
							g_scale = (blit_scaler_e) ( ( g_scale + 1 ) % bs_max );
							current_scaler = blit_scalers [ g_scale ].desc_en;
							++safe_abort;
						}
						if (safe_abort >= 20)
						{
							std::cerr << "unknown or broken entry for display_mode! switching to default '2x2 no-AA'." << std::endl;
							g_scale = bs_1to2_double;
						}
					}
					else if (this_line.find("frameskip=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						Settings.SkipFrames = atoi ( this_line.c_str() );
					}
					else if (this_line.find("savestate_slot=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						SaveSlotNum = atoi ( this_line.c_str() );
						if ( SaveSlotNum >= MAX_SAVE_SLOTS || SaveSlotNum < 0 )
							SaveSlotNum = 0;
					}
					else if (this_line.find("show_fps=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						if ( this_line == "1" )
							Settings.DisplayFrameRate = TRUE;
						else
							Settings.DisplayFrameRate = FALSE;
					}
					else if (this_line.find("transparency=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						if ( this_line == "1" )
							Settings.Transparency = TRUE;
						else
							Settings.Transparency = FALSE;
					}
					else if (this_line.find("vsync=") == 0)
					{
						this_line = this_line.substr(this_line.find("=")+1);
						if (this_line == "1")
							g_vsync = 1;
						else
							g_vsync = 0;
					}
					else 
					{
						std::cerr << "unknown entry in list of rom specific settings, ignoring this entry" << std::endl;
					}
				}
			}
		}
		ifs.close();
#endif
    }
    else
    {
	    S9xExit();
    }

    S9xInitDisplay (argc, argv);
    if (!S9xGraphicsInit ())
    {
		OutOfMemory ();
	}

#ifdef PANDORA
    hqxInit();
#endif

    S9xInitInputDevices ();

    CPU.Flags = saved_flags;
    Settings.StopEmulation = FALSE;

#ifdef DEBUGGER
	struct sigaction sa;
	sa.sa_handler = sigbrkhandler;
#ifdef SA_RESTART
	sa.sa_flags = SA_RESTART;
#else
	sa.sa_flags = 0;
#endif
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
#endif

#ifdef NETPLAY_SUPPORT
	if (strlen(Settings.ServerName) == 0)
	{
		char	*server = getenv("S9XSERVER");
		if (server)
		{
			strncpy(Settings.ServerName, server, 127);
			Settings.ServerName[127] = 0;
		}
	}

	char	*port = getenv("S9XPORT");
	if (Settings.Port >= 0 && port)
		Settings.Port = atoi(port);
	else
	if (Settings.Port < 0)
		Settings.Port = -Settings.Port;

	if (Settings.NetPlay)
	{
		NetPlay.MaxFrameSkip = 10;

		if (!S9xNPConnectToServer(Settings.ServerName, Settings.Port, Memory.ROMName))
		{
			fprintf(stderr, "Failed to connect to server %s on port %d.\n", Settings.ServerName, Settings.Port);
			S9xExit();
		}

		fprintf(stderr, "Connected to server %s on port %d as player #%d playing %s.\n", Settings.ServerName, Settings.Port, NetPlay.Player, Memory.ROMName);
	}
#endif

	//Handheld Key Infos
#ifdef CAANOO
	sprintf(msg,"Press HOME to Show MENU");
#elif PANDORA
    sprintf(msg,"Press SPACEBAR to Show MENU");
#elif CYGWIN32
	sprintf(msg,"Press ESC+LALT to Show MENU");
#else
    sprintf(msg,"Press SELECT+B to Show MENU");
#endif
    S9xSetInfoString(msg);

    //load Snapshot
    if (snapshot_filename)
    {
		int Flags = CPU.Flags & (DEBUG_MODE_FLAG | TRACE_FLAG);
		if (!S9xLoadSnapshot (snapshot_filename))
		    exit (1);
		CPU.Flags |= Flags;
    }

#ifndef _ZAURUS
    S9xGraphicsMode ();
    sprintf (String, "\"%s\" %s: %s", Memory.ROMName, TITLE, VERSION);
    S9xSetTitle (String);
#endif

#ifdef JOYSTICK_SUPPORT
	uint32	JoypadSkip = 0;
#endif

	InitTimer();
	S9xSetSoundMute(FALSE);

#ifdef NETPLAY_SUPPORT
	bool8	NP_Activated = Settings.NetPlay;
#endif

	while (1)
	{
#ifdef NETPLAY_SUPPORT
		if (NP_Activated)
		{
			if (NetPlay.PendingWait4Sync && !S9xNPWaitForHeartBeatDelay(100))
			{
				S9xProcessEvents(FALSE);
				continue;
			}

//			for (int J = 0; J < 8; J++)
//				old_joypads[J] = MovieGetJoypad(J);

//			for (int J = 0; J < 8; J++)
//				MovieSetJoypad(J, joypads[J]);

			if (NetPlay.Connected)
			{
				if (NetPlay.PendingWait4Sync)
				{
					NetPlay.PendingWait4Sync = FALSE;
					NetPlay.FrameCount++;
					S9xNPStepJoypadHistory();
				}
			}
			else
			{
				fprintf(stderr, "Lost connection to server.\n");
				S9xExit();
			}
		}
#endif

#ifdef DEBUGGER
		if (!Settings.Paused || (CPU.Flags & (DEBUG_MODE_FLAG | SINGLE_STEP_FLAG)))
#else
		if (!Settings.Paused)
#endif
			S9xMainLoop();

#ifdef NETPLAY_SUPPORT
		if (NP_Activated)
		{
//			for (int J = 0; J < 8; J++)
//				MovieSetJoypad(J, old_joypads[J]);
		}
#endif

#ifdef DEBUGGER
		if (Settings.Paused || (CPU.Flags & DEBUG_MODE_FLAG))
#else
		if (Settings.Paused)
#endif
			S9xSetSoundMute(TRUE);

#ifdef DEBUGGER
		if (CPU.Flags & DEBUG_MODE_FLAG)
			S9xDoDebug();
		else
#endif
		if (Settings.Paused)
		{
			S9xProcessEvents(FALSE);
			usleep(100000);
		}

#ifdef JOYSTICK_SUPPORT
		if (unixSettings.JoystickEnabled && (JoypadSkip++ & 1) == 0)
			ReadJoysticks();
#endif

		S9xProcessEvents(TRUE);

#ifdef DEBUGGER
		if (!Settings.Paused && !(CPU.Flags & DEBUG_MODE_FLAG))
#else
		if (!Settings.Paused)
#endif
			S9xSetSoundMute(FALSE);
	}

	return (0);
}

void S9xAutoSaveSRAM ()
{
    Memory.SaveSRAM (S9xGetFilename (".srm"));
}

void OutOfMemory()
{
    fprintf (stderr, "Snes9X: Memory allocation failure - not enough RAM/virtual memory available.\n S9xExiting...\n");

    Memory.Deinit ();
    S9xDeinitAPU ();
    S9xDeinitDisplay();

    exit (1);
}

void S9xExit()
{
#ifdef PANDORA
	std::string romSettingsFileName = std::string(S9xGetFilename (".scfg"));
	std::ofstream romSettingsFile;
	romSettingsFile.open ( romSettingsFileName.c_str() );
	if (romSettingsFile)
	{
		romSettingsFile << "# This file was automatically created. Please do not modify it!" << std::endl;
		romSettingsFile << "# Filename: " << romSettingsFileName << std::endl;
		
		
		if ( Settings.AltSampleDecode == 1 ) // there seem to be some issues with this data type, so got to work around it...
			romSettingsFile << "alternative_sample_decoding=1"  << std::endl;
		else
			romSettingsFile << "alternative_sample_decoding=0" << std::endl;
		romSettingsFile << "display_mode=" << blit_scalers [ g_scale ].desc_en << std::endl;
		romSettingsFile << "cut_top=" << cut_top << std::endl;
		romSettingsFile << "cut_bottom=" << cut_bottom << std::endl;
		romSettingsFile << "cut_left=" << cut_left << std::endl;
		romSettingsFile << "cut_right=" << cut_right << std::endl;
		romSettingsFile << "frameskip=" << Settings.SkipFrames << std::endl;
		if ( SaveSlotNum >= MAX_SAVE_SLOTS || SaveSlotNum < 0 )
			SaveSlotNum = 0; //reset savestate_slot number to 0 if "out of bounds"
		romSettingsFile << "savestate_slot=" << SaveSlotNum << std::endl;
		romSettingsFile << "show_fps=" << Settings.DisplayFrameRate << std::endl;
		romSettingsFile << "transparency=" << Settings.Transparency << std::endl;
		romSettingsFile.close();
	}
	else
		std::cerr << "could not open file for saving rom preferences!" << std::endl;
#endif

	S9xSetSoundMute(true);

#ifdef USE_THREADS
    if (Settings.ThreadSound)
    {
		int status = pthread_cancel(thread);
		if (status != 0)
			perror ("Error pthread_cancel");
	}
#endif

#ifdef NETPLAY_SUPPORT
	if (Settings.NetPlay)
		S9xNPDisconnect();
#endif

    Memory.SaveSRAM (S9xGetFilename (".srm"));
//    S9xSaveCheatFile (S9xGetFilename (".cht")); // not needed for embedded devices

    Memory.Deinit ();
    S9xDeinitAPU ();
    S9xDeinitDisplay ();

    SDL_ShowCursor(SDL_ENABLE);
    SDL_Quit();

    exit (0);
}

Uint16 sfc_key[256];
void S9xInitInputDevices ()
{
#ifdef CAANOO
	keyssnes = SDL_JoystickOpen(0);
#else
	keyssnes = SDL_GetKeyState(NULL);
#endif

	memset(sfc_key, 0, 256);

#ifdef CAANOO
	// Caanoo mapping
	sfc_key[A_1] = CAANOO_BUTTON_B;	//Snes A
	sfc_key[B_1] = CAANOO_BUTTON_X;	//Snes B
	sfc_key[X_1] = CAANOO_BUTTON_Y;	//Snes X
	sfc_key[Y_1] = CAANOO_BUTTON_A;	//Snes Y
	sfc_key[L_1] = CAANOO_BUTTON_L;
	sfc_key[R_1] = CAANOO_BUTTON_R;
	sfc_key[START_1] = CAANOO_BUTTON_HELP1;
	sfc_key[SELECT_1] = CAANOO_BUTTON_HELP2;
	sfc_key[LEFT_1] = CAANOO_BUTTON_LEFT;
	sfc_key[RIGHT_1] = CAANOO_BUTTON_RIGHT;
	sfc_key[UP_1] = CAANOO_BUTTON_UP;
	sfc_key[DOWN_1] = CAANOO_BUTTON_DOWN;

	sfc_key[QUIT] = CAANOO_BUTTON_HOME;
#elif PANDORA
	// Pandora mapping
	sfc_key[A_1] = SDLK_END; //DINGOO_BUTTON_A; A = B
	sfc_key[B_1] = SDLK_PAGEDOWN; //DINGOO_BUTTON_B; B = X
	sfc_key[X_1] = SDLK_PAGEUP; //DINGOO_BUTTON_X; X = Y
	sfc_key[Y_1] = SDLK_HOME; //DINGOO_BUTTON_Y; Y = A
	sfc_key[L_1] = SDLK_RSHIFT; //DINGOO_BUTTON_L;
	sfc_key[R_1] = SDLK_RCTRL; // DINGOO_BUTTON_R;
	sfc_key[START_1] = SDLK_LALT; //DINGOO_BUTTON_START;
	sfc_key[SELECT_1] = SDLK_LCTRL; //DINGOO_BUTTON_SELECT;
	sfc_key[LEFT_1] = SDLK_LEFT; //DINGOO_BUTTON_LEFT;
	sfc_key[RIGHT_1] = SDLK_RIGHT; //DINGOO_BUTTON_RIGHT;
	sfc_key[UP_1] = SDLK_UP; //DINGOO_BUTTON_UP;
	sfc_key[DOWN_1] = SDLK_DOWN; //DINGOO_BUTTON_DOWN;
/*
	// for now, essentially unmapped
	sfc_key[LEFT_2] = SDLK_g;
	sfc_key[RIGHT_2] = SDLK_j;
	sfc_key[UP_2] = SDLK_u;
	sfc_key[DOWN_2] = SDLK_n;
	sfc_key[LU_2] = SDLK_y;
	sfc_key[LD_2] = SDLK_b;
	sfc_key[RU_2] = SDLK_i;
	sfc_key[RD_2] = SDLK_m;

	sfc_key[QUIT] = SDLK_ESCAPE;
	sfc_key[ACCEL] = SDLK_TAB;
*/
#else
	// Dingoo mapping
	sfc_key[A_1] = DINGOO_BUTTON_A;
	sfc_key[B_1] = DINGOO_BUTTON_B;
	sfc_key[X_1] = DINGOO_BUTTON_X;
	sfc_key[Y_1] = DINGOO_BUTTON_Y;
	sfc_key[L_1] = DINGOO_BUTTON_L;
	sfc_key[R_1] = DINGOO_BUTTON_R;
	sfc_key[START_1] = DINGOO_BUTTON_START;
	sfc_key[SELECT_1] = DINGOO_BUTTON_SELECT;
	sfc_key[LEFT_1] = DINGOO_BUTTON_LEFT;
	sfc_key[RIGHT_1] = DINGOO_BUTTON_RIGHT;
	sfc_key[UP_1] = DINGOO_BUTTON_UP;
	sfc_key[DOWN_1] = DINGOO_BUTTON_DOWN;
/*
	// for now, essentially unmapped
	sfc_key[LEFT_2] = SDLK_g;
	sfc_key[RIGHT_2] = SDLK_j;
	sfc_key[UP_2] = SDLK_u;
	sfc_key[DOWN_2] = SDLK_n;
	sfc_key[LU_2] = SDLK_y;
	sfc_key[LD_2] = SDLK_b;
	sfc_key[RU_2] = SDLK_i;
	sfc_key[RD_2] = SDLK_m;

	sfc_key[QUIT] = SDLK_d;
	sfc_key[ACCEL] = SDLK_u;
*/
#endif

	int i = 0;
	char *envp, *j;
	envp = j = getenv ("S9XKEYS");
	if (envp) {
		do {
			if (j = strchr(envp, ','))
				*j = 0;
			if (i == 0) sfc_key[QUIT] = atoi(envp);
			else if (i == 1)  sfc_key[A_1] = atoi(envp);
			else if (i == 2)  sfc_key[B_1] = atoi(envp);
			else if (i == 3)  sfc_key[X_1] = atoi(envp);
			else if (i == 4)  sfc_key[Y_1] = atoi(envp);
			else if (i == 5)  sfc_key[L_1] = atoi(envp);
			else if (i == 6)  sfc_key[R_1] = atoi(envp);
			else if (i == 7)  sfc_key[START_1] = atoi(envp);
			else if (i == 8)  sfc_key[SELECT_1] = atoi(envp);
			else if (i == 9)  sfc_key[LEFT_1] = atoi(envp);
			else if (i == 10) sfc_key[RIGHT_1] = atoi(envp);
			else if (i == 11) sfc_key[UP_1] = atoi(envp);
			else if (i == 12) sfc_key[DOWN_1] = atoi(envp);
/*			else if (i == 13) sfc_key[LU_2] = atoi(envp);
			else if (i == 14) sfc_key[LD_2] = atoi(envp);
			else if (i == 15) sfc_key[RU_2] = atoi(envp);
			else if (i == 16) sfc_key[RD_2] = atoi(envp);
*/			envp = j + 1;
			++i;
		} while(j);
	}

}

const char *GetHomeDirectory ()
{
#if CAANOO
    return (".");
#elif CYGWIN32
    return (".");
#else
    return (getenv ("HOME"));
#endif
}

const char *S9xGetSnapshotDirectory ()
{
    static char filename [PATH_MAX];
    const char *snapshot;

    if (!(snapshot = getenv ("SNES9X_SNAPSHOT_DIR")) &&
	!(snapshot = getenv ("SNES96_SNAPSHOT_DIR")))
    {
		const char *home = GetHomeDirectory ();
		strcpy (filename, home);
		strcat (filename, SLASH_STR);
		strcat (filename, ".snes96_snapshots");
		mkdir (filename, 0777);
		chown (filename, getuid (), getgid ());
    }
    else
	return (snapshot);

    return (filename);
}

const char *S9xGetFilename (const char *ex)
{
    static char filename [PATH_MAX + 1];
    char drive [_MAX_DRIVE + 1];
    char dir [_MAX_DIR + 1];
    char fname [_MAX_FNAME + 1];
    char ext [_MAX_EXT + 1];

    _splitpath (Memory.ROMFilename, drive, dir, fname, ext);
    strcpy (filename, S9xGetSnapshotDirectory ());
    strcat (filename, SLASH_STR);
    strcat (filename, fname);
    strcat (filename, ex);

    return (filename);
}

const char *S9xGetROMDirectory ()
{
    const char *roms;

    if (!(roms = getenv ("SNES9X_ROM_DIR")) && !(roms = getenv ("SNES96_ROM_DIR")))
		return ("." SLASH_STR "roms");
    else
		return (roms);
}

const char *S9xBasename (const char *f)
{
    const char *p;
    if ((p = strrchr (f, '/')) != NULL || (p = strrchr (f, '\\')) != NULL)
	return (p + 1);

    return (f);
}

bool8 S9xOpenSnapshotFile (const char *fname, bool8 read_only, STREAM *file)
{
    char filename [PATH_MAX + 1];
    char drive [_MAX_DRIVE + 1];
    char dir [_MAX_DIR + 1];
    char ext [_MAX_EXT + 1];

    _splitpath (fname, drive, dir, filename, ext);

    if (*drive || *dir == '/' ||
	(*dir == '.' && (*(dir + 1) == '/'
        )))
    {
	strcpy (filename, fname);
	if (!*ext)
	    strcat (filename, ".s96");
    }
    else
    {
	strcpy (filename, S9xGetSnapshotDirectory ());
	strcat (filename, SLASH_STR);
	strcat (filename, fname);
	if (!*ext)
	    strcat (filename, ".s96");
    }

#ifdef ZLIB
    if (read_only)
    {
	if ((*file = OPEN_STREAM (filename, "rb")))
	    return (TRUE);
    }
    else
    {
	if ((*file = OPEN_STREAM (filename, "wb")))
	{
	    chown (filename, getuid (), getgid ());
	    return (TRUE);
	}
    }
#else
    char command [PATH_MAX];

    if (read_only)
    {
	sprintf (command, "gzip -d <\"%s\"", filename);
	if (*file = popen (command, "r"))
	    return (TRUE);
    }
    else
    {
	sprintf (command, "gzip --best >\"%s\"", filename);
	if (*file = popen (command, "wb"))
	    return (TRUE);
    }
#endif
    return (FALSE);
}

void S9xCloseSnapshotFile (STREAM file)
{
#ifdef ZLIB
    CLOSE_STREAM (file);
#else
    pclose (file);
#endif
    sync();
}

bool8_32 S9xInitUpdate ()
{
    return (TRUE);
}

//		uint32 xs = 320, ys = 240, cl = 0, cs = 0, mfs = 10;
#ifdef PANDORA
bool8_32 S9xDeinitUpdate ( int Width, int Height ) {
	//fprintf (stderr, "width: %d, height: %d\n", Width, Height);
	//std::cerr << "current size: " << screen -> w << "x" << screen -> h << std::endl;
	
	uint16 source_panewidth = 320; // LoRes games are rendered into a 320 wide SDL screen
	if (Settings.SupportHiRes)
		source_panewidth = 512; // HiRes games are rendered into a 512 wide SDL screen
	
	// the following two vars are used with the non HW scalers as well as when displaying text overlay
	uint16 widescreen_center_x = 0;
	uint16 widescreen_center_y = 0;
	
	SDL_LockSurface(screen);
	
	if ( blit_scalers [ g_scale ].hw_fullscreen )
	{
		if ( ( screen->w != Width ) || ( screen->h != Height ))
		{
			//std::cerr << "resetting video mode in S9xDeinitUpdate:v2"<< std::endl;
			//set sdl layersize environment variable
			setenv("SDL_OMAP_LAYER_SIZE",blit_scalers [ g_scale ].layersize,1);

			//set sdl bordercut environment variable
			char cutborders[20];
			sprintf(cutborders, "%d,%d,%d,%d", cut_left, cut_right, cut_top, cut_bottom);
			setenv("SDL_OMAP_BORDER_CUT",cutborders,1);

// 			screen = SDL_SetVideoMode( Width , Height, 16,
// 					SDL_DOUBLEBUF|SDL_FULLSCREEN);

			screen = SDL_SetVideoMode( Width , Height, 16,
					SDL_SWSURFACE|SDL_FULLSCREEN);
		}
		
		render_x_single_xy((uint16*)(screen -> pixels) /*destination_pointer_address*/,
						(screen -> pitch) >> 1 /*screen_pitch_half*/,
						(uint16*)(GFX.Screen), source_panewidth, Width, Height);
	}
	else 
	{
		//NOTE: This block should no longer be required since only valid modes
		//      are allowed to be selected when changing the display mode.
		//if ( ( screen->w != blit_scalers [ g_scale ].res_x ) || ( screen->h != blit_scalers [ g_scale ].res_y ) )
		//{
		//	std::cerr << "resetting video mode in S9xDeinitUpdate:v3"<< std::endl;
		//	setenv("SDL_OMAP_LAYER_SIZE",blit_scalers [ g_scale ].layersize,1);
		//	screen = SDL_SetVideoMode( blit_scalers [ g_scale ].res_x , blit_scalers [ g_scale ].res_y, 16,
		//			SDL_SWSURFACE|SDL_FULLSCREEN);
		//}
		
		// get the pitch only once...
		// pitch is in 1b increments, so is 2* what you think!
		uint16 screen_pitch_half = (screen -> pitch) >> 1;
		
		//pointer to the screen
		uint16* screen_pixels = (uint16*)(screen -> pixels);
		
		// this line for centering in Y direction always assumes that Height<=240 and double scaling in Y is wanted (interlacing!)
		// screen_pitch_half * (480-(Heigth*2))/2; due to shifting no "div by zero" not possible
		// heigth is usually 224, 239 or 240!
		widescreen_center_y = ( 480 - ( Height << 1 ) ) >> 1;
		uint16 widescreen_center_y_fb = widescreen_center_y * screen_pitch_half;
		// destination pointer address: pointer to screen_pixels plus moving for centering
		uint16* destination_pointer_address;
		
		
		switch (g_scale) {
			case bs_1to2_double:
				// the pandora screen has a width of 800px, so everything above should be handled differently
				// only some scenes in hires roms use 512px width, otherwise the max is 320px in the menu!
				// hires modules themselves come with two modes, one with 512 width, one with 256
				if (Width > 400 ) {
					widescreen_center_x = ( 800 - Width ) >> 1; // ( screen -> w - Width ) / 2
					destination_pointer_address = screen_pixels + widescreen_center_x + widescreen_center_y_fb;
					
					render_x_single(destination_pointer_address, screen_pitch_half,
									(uint16*)(GFX.Screen), source_panewidth, Width, Height);
				}
				else {
					widescreen_center_x = ( 800 - ( Width << 1 ) ) >> 1; // ( screen -> w - ( Width * 2 ) ) / 2
					// destination pointer address: pointer to screen_pixels plus moving for centering
					destination_pointer_address = screen_pixels + widescreen_center_x + widescreen_center_y_fb;
					
					render_x_double(destination_pointer_address, screen_pitch_half,
									(uint16*)(GFX.Screen), source_panewidth, Width, Height);
				}
				break;
			case bs_1to32_multiplied:
				widescreen_center_x = 16; // screen -> w - 3*Width // 800-3*Width
				// destination pointer address: pointer to screen_pixels plus moving for centering
				destination_pointer_address = screen_pixels + 16 + widescreen_center_y_fb;
				
				// the pandora screen has a width of 800px, so everything above should be handled differently
				// only some scenes in hires roms use 512px width, otherwise the max is 320px in the menu!
				// hires modules themselves come with two modes, one with 512 width, one with 256
				if (Width > 400 ) {
					render_x_oneandhalf(destination_pointer_address, screen_pitch_half,
									(uint16*)(GFX.Screen), source_panewidth, Width, Height);
				}
				else {
					if ( Width > 266 ) //crash in hardware mode if Width is too high... 266*3 < 800
						Width = 256;
					render_x_triple(destination_pointer_address, screen_pitch_half,
									(uint16*)(GFX.Screen), source_panewidth, Width, Height);
				}
				break;
			case bs_1to2_smooth:
					widescreen_center_x = ( 800 - ( Width << 1 ) ) >> 1; // ( screen -> w - ( Width * 2 ) ) / 2
					hq2x_16((uint16*)(GFX.Screen), screen_pixels, Width, Height, screen->w, screen->h);
				break;
			case bs_1to2_scale2x:
					widescreen_center_x = ( 800 - ( Width << 1 ) ) >> 1; // ( screen -> w - ( Width * 2 ) ) / 2
					destination_pointer_address = screen_pixels + widescreen_center_x + widescreen_center_y_fb;
					
					scale(2, (uint16*)destination_pointer_address, screen->w*2, (uint16*)GFX.Screen, 320*2, 2, Width, Height);
				break;
			default:
				// code error; unknown scaler
				fprintf ( stderr, "invalid scaler option handed to render code; fix me!\n" );
				exit ( 0 );
		}
	}
	
	
//The part below is the version that should be used when you want scanline support.
//if you don't want scanlines, the other system should be faster.
// 		for (register uint16 i = 0; i < Height; ++i) {
// 			// first scanline of doubled pair
// 			register uint16 *dp16 = destination_pointer_address + ( i * screen_pitch );
// 			
// 			register uint16 *sp16 = (uint16*)(GFX.Screen);
// 			sp16 += ( i * 320 );
// 			
// 			for (register uint16 j = 0; j < Width /*256*/; ++j, ++sp16) {
// 				*dp16++ = *sp16;
// 				*dp16++ = *sp16; // doubled
// 			}
// 			
// 			if ( ! g_scanline ) {
// 				// second scanline of doubled pair
// 				dp16 = destination_pointer_address + ( i * screen_pitch ) + screen_pitch_half;
// 				
// 				sp16 = (uint16*)(GFX.Screen);
// 				sp16 += ( i * 320 );
// 				for (register uint16 j = 0; j < Width /*256*/; ++j, ++sp16) {
// 					*dp16++ = *sp16;
// 					*dp16++ = *sp16; // doubled
// 				}
// 			} // scanline
// 		} // for each height unit

	if (Settings.DisplayFrameRate) {
		//S9xDisplayFrameRate ((uint8 *)screen->pixels + 64, 800 * 2 * 2 );
		
		widescreen_center_x = widescreen_center_x << 1; // the pitch has to be taken into account here, so double the values!
		
		uint8* temp_pointer = (uint8 *) screen->pixels
								+ 16
								+ widescreen_center_x // move to the right if we are in "non hw scaling mode"
								+ ( screen->h - font_height - 1 - widescreen_center_y ) * (screen -> pitch);
		S9xDisplayFrameRate ( temp_pointer, (screen -> pitch) );
	}

	//display the info string above the possibly active framerate display
	if (GFX.InfoString) {
		//S9xDisplayString (GFX.InfoString, (uint8 *)screen->pixels + 64, 800 * 2 * 2, 240 );
		
		if ( ! Settings.DisplayFrameRate ) // only take the pitch into account if this was not done before!
			widescreen_center_x = widescreen_center_x << 1; // the pitch has to be taken into account here, so double the values!
		
		uint16 string_x_startpos = 14 // 2px indention applied by function, resulting in the same 16 as with the framerate counter
									+ (font_width - 1) * sizeof (uint16) // framerate counter starts with empty char, so move to the right by one
									+ widescreen_center_x; // move to the right if we are in "non hw scaling mode"
		
		S9xDisplayString (GFX.InfoString,
						  (uint8 *)screen->pixels + string_x_startpos,
						  (screen -> pitch),
						  screen->h + font_height * 3 - 2 - widescreen_center_y); // "font_height * 3 - 2" to place it in the line above the framerate counter
	}

	SDL_UnlockSurface(screen);

	// vsync
	if ( g_vsync && g_fb >= 0 ) {
		int arg = 0;
		ioctl ( g_fb, FBIO_WAITFORVSYNC, &arg );
	}

	// update the actual screen
	//SDL_UpdateRect(screen,0,0,0,0);
	// The following line only makes "real" sense if in doublebuffering mode.
	// this is currently not working as nicely as it could/should, so not
	// activating it. While in a swsurface this just behaves the same as a plain
	// SDL_UpdateRect(screen,0,0,0,0);
	SDL_Flip(screen);

  return(TRUE);
}
#else
bool8_32 S9xDeinitUpdate (int Width, int Height)
{
//printf("Width=%d, Height=%d\n",Width,Height);

	register uint32 lp = (xs > 256) ? 16 : 0;

	if (Width > 256)
		lp *= 2;

	if (Settings.SupportHiRes)
	{
		if (Width > 256)
		{
			//Wenn SupportHiRes activ und HighRes Frame
			for (register uint32 i = 0; i < Height; i++) {
				register uint16 *dp16 = (uint16 *)(screen->pixels) + ((i + cl) * xs) + lp;
				register uint32 *sp32 = (uint32 *)(GFX.Screen) + (i << 8) + cs;
				for (register uint32 j = 0; j < 256; j++) {
					*dp16++ = *sp32++;
				}
			}
		}
		else
		{
			//Wenn SupportHiRes activ aber kein HighRes Frame
			for (register uint32 i = 0; i < Height; i++) {
				register uint32 *dp32 = (uint32 *)(screen->pixels) + ((i + cl) * xs / 2) + lp;
				register uint32 *sp32 = (uint32 *)(GFX.Screen) + (i << 8) + cs;
				for (register uint32 j = 0; j < 128; j++) {
					*dp32++ = *sp32++;
				}
			}
		}

		if (GFX.InfoString)
		    S9xDisplayString (GFX.InfoString, (uint8 *)screen->pixels + 64, 640,0);
		else if (Settings.DisplayFrameRate)
		    S9xDisplayFrameRate ((uint8 *)screen->pixels + 64, 640);

		SDL_UpdateRect(screen,32,0,256,Height);
	}
	else
	{
		// if scaling for non-highres (is centered)
		if(Scale)
		{
			int x,y,s;
			uint32 x_error;
			static uint32 x_fraction=52428;
		 	char temp[512];
			register uint8 *d;

			//center ypos if ysize is only 224px
			int yoffset = 8*(Height == 224);

			for (y = Height-1;y >= 0; y--)
			{
			    d = GFX.Screen + y * 640;
			    memcpy(temp,d,512);
			    d += yoffset*640;
			    x_error = x_fraction;
			    s=0;

			    for (x = 0; x < 320; x++)
			    {
					x_error += x_fraction;

					if(x_error >= 0x10000)
					{
					    *d++ = temp[s++];
					    *d++ = temp[s++];
					    x_error -= 0x10000;
					}
					else
					{
						*d++ = ((temp[s-2] ^ temp[s])>>1)&0xEF | ((temp[s-2] & temp[s]));
						*d++ = ((temp[s-1] ^ temp[s+1])>>1)&0x7B | ((temp[s-1] & temp[s+1]));
					}
			    }
			}
			if (GFX.InfoString)
			    S9xDisplayString (GFX.InfoString, (uint8 *)screen->pixels + 64, 640,0);
			else if (Settings.DisplayFrameRate)
			    S9xDisplayFrameRate ((uint8 *)screen->pixels + 64, 640);
	
			SDL_UpdateRect(screen,0,yoffset,320,Height);
		}
		else
		{
			//center ypos if ysize is only 224px
//			int yoffset = 8*(Height == 224);
			
			if (GFX.InfoString)
			    S9xDisplayString (GFX.InfoString, (uint8 *)screen->pixels + 64, 640,0);
			else if (Settings.DisplayFrameRate)
			    S9xDisplayFrameRate ((uint8 *)screen->pixels + 64, 640);
	
//		    SDL_UpdateRect(screen,0,yoffset,320,Height);
		    SDL_UpdateRect(screen,32,0,256,Height);
		}
	}

	return(TRUE);
}
#endif

#ifndef _ZAURUS
static unsigned long now ()
{
    static unsigned long seconds_base = 0;
    struct timeval tp;
    gettimeofday (&tp, NULL);
    if (!seconds_base)
	seconds_base = tp.tv_sec;

    return ((tp.tv_sec - seconds_base) * 1000 + tp.tv_usec / 1000);
}
#endif

void _makepath (char *path, const char *, const char *dir,
		const char *fname, const char *ext)
{
    if (dir && *dir)
    {
	strcpy (path, dir);
	strcat (path, "/");
    }
    else
	*path = 0;
    strcat (path, fname);
    if (ext && *ext)
    {
        strcat (path, ".");
        strcat (path, ext);
    }
}

void _splitpath (const char *path, char *drive, char *dir, char *fname,
		 char *ext)
{
    *drive = 0;

    char *slash = strrchr ( (char*) path, '/');
    if (!slash)
	slash = strrchr ( (char*) path, '\\');

    char *dot = strrchr ( (char*) path, '.');

    if (dot && slash && dot < slash)
	dot = NULL;

    if (!slash)
    {
	strcpy (dir, "");
	strcpy (fname, path);
        if (dot)
        {
	    *(fname + (dot - path)) = 0;
	    strcpy (ext, dot + 1);
        }
	else
	    strcpy (ext, "");
    }
    else
    {
	strcpy (dir, path);
	*(dir + (slash - path)) = 0;
	strcpy (fname, slash + 1);
        if (dot)
	{
	    *(fname + (dot - slash) - 1) = 0;
    	    strcpy (ext, dot + 1);
	}
	else
	    strcpy (ext, "");
    }
}

#ifndef _ZAURUS
void S9xToggleSoundChannel (int c)
{
    if (c == 8)
	so.sound_switch = 255;
    else
	so.sound_switch ^= 1 << c;
    S9xSetSoundControl (so.sound_switch);
}
#endif

static void SoundTrigger ()
{
    if (Settings.APUEnabled && !so.mute_sound)
		S9xProcessSound (NULL);
}

void StopTimer ()
{
    struct itimerval timeout;

    timeout.it_interval.tv_sec = 0;
    timeout.it_interval.tv_usec = 0;
    timeout.it_value.tv_sec = 0;
    timeout.it_value.tv_usec = 0;
    if (setitimer (ITIMER_REAL, &timeout, NULL) < 0)
	perror ("setitimer");
}

void InitTimer ()
{
    struct itimerval timeout;
    struct sigaction sa;

#ifdef USE_THREADS
    if (Settings.ThreadSound)
    {
		pthread_mutex_init (&mutex, NULL);
		pthread_create (&thread, NULL, S9xProcessSound, NULL);
		return;
    }
#endif

    sa.sa_handler = (SIG_PF) SoundTrigger;

#if defined (SA_RESTART)
    sa.sa_flags = SA_RESTART;
#else
    sa.sa_flags = 0;
#endif

    sigemptyset (&sa.sa_mask);
    sigaction (SIGALRM, &sa, NULL);

    timeout.it_interval.tv_sec = 0;
    timeout.it_interval.tv_usec = 10000;
    timeout.it_value.tv_sec = 0;
    timeout.it_value.tv_usec = 10000;
    if (setitimer (ITIMER_REAL, &timeout, NULL) < 0)
	perror ("setitimer");
}

void S9xSyncSpeed ()
{
#ifdef NETPLAY_SUPPORT
	if (Settings.NetPlay && NetPlay.Connected)
	{
	#if defined(NP_DEBUG) && NP_DEBUG == 2
		printf("CLIENT: SyncSpeed @%d\n", S9xGetMilliTime());
	#endif

		S9xNPSendJoypadUpdate(old_joypads[0]);
		for (int J = 0; J < 8; J++)
			joypads[J] = S9xNPGetJoypad(J);

		if (!S9xNPCheckForHeartBeat())
		{
			NetPlay.PendingWait4Sync = !S9xNPWaitForHeartBeatDelay(100);
		#if defined(NP_DEBUG) && NP_DEBUG == 2
			if (NetPlay.PendingWait4Sync)
				printf("CLIENT: PendingWait4Sync1 @%d\n", S9xGetMilliTime());
		#endif

			IPPU.RenderThisFrame = TRUE;
			IPPU.SkippedFrames = 0;
		}
		else
		{
			NetPlay.PendingWait4Sync = !S9xNPWaitForHeartBeatDelay(200);
		#if defined(NP_DEBUG) && NP_DEBUG == 2
			if (NetPlay.PendingWait4Sync)
				printf("CLIENT: PendingWait4Sync2 @%d\n", S9xGetMilliTime());
		#endif

			if (IPPU.SkippedFrames < NetPlay.MaxFrameSkip)
			{
				IPPU.RenderThisFrame = FALSE;
				IPPU.SkippedFrames++;
			}
			else
			{
				IPPU.RenderThisFrame = TRUE;
				IPPU.SkippedFrames = 0;
			}
		}

		if (!NetPlay.PendingWait4Sync)
		{
			NetPlay.FrameCount++;
			S9xNPStepJoypadHistory();
		}

		return;
	}
#endif
    if (!Settings.TurboMode && Settings.SkipFrames == AUTO_FRAMERATE)
    {
		static struct timeval next1 = {0, 0};
		struct timeval now;

		while (gettimeofday (&now, NULL) < 0) ;
		if (next1.tv_sec == 0)
		{
		    next1 = now;
		    next1.tv_usec++;
		}

		if (timercmp(&next1, &now, >))
		{
		    if (IPPU.SkippedFrames == 0)
		    {
				do
				{
				    CHECK_SOUND ();
				    while (gettimeofday (&now, NULL) < 0) ;
				} while (timercmp(&next1, &now, >));
		    }
		    IPPU.RenderThisFrame = TRUE;
		    IPPU.SkippedFrames = 0;
		}
		else
		{
		    if (IPPU.SkippedFrames < mfs)
		    {
			IPPU.SkippedFrames++;
			IPPU.RenderThisFrame = FALSE;
		    }
		    else
		    {
			IPPU.RenderThisFrame = TRUE;
			IPPU.SkippedFrames = 0;
			next1 = now;
		    }
		}
		next1.tv_usec += Settings.FrameTime;
		if (next1.tv_usec >= 1000000)
		{
		    next1.tv_sec += next1.tv_usec / 1000000;
		    next1.tv_usec %= 1000000;
		}
    }
    else
    {
		if (++IPPU.FrameSkip >= (Settings.TurboMode ? Settings.TurboSkipFrames
							    : Settings.SkipFrames))
		{
		    IPPU.FrameSkip = 0;
		    IPPU.SkippedFrames = 0;
		    IPPU.RenderThisFrame = TRUE;
		}
		else
		{
		    IPPU.SkippedFrames++;
		    IPPU.RenderThisFrame = FALSE;
		}
    }
}

void S9xProcessEvents (bool8_32 block)
{
	SDL_Event event;

	while(block && SDL_PollEvent(&event))
	{
		switch(event.type)
		{
#ifdef CAANOO
			// CAANOO -------------------------------------------------------------
			case SDL_JOYBUTTONDOWN:
				keyssnes = SDL_JoystickOpen(0);
				//QUIT Emulator
				if ( SDL_JoystickGetButton(keyssnes, sfc_key[QUIT]) && SDL_JoystickGetButton(keyssnes, sfc_key[B_1] ) )
				{
					S9xExit();
				}
				// MAINMENU
				else if ( SDL_JoystickGetButton(keyssnes, sfc_key[QUIT]) )
				{
					S9xSetSoundMute (TRUE);
					menu_loop();
					S9xSetSoundMute(FALSE);
				}
				break;

			case SDL_JOYBUTTONUP:
				keyssnes = SDL_JoystickOpen(0);
				switch(event.jbutton.button)
				{
				}
				break;
#else
			//PANDORA & DINGOO ------------------------------------------------------
			case SDL_KEYDOWN:
				keyssnes = SDL_GetKeyState(NULL);

				// shortcut
#ifdef PANDORA
				if ( event.key.keysym.sym == SDLK_ESCAPE )
				{
					S9xExit();
				}
				if ( event.key.keysym.sym == SDLK_s )
				{
					do
					{
						g_scale = (blit_scaler_e) ( ( g_scale + 1 ) % bs_max );
					} while ( ( blit_scalers [ g_scale ].valid == bs_invalid )
								|| ( Settings.SupportHiRes && !(blit_scalers [ g_scale ].support_hires) ) );
					S9xDeinitDisplay();
					S9xInitDisplay(0, 0);
				}

#endif //PANDORA
				//QUIT Emulator
				if ( (keyssnes[sfc_key[SELECT_1]] == SDL_PRESSED) &&(keyssnes[sfc_key[START_1]] == SDL_PRESSED) && (keyssnes[sfc_key[X_1]] == SDL_PRESSED) )
				{
					S9xExit();
				}
				//RESET ROM Playback
				else if ((keyssnes[sfc_key[SELECT_1]] == SDL_PRESSED) && (keyssnes[sfc_key[START_1]] == SDL_PRESSED) && (keyssnes[sfc_key[B_1]] == SDL_PRESSED))
				{
					//make sure the sram is stored before resetting the console
					//it should work without, but better safe than sorry...
					Memory.SaveSRAM (S9xGetFilename (".srm"));
					S9xReset();
				}
				//SAVE State
				else if ( (keyssnes[sfc_key[START_1]] == SDL_PRESSED) && (keyssnes[sfc_key[R_1]] == SDL_PRESSED) )
				{
					//extern char snapscreen;
					char fname[256], ext[20];
					S9xSetSoundMute(true);
					sprintf(ext, ".00%d", SaveSlotNum);
					strcpy(fname, S9xGetFilename (ext));
					S9xFreezeGame (fname);
					capt_screenshot();
					sprintf(ext, ".s0%d", SaveSlotNum);
					strcpy(fname, S9xGetFilename (ext));
					save_screenshot(fname);
					S9xSetSoundMute(false);
				}
				//LOAD State
				else if ( (keyssnes[sfc_key[START_1]] == SDL_PRESSED) && (keyssnes[sfc_key[L_1]] == SDL_PRESSED) )
				{
					char fname[256], ext[8];
					S9xSetSoundMute(true);
					sprintf(ext, ".00%d", SaveSlotNum);
					strcpy(fname, S9xGetFilename (ext));
					S9xLoadSnapshot (fname);
					S9xSetSoundMute(false);
				}
				// MAINMENU
				else if ((keyssnes[sfc_key[SELECT_1]] == SDL_PRESSED)&&(keyssnes[sfc_key[B_1]] == SDL_PRESSED) )
				{
					S9xSetSoundMute(true);
					menu_loop();
					S9xSetSoundMute(false);
#ifdef DINGOO
					//S9xSetMasterVolume (vol, vol);
					gp2x_sound_volume(vol, vol);
#endif

#ifdef PANDORA
				}
				// another shortcut I'm afraid
				else if (event.key.keysym.sym == SDLK_SPACE)
				{
					S9xSetSoundMute(true);
					menu_loop();
					S9xSetSoundMute(false);
					//S9xSetMasterVolume (vol, vol);
				}
				else if (event.key.keysym.sym == SDLK_t)
				{
					Settings.TurboMode = !Settings.TurboMode;
#endif //PANDORA
				}
				break;
			case SDL_KEYUP:
				keyssnes = SDL_GetKeyState(NULL);
				break;
#endif //CAANOO
		}
	}
}

//#endif

static long log2 (long num)
{
    long n = 0;

    while (num >>= 1)
	n++;

    return (n);
}

uint32 S9xReadJoypad (int which1)
{
	uint32 val=0x80000000;

	if (which1 > 4)
		return 0;

#ifdef CAANOO
	//player1
	if (SDL_JoystickGetButton(keyssnes, sfc_key[L_1]))			val |= SNES_TL_MASK;
	if (SDL_JoystickGetButton(keyssnes, sfc_key[R_1]))			val |= SNES_TR_MASK;
	if (SDL_JoystickGetButton(keyssnes, sfc_key[X_1]))			val |= SNES_X_MASK;
	if (SDL_JoystickGetButton(keyssnes, sfc_key[Y_1]))			val |= SNES_Y_MASK;
	if (SDL_JoystickGetButton(keyssnes, sfc_key[B_1]))			val |= SNES_B_MASK;
	if (SDL_JoystickGetButton(keyssnes, sfc_key[A_1]))			val |= SNES_A_MASK;
	if (SDL_JoystickGetButton(keyssnes, sfc_key[START_1]))		val |= SNES_START_MASK;
	if (SDL_JoystickGetButton(keyssnes, sfc_key[SELECT_1]))		val |= SNES_SELECT_MASK;
	if (SDL_JoystickGetAxis(keyssnes, 1) < -20000)				val |= SNES_UP_MASK;
	if (SDL_JoystickGetAxis(keyssnes, 1) > 20000)				val |= SNES_DOWN_MASK;
	if (SDL_JoystickGetAxis(keyssnes, 0) < -20000)				val |= SNES_LEFT_MASK;
	if (SDL_JoystickGetAxis(keyssnes, 0) > 20000)				val |= SNES_RIGHT_MASK;
#else
	//player1
	if (keyssnes[sfc_key[L_1]] == SDL_PRESSED)		val |= SNES_TL_MASK;
	if (keyssnes[sfc_key[R_1]] == SDL_PRESSED)		val |= SNES_TR_MASK;
	if (keyssnes[sfc_key[X_1]] == SDL_PRESSED)		val |= SNES_X_MASK;
	if (keyssnes[sfc_key[Y_1]] == SDL_PRESSED)		val |= SNES_Y_MASK;
	if (keyssnes[sfc_key[B_1]] == SDL_PRESSED)		val |= SNES_B_MASK;
	if (keyssnes[sfc_key[A_1]] == SDL_PRESSED)		val |= SNES_A_MASK;
	if (keyssnes[sfc_key[START_1]] == SDL_PRESSED)	val |= SNES_START_MASK;
	if (keyssnes[sfc_key[SELECT_1]] == SDL_PRESSED)	val |= SNES_SELECT_MASK;
	if (keyssnes[sfc_key[UP_1]] == SDL_PRESSED)		val |= SNES_UP_MASK;
	if (keyssnes[sfc_key[DOWN_1]] == SDL_PRESSED)	val |= SNES_DOWN_MASK;
	if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED)	val |= SNES_LEFT_MASK;
	if (keyssnes[sfc_key[RIGHT_1]] == SDL_PRESSED)	val |= SNES_RIGHT_MASK;
	//player2
	/*
	if (keyssnes[sfc_key[UP_2]] == SDL_PRESSED)		val |= SNES_UP_MASK;
	if (keyssnes[sfc_key[DOWN_2]] == SDL_PRESSED)	val |= SNES_DOWN_MASK;
	if (keyssnes[sfc_key[LEFT_2]] == SDL_PRESSED)	val |= SNES_LEFT_MASK;
	if (keyssnes[sfc_key[RIGHT_2]] == SDL_PRESSED)	val |= SNES_RIGHT_MASK;
	if (keyssnes[sfc_key[LU_2]] == SDL_PRESSED)	val |= SNES_LEFT_MASK | SNES_UP_MASK;
	if (keyssnes[sfc_key[LD_2]] == SDL_PRESSED)	val |= SNES_LEFT_MASK | SNES_DOWN_MASK;
	if (keyssnes[sfc_key[RU_2]] == SDL_PRESSED)	val |= SNES_RIGHT_MASK | SNES_UP_MASK;
	if (keyssnes[sfc_key[RD_2]] == SDL_PRESSED)	val |= SNES_RIGHT_MASK | SNES_DOWN_MASK;
	*/
#endif

#ifdef NETPLAY_SUPPORT
    if (Settings.NetPlay)
		return (S9xNPGetJoypad (which1));
#endif

	return(val);
}

//===============================================================================================
// SOUND
//===============================================================================================

static int Rates[8] =
{
//    0, 8192, 11025, 16000, 22050, 29300, 36600, 44000
	0, 8192, 11025, 16000, 22050, 32000, 44100, 48000
};

static int BufferSizes [8] =
{
    0, 256, 256, 256, 512, 512, 1024, 1024
};


static uint8 Buf[MAX_BUFFER_SIZE] __attribute__((aligned(4)));

#define FIXED_POINT 0x10000
#define FIXED_POINT_SHIFT 16
#define FIXED_POINT_REMAINDER 0xffff

static volatile bool8 block_signal = FALSE;
static volatile bool8 block_generate_sound = FALSE;
static volatile bool8 pending_signal = FALSE;

bool8_32 S9xOpenSoundDevice (int mode, bool8_32 stereo, int buffer_size)
{
#ifndef CYGWIN32
    int J, K;

    if ((so.sound_fd = open ("/dev/dsp", O_WRONLY|O_ASYNC)) < 0)
    {
		perror ("/dev/dsp");
		return (FALSE);
    }

    mixerdev = open("/dev/mixer", O_RDWR);
    ioctl(mixerdev, SOUND_MIXER_READ_VOLUME, &vol);
    vol = vol>>8;

    J = AFMT_S16_LE;
//    J = AFMT_U8;
    if (ioctl (so.sound_fd, SNDCTL_DSP_SETFMT, &J) < 0)
    {
		perror ("ioctl SNDCTL_DSP_SETFMT");
		return (FALSE);
    }
	so.sixteen_bit = TRUE;
    so.stereo = stereo;
    if (ioctl (so.sound_fd, SNDCTL_DSP_STEREO, &so.stereo) < 0)
    {
		perror ("ioctl SNDCTL_DSP_STEREO");
		return (FALSE);
    }

    so.playback_rate = Rates[mode & 0x07];
 //   so.playback_rate = 16000;

    if (ioctl (so.sound_fd, SNDCTL_DSP_SPEED, &so.playback_rate) < 0)
    {
		perror ("ioctl SNDCTL_DSP_SPEED");
		return (FALSE);
    }

    S9xSetPlaybackRate (so.playback_rate);

    //    if (buffer_size == 0)
so.buffer_size = buffer_size;
//	so.buffer_size = buffer_size = BufferSizes [mode & 7];

	//	buffer_size = so.buffer_size = 256;

    if (buffer_size > MAX_BUFFER_SIZE / 4)
	buffer_size = MAX_BUFFER_SIZE / 4;
//    if (so.sixteen_bit)
	buffer_size *= 2;
    if (so.stereo)
	buffer_size *= 2;

    int power2 = log2 (buffer_size);
    J = K = power2 | (3 << 16);
    if (ioctl (so.sound_fd, SNDCTL_DSP_SETFRAGMENT, &J) < 0)
    {
		perror ("ioctl SNDCTL_DSP_SETFRAGMENT");
		return (FALSE);
    }

    printf ("Rate: %d, Buffer size: %d, 16-bit: %s, Stereo: %s, Encoded: %s\n",
	    so.playback_rate, so.buffer_size, so.sixteen_bit ? "yes" : "no",
	    so.stereo ? "yes" : "no", so.encoded ? "yes" : "no");

#ifdef DINGOO
    gp2x_sound_volume(vol,vol);
#endif //DINGOO

#endif
    return (TRUE);
}

void S9xGenerateSound ()
{
    int bytes_so_far = (so.samples_mixed_so_far << 1);
//    int bytes_so_far = so.sixteen_bit ? (so.samples_mixed_so_far << 1) :
//				        so.samples_mixed_so_far;
#ifndef _ZAURUS
    if (Settings.SoundSync == 2)
    {
	// Assumes sound is signal driven
	while (so.samples_mixed_so_far >= so.buffer_size && !so.mute_sound)
	    pause ();
    }
    else
#endif
    if (bytes_so_far >= so.buffer_size)
	return;

#ifdef USE_THREADS
    if (Settings.ThreadSound)
    {
		if (block_generate_sound || pthread_mutex_trylock (&mutex))
		    return;
    }
#endif

    block_signal = TRUE;

    so.err_counter += so.err_rate;
    if (so.err_counter >= FIXED_POINT)
    {
        int sample_count = so.err_counter >> FIXED_POINT_SHIFT;
	int byte_offset;
	int byte_count;

        so.err_counter &= FIXED_POINT_REMAINDER;
	if (so.stereo)
	    sample_count <<= 1;
	byte_offset = bytes_so_far + so.play_position;

	do
	{
	    int sc = sample_count;
	    byte_count = sample_count;
//	    if (so.sixteen_bit)
		byte_count <<= 1;

	    if ((byte_offset & SOUND_BUFFER_SIZE_MASK) + byte_count > SOUND_BUFFER_SIZE)
	    {
		sc = SOUND_BUFFER_SIZE - (byte_offset & SOUND_BUFFER_SIZE_MASK);
		byte_count = sc;
//		if (so.sixteen_bit)
		    sc >>= 1;
	    }
	    if (bytes_so_far + byte_count > so.buffer_size)
	    {
		byte_count = so.buffer_size - bytes_so_far;
		if (byte_count == 0)
		    break;
		sc = byte_count;
//		if (so.sixteen_bit)
		    sc >>= 1;
	    }
	    S9xMixSamplesO (Buf, sc, byte_offset & SOUND_BUFFER_SIZE_MASK);
	    so.samples_mixed_so_far += sc;
	    sample_count -= sc;
	    bytes_so_far = (so.samples_mixed_so_far << 1);
//	    bytes_so_far = so.sixteen_bit ? (so.samples_mixed_so_far << 1) :
//	 	           so.samples_mixed_so_far;
	    byte_offset += byte_count;
	} while (sample_count > 0);
    }
    block_signal = FALSE;

#ifdef USE_THREADS
    if (Settings.ThreadSound)
		pthread_mutex_unlock (&mutex);
    else
#endif
    if (pending_signal)
    {
		S9xProcessSound (NULL);
		pending_signal = FALSE;
    }
}

void *S9xProcessSound (void *)
{
//    audio_buf_info info;
//    if (!Settings.ThreadSound &&
//	(ioctl (so.sound_fd, SNDCTL_DSP_GETOSPACE, &info) == -1 ||
//	 info.bytes < so.buffer_size))
//    {
//	return (NULL);
//    }

#ifdef USE_THREADS
    do
    {
#endif

    int sample_count = so.buffer_size;
    int byte_offset;

//    if (so.sixteen_bit)
	sample_count >>= 1;

#ifdef USE_THREADS
//    if (Settings.ThreadSound)
	pthread_mutex_lock (&mutex);
//    else
#endif
//    if (block_signal)
//    {
//	pending_signal = TRUE;
//	return (NULL);
//    }

    block_generate_sound = TRUE;

    if (so.samples_mixed_so_far < sample_count)
    {
	//	byte_offset = so.play_position +
	//		      (so.sixteen_bit ? (so.samples_mixed_so_far << 1)
	//				      : so.samples_mixed_so_far);
		byte_offset = so.play_position + (so.samples_mixed_so_far << 1);

		if (Settings.SoundSync == 2)
		{
		    memset (Buf + (byte_offset & SOUND_BUFFER_SIZE_MASK), 0, sample_count - so.samples_mixed_so_far);
		}
		else
		{
		    S9xMixSamplesO (Buf, sample_count - so.samples_mixed_so_far, byte_offset & SOUND_BUFFER_SIZE_MASK);
	    }

		so.samples_mixed_so_far = 0;
    }
    else
    {
		so.samples_mixed_so_far -= sample_count;
	}

//    if (!so.mute_sound)
    {
		int I;
		int J = so.buffer_size;

		byte_offset = so.play_position;
		so.play_position = (so.play_position + so.buffer_size) & SOUND_BUFFER_SIZE_MASK;

#ifdef USE_THREADS
	//	if (Settings.ThreadSound)
		    pthread_mutex_unlock (&mutex);
#endif
		block_generate_sound = FALSE;
		do
		{
		    if (byte_offset + J > SOUND_BUFFER_SIZE)
		    {
			I = write (so.sound_fd, (char *) Buf + byte_offset,
				   SOUND_BUFFER_SIZE - byte_offset);
			if (I > 0)
			{
			    J -= I;
			    byte_offset = (byte_offset + I) & SOUND_BUFFER_SIZE_MASK;
			}
		    }
		    else
		    {
			I = write (so.sound_fd, (char *) Buf + byte_offset, J);
			if (I > 0)
			{
			    J -= I;
			    byte_offset = (byte_offset + I) & SOUND_BUFFER_SIZE_MASK;
			}
		    }
		} while ((I < 0 && errno == EINTR) || J > 0);
    }

#ifdef USE_THREADS
//    } while (Settings.ThreadSound);
    } while (1);
#endif

    return (NULL);
}

#ifdef DINGOO
void gp2x_sound_volume(int l, int r)
{
 	l=l<0?0:l; l=l>255?255:l; r=r<0?0:r; r=r>255?255:r;
 	l<<=8; l|=r;
  	ioctl(mixerdev, SOUND_MIXER_WRITE_VOLUME, &l);
}
#endif

//===============================================================================================
