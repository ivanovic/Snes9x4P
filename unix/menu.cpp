#include <SDL/SDL.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <strings.h>
#include "keydef.h"
#include "dingoo.h"

#include "snes9x.h"
#include "memmap.h"
#include "cpuexec.h"
#include "snapshot.h"
#include "display.h"
#include "gfx.h"
#include "unistd.h"

#include "unix/extra_defines.h"
#include "unix/menu.h"

#ifdef PANDORA
	#include <iostream>
	#include "pandora_scaling/blitscale.h"
	extern blit_scaler_option_t blit_scalers[];
	extern blit_scaler_e g_scale;
	extern unsigned char g_vsync;
	extern SDL_Surface *screen,*gfxscreen;
	extern int cut_top, cut_bottom, cut_left, cut_right;
#endif

extern Uint16 sfc_key[256];
extern bool8_32 Scale;
extern int SaveSlotNum;
extern short vol;

extern void S9xDisplayString (const char *string, uint8 *, uint32, int ypos);
void save_screenshot(char *fname);
void load_screenshot(char *fname);
void show_screenshot(void);
void capt_screenshot(void);
void menu_dispupdate(void);
void ShowCredit(void);

int cursor = 6; //set cursor start to "save state"
int loadcursor = 0;
int romcount_maxrows = 16;

int battery_level;

char SaveSlotNum_old=255;
bool8_32 Scale_org=Scale;
bool8_32 highres_current = false;
char snapscreen[17120]={};

char temp[256];
char disptxt[23][256];

void sys_sleep(int us)
{
	if(us>0)
		SDL_Delay(us/100);
}

struct dirent **namelist;

int isFile(const struct dirent *nombre) {
 int isFile = 0;
 char *extension = rindex( (char*) nombre->d_name, '.');
 if (strcmp(extension, ".sfc") == 0 ||
 	 strcmp(extension, ".smc") == 0 ||
 	 strcmp(extension, ".zip" ) == 0 )
 {
  isFile = 1;
 }

 return isFile;
}

int FileDir(char *dir, const char *ext)
{
	int n;

	//printf ("Try to create ./roms directory..");
	mkdir (dir, 0777);
	chown (dir, getuid (), getgid ());

	n = scandir (dir, &namelist, isFile, alphasort);
	if (n >= 0)
	{
//		int cnt;
//		for (cnt = 0; cnt < n; ++cnt)
//			puts (namelist[cnt]->d_name);
	}
	else
	{
		perror ("Couldn't open ./roms directory..try to create this directory");

		mkdir (dir, 0777);
		chown (dir, getuid (), getgid ());
	
		n = scandir (dir, &namelist, isFile, alphasort);
	}
		
	return n;
}

void loadmenu_dispupdate(int romcount)
{
	//draw blue screen
	for(int y=0;y<240;y++){
		for(int x=0;x<256*2;x+=2){
			memset(GFX.Screen + 320*y*2+x,0x11,2);
		}
	}


	strcpy(disptxt[0],"  Snes9x4P v20120226");

	//copy roms filenames to disp[] cache
	for(int i=0;i<=romcount_maxrows;i++)
	{
		if (loadcursor>romcount_maxrows)
		{
			if((i+(loadcursor-romcount_maxrows))==loadcursor)
				sprintf(temp," >%s",namelist[ i+(loadcursor-romcount_maxrows) ]->d_name);
			else
				sprintf(temp,"  %s",namelist[ i+(loadcursor-romcount_maxrows) ]->d_name);

			strncpy(disptxt[i+2],temp,34);
			disptxt[i+2][34]='\0';
		}
		else
		if (i<romcount)
		{
			if(i==loadcursor)
				sprintf(temp," >%s",namelist[i]->d_name);
			else
				sprintf(temp,"  %s",namelist[i]->d_name);

			strncpy(disptxt[i+2],temp,34);
			disptxt[i+2][34]='\0';
		}
	}

	//draw 20 lines on screen
	for(int i=0;i<19;i++)
	{
		S9xDisplayString (disptxt[i], GFX.Screen, 640,i*10+64);
	}

	//update screen
	S9xDeinitUpdate (320, 240);
}

char* menu_romselector()
{
	char *rom_filename = NULL;
	int romcount = 0;

	bool8_32 exit_loop = false;

	uint8 *keyssnes = 0;


	//Read ROM-Directory
	romcount = FileDir("./roms", "sfc,smc");

	Scale_org = Scale;
	highres_current=Settings.SupportHiRes;

	Scale = false;
	Settings.SupportHiRes=FALSE;
	S9xDeinitDisplay();
	S9xInitDisplay(0, 0);
	
	loadmenu_dispupdate(romcount);
	sys_sleep(100000);

	SDL_Event event;

	do
	{
		loadmenu_dispupdate(romcount);
		sys_sleep(100);

		while(SDL_PollEvent(&event)==1)
		{
			//PANDORA & DINGOO & WIN32 -----------------------------------------------------
			keyssnes = SDL_GetKeyState(NULL);
			switch(event.type)
			{
				case SDL_KEYDOWN:
					keyssnes = SDL_GetKeyState(NULL);

					//UP
					if(keyssnes[sfc_key[UP_1]] == SDL_PRESSED)
						loadcursor--;
					//DOWN
					else if(keyssnes[sfc_key[DOWN_1]] == SDL_PRESSED)
						loadcursor++;
//					//LS
//					else if(keyssnes[sfc_key[L_1]] == SDL_PRESSED)
//						loadcursor=loadcursor-10;
//					//RS
//					else if(keyssnes[sfc_key[R_1]] == SDL_PRESSED)
//						loadcursor=loadcursor+10;
					//QUIT Emulator : press ESCAPE KEY
					else if (keyssnes[sfc_key[SELECT_1]] == SDL_PRESSED)
						S9xExit();
					else if( (keyssnes[sfc_key[B_1]] == SDL_PRESSED) )
					{
						switch(loadcursor)
						{
							default:
								if ((keyssnes[sfc_key[B_1]] == SDL_PRESSED))
								{
									if ((loadcursor>=0) && (loadcursor<(romcount)))
									{
										rom_filename=namelist[loadcursor]->d_name;
										exit_loop = TRUE;
									}
								}
								break;
						}
					}
					break;
			}

			if(loadcursor==-1)
			{
				loadcursor=romcount-1;
			}
			else
			if(loadcursor==romcount)
			{
				loadcursor=0;
			}

			break;
		}
	}
	while( exit_loop!=TRUE && keyssnes[sfc_key[B_1]] != SDL_PRESSED );

	// TODO:
	///free(). 	namelist

	Scale = Scale_org;
	Settings.SupportHiRes=highres_current;
	S9xDeinitDisplay();
	S9xInitDisplay(0, 0);

	return (rom_filename);
}

//------------------------------------------------------------------------------------------

void menu_dispupdate(void)
{
//	char temp[256];
//	char disptxt[20][256];

	//memset(GFX.Screen + 320*12*2,0x11,320*200*2);
	//draw blue screen
	for(int y=0;y<240;y++){
		for(int x=0;x<256*2;x+=2){
			memset(GFX.Screen + 320*y*2+x,0x11,2);
		}
	}
	

	strcpy(disptxt[0],"Snes9x4P v20120226");

	strcpy(disptxt[1],"------------------");
	strcpy(disptxt[2],"Exit Emulator");
	strcpy(disptxt[3],"Reset Game");
	strcpy(disptxt[4],"Credits");
	strcpy(disptxt[5],"------------------");
	strcpy(disptxt[6],"Save State");
	strcpy(disptxt[7],"Load State");
	strcpy(disptxt[8],"State Slot                  No.");
	sprintf(temp,"%s%d",disptxt[8],SaveSlotNum);
	strcpy(disptxt[8],temp);
	strcpy(disptxt[9],"------------------");
	
	strcpy(disptxt[10],"Display mode     ");
	sprintf ( temp, "%s%s", disptxt [10], blit_scalers [ g_scale ].desc_en );
	strcpy ( disptxt[10], temp );
	
	strcpy(disptxt[11],"Frameskip                  ");
	if (Settings.SkipFrames == AUTO_FRAMERATE)
		sprintf(temp,"%s Auto",disptxt[11]);
	else
		sprintf(temp,"%s %02d/%d",disptxt[11],(int) Memory.ROMFramesPerSecond, Settings.SkipFrames);
	strcpy(disptxt[11],temp);
	
	strcpy(disptxt[12],"V-Sync                     ");
	if(g_vsync)
		sprintf(temp,"%s   On",disptxt[12]);
	else
		sprintf(temp,"%s  Off",disptxt[12]);
	strcpy(disptxt[12],temp);
	
	strcpy(disptxt[13],"Display Frame Rate         ");
	if(Settings.DisplayFrameRate)
		sprintf(temp,"%s   On",disptxt[13]);
	else
		sprintf(temp,"%s  Off",disptxt[13]);
	strcpy(disptxt[13],temp);
	
	strcpy(disptxt[14],"Transparency               ");
	if(Settings.Transparency)
		sprintf(temp,"%s   On",disptxt[14]);
	else
		sprintf(temp,"%s  Off",disptxt[14]);
	strcpy(disptxt[14],temp);
	
	strcpy(disptxt[15],"Cut Top                        ");
	sprintf(temp,"%s%d",disptxt[15],cut_top);
	strcpy(disptxt[15],temp);
	strcpy(disptxt[16],"Cut Bottom                     ");
	sprintf(temp,"%s%d",disptxt[16],cut_bottom);
	strcpy(disptxt[16],temp);
	strcpy(disptxt[17],"Cut Left                       ");
	sprintf(temp,"%s%d",disptxt[17],cut_left);
	strcpy(disptxt[17],temp);
	strcpy(disptxt[18],"Cut Right                      ");
	sprintf(temp,"%s%d",disptxt[18],cut_right);
	strcpy(disptxt[18],temp);
	
	strcpy(disptxt[19],"------------------");
	
	strcpy(disptxt[20],"Alt. Sample Decoding       ");
	if(Settings.AltSampleDecode)
		sprintf(temp,"%s   On",disptxt[20]);
	else
		sprintf(temp,"%s  Off",disptxt[20]);
	strcpy(disptxt[20],temp);
	
	strcpy(disptxt[21],"");
	battery_level = batt_level();
	strcpy(disptxt[22],"                   Battery:");
	if (battery_level >= 100)
		sprintf(temp,"%s %d%%",disptxt[22],battery_level);
	else if (battery_level >=10)
		sprintf(temp,"%s  %d%%",disptxt[22],battery_level);
	else
		sprintf(temp,"%s   %d%%",disptxt[22],battery_level);
	strcpy(disptxt[22],temp);
	


	for(int i=0;i<=22;i++)
	{
		if(i==cursor)
			sprintf(temp," >%s",disptxt[i]);
		else
			sprintf(temp,"  %s",disptxt[i]);
		strcpy(disptxt[i],temp);

		if ( i<=21 )
			S9xDisplayString (disptxt[i], GFX.Screen, 640,i*10+50);
		else //put the last line (for the battery indicator) a little lower than the rest
			S9xDisplayString (disptxt[i], GFX.Screen, 640,i*10+55);
	}

	//show screen shot for snapshot
	if(SaveSlotNum_old != SaveSlotNum)
	{
		strcpy(temp," Loading...");
		S9xDisplayString (temp, GFX.Screen +320/*280*/, 640,80/*204*/);
		//S9xDeinitUpdate (320, 240);
		char fname[256], ext[8];
		sprintf(ext, ".s0%d", SaveSlotNum);
		strcpy(fname, S9xGetFilename (ext));
		load_screenshot(fname);
		SaveSlotNum_old = SaveSlotNum;
	}
	show_screenshot();
	S9xDeinitUpdate (320, 240);
}

void menu_loop(void)
{
	bool8_32 exit_loop = false;
	char fname[256], ext[8];
	char snapscreen_tmp[17120];

	uint8 *keyssnes = 0;

	SaveSlotNum_old = -1;

	Scale_org = Scale;
	highres_current=Settings.SupportHiRes;

	capt_screenshot();
	memcpy(snapscreen_tmp,snapscreen,17120);

	Scale = false;
	Settings.SupportHiRes=FALSE;
	S9xDeinitDisplay();
	S9xInitDisplay(0, 0);

	menu_dispupdate();
	sys_sleep(100000);

	SDL_Event event;

	do
	{
		while(SDL_PollEvent(&event)==1)
		{

				//PANDORA & DINGOO & WIN32 -----------------------------------------------------
				keyssnes = SDL_GetKeyState(NULL);

				if(keyssnes[sfc_key[UP_1]] == SDL_PRESSED)
					cursor--;
				else if(keyssnes[sfc_key[DOWN_1]] == SDL_PRESSED)
					cursor++;
				else if( (keyssnes[sfc_key[A_1]] == SDL_PRESSED) ||
						 (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED) ||
						 (keyssnes[sfc_key[RIGHT_1]] == SDL_PRESSED) )
				{
					switch(cursor)
					{
						case 2: //exit snes9x
							if (keyssnes[sfc_key[A_1]] == SDL_PRESSED)
								S9xExit();
							break;
						case 3: //reset snes9x
							if ((keyssnes[sfc_key[A_1]] == SDL_PRESSED))
							{
								//make sure the sram is stored before resetting the console
								//it should work without, but better safe than sorry...
								Memory.SaveSRAM (S9xGetFilename (".srm"));
								S9xReset();
								exit_loop = TRUE;
							}
							break;
						case 4:
							if (keyssnes[sfc_key[A_1]] == SDL_PRESSED)
								ShowCredit();
							break;
						case 6: //save state
							if (keyssnes[sfc_key[A_1]] == SDL_PRESSED)
							{
								memcpy(snapscreen,snapscreen_tmp,16050);
								show_screenshot();
								strcpy(fname," Saving...");
								S9xDisplayString (temp, GFX.Screen +320/*280*/, 640,80/*204*/);
								S9xDeinitUpdate (320, 240);
								sprintf(ext, ".s0%d", SaveSlotNum);
								strcpy(fname, S9xGetFilename (ext));
								save_screenshot(fname);
								sprintf(ext, ".00%d", SaveSlotNum);
								strcpy(fname, S9xGetFilename (ext));
								S9xFreezeGame (fname);
								sync();
								exit_loop = TRUE;
							}
							break;
						case 7: //load state
							if (keyssnes[sfc_key[A_1]] == SDL_PRESSED)
							{
								sprintf(ext, ".00%d", SaveSlotNum);
								strcpy(fname, S9xGetFilename (ext));
								S9xLoadSnapshot (fname);
								exit_loop = TRUE;
							}
							break;
						case 8: //select save state slot
							if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED)
							{
								if ( SaveSlotNum == 0 )
									SaveSlotNum = MAX_SAVE_SLOTS-1; // slots start at 0, so 10 slots means slot 0 to 9
								else
									--SaveSlotNum;
							}
							else
							if (keyssnes[sfc_key[RIGHT_1]] == SDL_PRESSED)
							{
								if ( SaveSlotNum == MAX_SAVE_SLOTS-1 ) // slots start at 0, so 10 slots means slot 0 to 9
									SaveSlotNum = 0;
								else
									++SaveSlotNum;
							}
							break;
						case 10: // rotate through scalers
							if (keyssnes[sfc_key[RIGHT_1]] == SDL_PRESSED)
							{
								do
								{
									g_scale = (blit_scaler_e) ( ( g_scale + 1 ) % bs_max );
								} while ( ( blit_scalers [ g_scale ].valid == bs_invalid )
											|| ( highres_current && !(blit_scalers [ g_scale ].support_hires) ) );
							} else if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED)
							{
								do
								{
									g_scale = (blit_scaler_e) ( g_scale - 1 );
									if (g_scale < 1) g_scale = (blit_scaler_e)(bs_max-1);
								} while ( ( blit_scalers [ g_scale ].valid == bs_invalid )
											|| ( highres_current && !(blit_scalers [ g_scale ].support_hires) ) );
							}
							// now force update the display, so that the new scaler is directly used (fixes some glitches)
							S9xDeinitDisplay();
							S9xInitDisplay(0, 0);
							break;
						case 11: // set frameskip
							if (Settings.SkipFrames == AUTO_FRAMERATE)
								Settings.SkipFrames = 10;
	
							if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED)
								Settings.SkipFrames--;
							else
								Settings.SkipFrames++;
	
							if(Settings.SkipFrames<=0 || Settings.SkipFrames==10)
								Settings.SkipFrames = AUTO_FRAMERATE;
							else if (Settings.SkipFrames>=11)
								Settings.SkipFrames = 1;
							break;
						case 12: // set vsync
							if (g_vsync)
								g_vsync = 0;
							else 
								g_vsync = 1;
							break;
						case 13: // set display fps
							Settings.DisplayFrameRate = !Settings.DisplayFrameRate;
							break;
						case 14: // set transparency
							Settings.Transparency = !Settings.Transparency;
							break;
						case 15: // cut lines from top
							if (keyssnes[sfc_key[RIGHT_1]] == SDL_PRESSED)
								cut_top++;
							else if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED && cut_top>0)
								cut_top--;
							// now force update the display, so that the new scaler is directly used (fixes some glitches)
							S9xDeinitDisplay();
							S9xInitDisplay(0, 0);
							break;
						case 16: // cut lines from bottom
							if (keyssnes[sfc_key[RIGHT_1]] == SDL_PRESSED)
								cut_bottom++;
							else if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED && cut_bottom>0)
								cut_bottom--;
							S9xDeinitDisplay();
							S9xInitDisplay(0, 0);
							break;
						case 17: // cut from the left
							if (keyssnes[sfc_key[RIGHT_1]] == SDL_PRESSED)
								cut_left++;
							else if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED && cut_left>0)
								cut_left--;
							S9xDeinitDisplay();
							S9xInitDisplay(0, 0);
							break;
						case 18: // cut from the right
							if (keyssnes[sfc_key[RIGHT_1]] == SDL_PRESSED)
								cut_right++;
							else if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED && cut_right>0)
								cut_right--;
							S9xDeinitDisplay();
							S9xInitDisplay(0, 0);
							break;
						case 20:
//offer an option to change to alternative sample decoding
//cf. http://www.gp32x.com/board/index.php?/topic/55378-snes9x4d4p-another-new-build-now-with-hi-res-and-new-rom-picker/page__view__findpost__p__958860
							if (Settings.AltSampleDecode)
								Settings.AltSampleDecode = 0;
							else 
								Settings.AltSampleDecode = 1;
							break;
					}
				}

				if(cursor==1)
					cursor=20;
				else if(cursor==21)	
					cursor=2;
				if(cursor==5 || cursor==9 || cursor==19) {
					if(keyssnes[sfc_key[UP_1]] == SDL_PRESSED)
						cursor--;
					else if(keyssnes[sfc_key[DOWN_1]] == SDL_PRESSED)
						cursor++;
				}

				menu_dispupdate();
				sys_sleep(1000);

				break;
		}
	}
	while( exit_loop!=TRUE && keyssnes[sfc_key[B_1]] != SDL_PRESSED );

	Scale = Scale_org;
	Settings.SupportHiRes=highres_current;
	S9xDeinitDisplay();
	S9xInitDisplay(0, 0);
}

void save_screenshot(char *fname){
	FILE  *fs = fopen (fname,"wb");
	if(fs==NULL)
		return;
	
	fwrite(snapscreen,17120,1,fs);
	fclose (fs);
	sync();
}

void load_screenshot(char *fname)
{
	FILE *fs = fopen (fname,"rb");
	if(fs==NULL){
		for(int i=0;i<17120;i++){
			snapscreen[i] = 0;
		}
		return;
	}
	fread(snapscreen,17120,1,fs);
	fclose(fs);
}

void capt_screenshot() //107px*80px
{
	bool8_32 Scale_disp=Scale;
	int s = 0;
	int yoffset = 0;
	struct InternalPPU *ippu = &IPPU;

	for(int i=0;i<17120;i++)
	{
		snapscreen[i] = 0x00;
	}

	if(ippu->RenderedScreenHeight == 224)
		yoffset = 8;

	if (highres_current==true)
	{
		//working but in highres mode
		for(int y=yoffset;y<240-yoffset;y+=3) //80,1 //240,3
		{
			s+=22*1/*(Scale_disp!=TRUE)*/;
			for(int x=0;x<640-128*1/*(Scale_disp!=TRUE)*/;x+=6) //107,1 //214,2 //428,4 +42+42
			{
				uint8 *d = GFX.Screen + y*1024 + x; //1024
				snapscreen[s++] = *d++;
				snapscreen[s++] = *d++;
			}
			s+=20*1/*(Scale_disp!=TRUE)*/;
		}
	}
	else
	{
		//original
		for(int y=yoffset;y<240-yoffset;y+=3) // 240/3=80
		{
			s+=22*(Scale_disp!=TRUE);
			for(int x=0 ;x<320*2-128*(Scale_disp!=TRUE);x+=3*2) // 640/6=107
			{
				uint8 *d = GFX.Screen + y*320 *2 + x;
				snapscreen[s++] = *d++;
				snapscreen[s++] = *d++;
			}
			s+=20*(Scale_disp!=TRUE);
		}
	}
}

void show_screenshot()
{
	int s=0;
//	for(int y=126;y<126+80;y++){
	for(int y=2;y<2+80;y++){
		for(int x=294; x<294+107*2; x+=2){
			uint8 *d = GFX.Screen + y*320*2 + x;
			*d++ = snapscreen[s++];
			*d++ = snapscreen[s++];
		}
	}
}

int batt_level(void)
{
	char temp[LINE_MAX];
	FILE *f;
	int percentage;
	/* capacity */
	f = fopen("/sys/class/power_supply/bq27500-0/capacity", "r");
	if(f)
	{
		//fgets( temp, LINE_MAX, f );
		fscanf( f, "%d", &percentage);
		fclose(f);
		//percentage = atoi(temp);
	}
	else
		percentage = 0;
	
	return percentage;
}

void ShowCredit()
{
	uint8 *keyssnes = 0;
	int line=0,ypix=0,maxlines=26;
	char disptxt[100][256]={
	"",
	"",
	" Thank you for using this Emulator! ",
	"",
	"",
	" Created by the Snes9x team.",
	"",
	" Port to libSDL by SiENcE",
	" crankgaming.blogspot.com",
	"",
	" regards to joyrider & g17",
	"",
	"",
	" Pandora port started by:",
	" skeezix",
	"",
	" Further work and maintenance:",
	" Ivanovic",
	"",
	" Special thanks to:",
	" EvilDragon (picklelauncher theme)",
	" john4p (cutting borders)",
	" Notaz (modified libSDL for Pandora)",
	" pickle (picklelauncher, Scale2x)",
	" WizardStan (smooth scaler)",
	"",
	};

	do
	{
		SDL_Event event;
		SDL_PollEvent(&event);

		keyssnes = SDL_GetKeyState(NULL);

		//draw blue screen
		for(int y=0;y<240;y++){
			for(int x=0;x<256*2;x+=2){
				memset(GFX.Screen + 320*y*2+x,0x11,2);
			}
		}
		
		for(int i=0;i<=16;i++){
			int j=i+line;
			if(j>=maxlines) j-=maxlines;
			S9xDisplayString (disptxt[j], GFX.Screen, 640,i*10+80-ypix);
		}
		
		ypix+=2;
		if(ypix==12) {
			line++;
			ypix=0;
		}
		if(line == maxlines) line = 0;
		S9xDeinitUpdate (320, 240);
		sys_sleep(3000);
	}
	while(keyssnes[sfc_key[B_1]] != SDL_PRESSED);

	return;
}

