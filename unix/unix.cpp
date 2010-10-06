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

#include "../netplay.h"

#ifdef CAANOO
	#include "caanoo.h"
#else
	#include "dingoo.h"
#endif

#ifdef PANDORA
#include <linux/fb.h>
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

#ifdef PANDORA
	#include "blitscale.h"
	blit_scaler_option_t blit_scalers[] = {
	  // KEEP IN SYNC TO BLIT_SCALER_E or Earth Crashes Into The Sun
	  { bs_error,                bs_invalid, 0, 0,       "Error" },
	  { bs_1to1,                 bs_invalid, 1, 1,       "1 to 1" },
	  { bs_1to2_double,          bs_valid,   2, 2,       "2x2 no-AA" },
	  { bs_1to2_smooth,          bs_invalid, 2, 2,       "2x2 Smoothed" },
	  { bs_1to32_multiplied,     bs_valid,   3, 2,       "3x2 no-AA" },
	  { bs_1to32_smooth,         bs_invalid, 3, 2,       "3x2 Smoothed" },
	  { bs_fs_aspect_multiplied, bs_invalid, 0xFF, 0xFF, "Fullscreen (aspect) (unsmoothed)" },
	  { bs_fs_aspect_smooth,     bs_invalid, 0xFF, 0xFF, "Fullscreen (aspect) (smoothed)" },
	  { bs_fs_always_multiplied, bs_invalid, 0xFF, 0xFF, "Fullscreen (unsmoothed)" },
	  { bs_fs_always_smooth,     bs_invalid, 0xFF, 0xFF, "Fullscreen (smoothed)" },
	};
	
	blit_scaler_e g_scale = bs_1to2_double;
	//blit_scaler_e g_scale = bs_1to32_multiplied;
	unsigned char g_fullscreen = 1;
	unsigned char g_scanline = 0; // pixel doubling, but skipping the vertical alternate lines
	unsigned char g_vsync = 1; // if >0, do vsync
	int g_fb = -1; // fb for framebuffer
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
char SaveSlotNum = 0;

bool8_32 Scale = FALSE;
char msg[256];
short vol=50;
static int mixerdev = 0;
clock_t start;

int OldSkipFrame;
void InitTimer ();
void *S9xProcessSound (void *);
void OutOfMemory();
//void gp2x_sound_volume(int l, int r);

extern void S9xDisplayFrameRate (uint8 *, uint32);
extern void S9xDisplayString (const char *string, uint8 *, uint32, int);
extern SDL_Surface *screen,*gfxscreen;

static uint32 ffc = 0;
uint32 xs = 320, ys = 240, cl = 0, cs = 0, mfs = 10;

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
    Settings.SoundPlaybackRate = 2;
    Settings.Stereo = TRUE;
    Settings.SoundBufferSize = 512; //256
    Settings.CyclesPercentage = 100;
    Settings.DisableSoundEcho = FALSE;
    Settings.APUEnabled = Settings.NextAPUEnabled = TRUE;
    Settings.H_Max = SNES_CYCLES_PER_SCANLINE;
#ifdef PANDORA
    Settings.SkipFrames = 1;
#else
    Settings.SkipFrames = AUTO_FRAMERATE;
#endif
    Settings.ShutdownMaster = TRUE;
    Settings.FrameTimePAL = 20000;
    Settings.FrameTimeNTSC = 16667;
    Settings.FrameTime = Settings.FrameTimeNTSC;
    Settings.DisableSampleCaching = FALSE;
    Settings.DisableMasterVolume = FALSE;
    Settings.Mouse = TRUE;
    Settings.SuperScope = FALSE;
    Settings.MultiPlayer5 = FALSE;
    Settings.ControllerOption = SNES_MULTIPLAYER5;
    Settings.ControllerOption = 0;
    Settings.Transparency = TRUE;
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

    Settings.HBlankStart = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;

    if (!Memory.Init () || !S9xInitAPU())
    {
		OutOfMemory ();
	}

   (void) S9xInitSound (Settings.SoundPlaybackRate, Settings.Stereo,
			 Settings.SoundBufferSize);

    if (!Settings.APUEnabled)
	S9xSetSoundMute (TRUE);

    uint32 saved_flags = CPU.Flags;

#ifdef GFX_MULTI_FORMAT
    S9xSetRenderPixelFormat (RGB565);
#endif

    S9xInitDisplay (argc, argv);
    if (!S9xGraphicsInit ())
    {
		OutOfMemory ();
	}
    S9xInitInputDevices ();

    // just to init Font here for ROM selector    
    S9xReset ();

    // ROM selector if no rom filename is available!!!!!!!!!!!!!!
    do
    {
//    if (!rom_filename)
		rom_filename = menu_romselector();
	}while(rom_filename==NULL);

	printf ("Romfile selected: %s\n", rom_filename);

//	if(!rom_filename)
//		S9xExit();

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
    }
    else
    {
	    S9xExit();
//		S9xReset ();
//		Settings.Paused |= 2;
    }

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
//		if (NP_Activated)
//		{
//			for (int J = 0; J < 8; J++)
//				MovieSetJoypad(J, old_joypads[J]);
//		}
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
    S9xSetSoundMute (TRUE);

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
    return (getenv ("HOME"));
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
    
    if (!(roms = getenv ("SNES9X_ROM_DIR")) &&
	!(roms = getenv ("SNES96_ROM_DIR")))
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
/*
#ifndef _ZAURUS
const char *S9xChooseFilename (bool8 read_only)
{
    char def [PATH_MAX + 1];
    char title [PATH_MAX + 1];
    char drive [_MAX_DRIVE + 1];
    char dir [_MAX_DIR + 1];
    char ext [_MAX_EXT + 1];

    _splitpath (Memory.ROMFilename, drive, dir, def, ext);
    strcat (def, ".s96");
    sprintf (title, "%s snapshot filename",
	    read_only ? "Select load" : "Choose save");
    const char *filename;

    S9xSetSoundMute (TRUE);
    filename = S9xSelectFilename (def, S9xGetSnapshotDirectory (), "s96", title);
    S9xSetSoundMute (FALSE);
    return (filename);
}
#endif
*/
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

  /* rules:
   * if highres mode -> g_scale should always be 1 (ie: no scaling, since high res mode is 512x480
   * for non-highres-mode -> g_scale can be 1 (1:1) or 2 (1:2, pixel doubling)
   *
   * future:
   * g_scale -> to imply anti-aliasing or stretch-blit or other scaling modes
   */

  // NEEDS WORK, THIS IS JUST TO GET WORKING

  register uint32 lp = (xs > 256) ? 16 : 0;
  if (Width > 256 ) {
    lp *= 2;
  }

  if ( g_scale == bs_1to2_double ) {

    for (register uint32 i = 0; i < Height; i++) {

      // first scanline of doubled pair
      register uint16 *dp16 = (uint16*)(screen -> pixels);
      dp16 += ( i * screen -> pitch ); // pitch is in 1b increments, so is 2* what you think!
      dp16 += ( screen -> w - ( Width * 2 ) ) / 2; // center horiz

      register uint16 *sp16 = (uint16*)(GFX.Screen);
      sp16 += ( i * 320 );

      for (register uint32 j = 0; j < Width + 32/*256*/; j++, sp16++) {
	*dp16++ = *sp16;
	*dp16++ = *sp16;
      }

      if ( ! g_scanline ) {
	// second scanline of doubled pair
	dp16 = (uint16*)(screen -> pixels);
	dp16 += ( i * screen -> pitch );
	dp16 += ( screen -> pitch / 2 );
	dp16 += ( screen -> w - ( Width * 2 ) ) / 2; // center horiz
	sp16 = (uint16*)(GFX.Screen);
	sp16 += ( i * 320 );
	for (register uint32 j = 0; j < Width + 32/*256*/; j++, sp16++) {
	  *dp16++ = *sp16;
	  *dp16++ = *sp16;
	}
      } // scanline

    } // for each height unit

  } else if ( g_scale == bs_1to32_multiplied ) {

    unsigned int effective_height = Height * 2;
    for (register uint32 i = 0; i < effective_height; i++) {

      // first scanline of doubled pair
      register uint16 *dp16 = (uint16*)(screen -> pixels);
      dp16 += ( i * ( screen -> pitch / 2 ) ); // pitch is in 1b increments, so is 2* what you think!

      dp16 += 20; // center-ish X :)
      dp16 += ( 5 * screen -> pitch ); // center-ish Y

      register uint16 *sp16 = (uint16*)(GFX.Screen);
      sp16 += ( ( i / 2 ) * 320 );

      for (register uint32 j = 0; j < Width + 32/*256*/; j++, sp16++) {
	*dp16++ = *sp16;
	*dp16++ = *sp16; // doubled
	*dp16++ = *sp16; // tripled
      }

    } // for each height unit

  } else {

    // code error; unknown scaler
    fprintf ( stderr, "invalid scaler option handed to render code; fix me!\n" );
    exit ( 0 );

  } // scaled?

  if (Settings.DisplayFrameRate) {
    S9xDisplayFrameRate ((uint8 *)screen->pixels + 64, 800 * 2 * 2 );
  }

  if (GFX.InfoString) {
    S9xDisplayString (GFX.InfoString, (uint8 *)screen->pixels + 64, 800 * 2 * 2, 460 );
  }

  // SDL_UnlockSurface(screen);

  // vsync
  if ( g_vsync && g_fb >= 0 ) {
    int arg = 0;
    ioctl ( g_fb, FBIO_WAITFORVSYNC, &arg );
  }

  // update the actual screen
  SDL_UpdateRect(screen,0,0,0,0);

  return(TRUE);
}
#else
bool8_32 S9xDeinitUpdate (int Width, int Height)
{
	register uint32 lp = (xs > 256) ? 16 : 0;

	if (Width > 256)
		lp *= 2;

	// ffc, what is it for?
//	if (ffc < 5)
//	{
//		SDL_UpdateRect(screen,0,0,0,0);
//		++ffc;
//	}
//	else
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
		//put here also scaling for non highres
		//start stretch
		int yoffset = 8*(Height == 224);
		if(Scale)
		{
			int x,y,s;
			uint32 x_error;
			static uint32 x_fraction=52428;
		 	char temp[512];
			register uint8 *d;
			//x_fraction = (SNES_WIDTH * 0x10000) / 320;		
			//x_fraction = 52428;		
		
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
		}
		//end stretch

		if (GFX.InfoString)
		    S9xDisplayString (GFX.InfoString, (uint8 *)screen->pixels + 64, 640,0);
		else if (Settings.DisplayFrameRate)
		    S9xDisplayFrameRate ((uint8 *)screen->pixels + 64, 640);

	    SDL_UpdateRect(screen,0,yoffset,320,Height+yoffset);
	}

//Bei Fullscreen kann man alles blitten	-- HighRes Sachn müssen aber angepasst werden!
//	SDL_BlitSurface(gfxscreen,NULL,screen,NULL);
//	SDL_Flip(screen);
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

    char *slash = strrchr (path, '/');
    if (!slash)
	slash = strrchr (path, '\\');

    char *dot = strrchr (path, '.');

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
					S9xSetSoundMute(true);
					menu_loop();
					S9xSetSoundMute(false);
					//S9xSetMasterVolume(vol,vol);
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
				if ( event.key.keysym.sym == SDLK_q )
				{
					S9xSetSoundMute(true);
					exit ( 0 ); // just die
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
					//S9xSetMasterVolume (vol, vol);
#ifdef PANDORA
				}
				// another shortcut I'm afraid
				else if (event.key.keysym.sym == SDLK_SPACE)
				{
						S9xSetSoundMute(true);
						menu_loop();
						S9xSetSoundMute(false);
						//S9xSetMasterVolume (vol, vol);
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

#ifndef _ZAURUS
static long power (int num, int pow)
{
    long val = num;
    int i;
    
    if (pow == 0)
	return (1);

    for (i = 1; i < pow; i++)
	val *= num;

    return (val);
}
#endif

static int Rates[8] =
{
    0, 8192, 11025, 16000, 22050, 29300, 36600, 44000
};

static int BufferSizes [8] =
{
    0, 256, 256, 256, 512, 512, 1024, 1024
};

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
	so.buffer_size = buffer_size = BufferSizes [mode & 7];
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

//    gp2x_sound_volume(vol,vol);
#endif
    return (TRUE);
}

void S9xUnixProcessSound (void)
{
}

static uint8 Buf[MAX_BUFFER_SIZE] __attribute__((aligned(4)));

#define FIXED_POINT 0x10000
#define FIXED_POINT_SHIFT 16
#define FIXED_POINT_REMAINDER 0xffff

static volatile bool8 block_signal = FALSE;
static volatile bool8 block_generate_sound = FALSE;
static volatile bool8 pending_signal = FALSE;

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
	    S9xMixSamplesO (Buf, sc,
			    byte_offset & SOUND_BUFFER_SIZE_MASK);
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
/*    audio_buf_info info;

    if (!Settings.ThreadSound &&
	(ioctl (so.sound_fd, SNDCTL_DSP_GETOSPACE, &info) == -1 ||
	 info.bytes < so.buffer_size))
    {
	return (NULL);
    }
*/
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

//printf ("%d:", sample_count - so.samples_mixed_so_far); fflush (stdout);
	if (Settings.SoundSync == 2)
	{
	    memset (Buf + (byte_offset & SOUND_BUFFER_SIZE_MASK), 0,
		    sample_count - so.samples_mixed_so_far);
	}
	else
	    S9xMixSamplesO (Buf, sample_count - so.samples_mixed_so_far,
			    byte_offset & SOUND_BUFFER_SIZE_MASK);
	so.samples_mixed_so_far = 0;
    }
    else
	so.samples_mixed_so_far -= sample_count;
    
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

/*
void gp2x_sound_volume(int l, int r)
{
 	l=l<0?0:l; l=l>255?255:l; r=r<0?0:r; r=r>255?255:r;
 	l<<=8; l|=r;
  	ioctl(mixerdev, SOUND_MIXER_WRITE_VOLUME, &l);
}
*/

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
