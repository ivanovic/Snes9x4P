#include <SDL/SDL.h>
#include <time.h>
#include "keydef.h"
#include "dingoo.h"

#include "snes9x.h"
#include "memmap.h"
#include "cpuexec.h"
#include "snapshot.h"
#include "display.h"
#include "gfx.h"

extern Uint16 sfc_key[256];
extern bool8_32 Scale;
extern char SaveSlotNum;
extern unsigned CLK_FREQ;
extern int vol;

extern void S9xDisplayString (const char *string, uint8 *, uint32, int ypos);
extern void set_FCLK(unsigned);
void save_screenshot(char *fname);
void load_screenshot(char *fname);
void show_screenshot(void);
void capt_screenshot(void);
int batt_level(void);
int chk_hold(void);
void set_lcd_backlight(int backlight);
int get_lcd_backlight(void);
void ShowCredit(void);

int cursor = 2;
char SaveSlotNum_old=255;
bool8_32 Scale_org=Scale;
bool8_32 highres_current = false;
char snapscreen[17120]={};
extern clock_t start;

void menu_dispupdate(void){
	char temp[256];
	char disptxt[20][256];

	//memset(GFX.Screen + 320*12*2,0x11,320*200*2);
	for(int y=12;y<=212;y++){
		for(int x=10;x<246*2;x+=2){
			memset(GFX.Screen + 320*y*2+x,0x11,2);
		}	
	}

	strcpy(disptxt[0],"Snes9x for DINGUX (ver.20100315)");
	strcpy(disptxt[1],"");
	//strcpy(disptxt[2],"Resume Game          ");
	strcpy(disptxt[2],"Reset Game           ");
	strcpy(disptxt[3],"Save State           ");
	strcpy(disptxt[4],"Load State           ");
	strcpy(disptxt[5],"State Slot              No.");
	strcpy(disptxt[6],"Display Frame Rate     ");
	strcpy(disptxt[7],"Full Screen         ");
	strcpy(disptxt[8],"Frameskip              ");	//	strcpy(disptxt[8],"Clock Speed          ");
	strcpy(disptxt[9],"Sound Volume           ");
	strcpy(disptxt[10],"Credit              ");
	strcpy(disptxt[11],"Exit");
	strcpy(disptxt[12],"--------------");
	strcpy(disptxt[13],"BATT:");
	strcpy(disptxt[14],"     ");
	strcpy(disptxt[15],"PLAYTIME :");	
	strcpy(disptxt[16],"BACKLIGHT:");	

	sprintf(temp,"%s%d",disptxt[5],SaveSlotNum);
	strcpy(disptxt[5],temp);

	if(Settings.DisplayFrameRate)
		sprintf(temp,"%s True",disptxt[6]);
	else
		sprintf(temp,"%sFalse",disptxt[6]);
	strcpy(disptxt[6],temp);

	if (highres_current==false)
	{
		if(Scale_org)
			sprintf(temp,"%s    True",disptxt[7]);
		else
			sprintf(temp,"%s   False",disptxt[7]);
		strcpy(disptxt[7],temp);
	}
	else
	{
		sprintf(temp,"%sinactive",disptxt[7]);
		strcpy(disptxt[7],temp);
	}

	if (Settings.SkipFrames == AUTO_FRAMERATE)
	{
		sprintf(temp,"%s Auto",disptxt[8]);
		strcpy(disptxt[8],temp);
	}
	else
	{
		sprintf(temp,"%s %02d/%d",disptxt[8],(int) Memory.ROMFramesPerSecond, Settings.SkipFrames);
		strcpy(disptxt[8],temp);
	}

	sprintf(temp,"%s %3d%%",disptxt[9],vol);
	strcpy(disptxt[9],temp);
/*
	sprintf(temp,"%s %d",disptxt[13],batt_level());
	strcpy(disptxt[13],temp);
*/
	if      (batt_level() >= 3739)
		sprintf(temp,"%s  (#####)",disptxt[13]);
	else if (batt_level() >= 3707)
		sprintf(temp,"%s  ( ####)",disptxt[13]);
	else if (batt_level() >= 3675)
		sprintf(temp,"%s  (  ###)",disptxt[13]);
    	else if (batt_level() >= 3643)
		sprintf(temp,"%s  (   ##)",disptxt[13]);
	else if (batt_level() >= 3611)
		sprintf(temp,"%s  (    #)",disptxt[13]);
   	else   sprintf(temp,"%s  (     )",disptxt[13]);
	strcpy(disptxt[13],temp);

	sprintf(temp,"%s  (%5d)",disptxt[14],(batt_level()/10)*10);
	strcpy(disptxt[14],temp);	
/*
	sprintf(temp,"%s%3dm",disptxt[15],(int)((clock()-start)/(CLOCKS_PER_SEC*60)));
	strcpy(disptxt[15],temp);		

	sprintf(temp,"%s %d%%",disptxt[16],get_lcd_backlight());
	strcpy(disptxt[16],temp);	
*/
	for(int i=0;i<=14;i++){
		if(i==cursor)
			sprintf(temp," >%s",disptxt[i]);
		else
			sprintf(temp,"  %s",disptxt[i]);
		strcpy(disptxt[i],temp);

		S9xDisplayString (disptxt[i], GFX.Screen, 640,i*10+64);		
	}


//show screen shot for snapshot
	if(SaveSlotNum_old != SaveSlotNum){
		strcpy(temp,"Loading...");
		S9xDisplayString (temp, GFX.Screen +280, 640,204);
		S9xDeinitUpdate (320, 240);
		char fname[256], ext[8];
		sprintf(ext, ".s0%d", SaveSlotNum);
		strcpy(fname, S9xGetFilename (ext));
		load_screenshot(fname);
		SaveSlotNum_old = SaveSlotNum;
	}
	show_screenshot();
	
	S9xDeinitUpdate (320, 240);
}

void menu_loop(void){
	uint8 *keyssnes=0;
	bool8_32 exit_loop = false;
	char fname[256], ext[8];
	char snapscreen_tmp[17120];
	//int backlight = get_lcd_backlight();
	//int hold_state = 0;	
	//int backlight_to_sleep = backlight;

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
	usleep(100000);

	SDL_Event event;

	do {
		menu_dispupdate();
		usleep(100);
		keyssnes = SDL_GetKeyState(NULL);
		while(SDL_PollEvent(&event)==1) {
			switch(event.type) {
			case SDL_KEYDOWN:
				keyssnes = SDL_GetKeyState(NULL);
				if(keyssnes[sfc_key[UP_1]] == SDL_PRESSED)
					cursor--;
				else if(keyssnes[sfc_key[DOWN_1]] == SDL_PRESSED)
					cursor++;
				else if((keyssnes[sfc_key[A_1]] == SDL_PRESSED)||
					(keyssnes[sfc_key[RIGHT_1]] == SDL_PRESSED)||
					(keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED)){
					switch(cursor){
					case 2:
						if ((keyssnes[sfc_key[A_1]] == SDL_PRESSED)){
							S9xReset();
							exit_loop = TRUE;
						}
					break;
					case 3:
						if (keyssnes[sfc_key[A_1]] == SDL_PRESSED){
							memcpy(snapscreen,snapscreen_tmp,16050);
							show_screenshot();
							strcpy(fname," Saving...");
							S9xDisplayString (fname, GFX.Screen +280, 640,204);
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
					case 4:
						if (keyssnes[sfc_key[A_1]] == SDL_PRESSED){
							sprintf(ext, ".00%d", SaveSlotNum);
							strcpy(fname, S9xGetFilename (ext));
							S9xLoadSnapshot (fname);
							exit_loop = TRUE;
						}
					break;
					case 5:
						if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED)
							SaveSlotNum--;
						else SaveSlotNum++;
						if(SaveSlotNum>3)
							SaveSlotNum =0;
						else if(SaveSlotNum<0)
							SaveSlotNum=3;
					break;
					case 6:
						Settings.DisplayFrameRate = !Settings.DisplayFrameRate;
					break;
					case 7:
						Scale_org = !Scale_org;
					break;
					case 8:
/*
						if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED)
							CLK_FREQ -=8;
						else CLK_FREQ +=8;
						if(CLK_FREQ>=432)
							CLK_FREQ=432;
						else if (CLK_FREQ<=200)
							CLK_FREQ=200;
*/
						if (Settings.SkipFrames == AUTO_FRAMERATE)
							Settings.SkipFrames = 10;

						if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED)
							Settings.SkipFrames--;
						else
							Settings.SkipFrames++;

						if(Settings.SkipFrames>=10)
							Settings.SkipFrames = AUTO_FRAMERATE;
						else if (Settings.SkipFrames <=1)
							Settings.SkipFrames = 1;
					break;
					case 9:
						if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED)
							vol -= 10;
						else	vol += 10;
						if(vol>=100)
							vol = 100;
						else if (vol <=0)
							vol = 0;
					break;
					case 10:
						ShowCredit();
					break;
					case 11:
						if (keyssnes[sfc_key[A_1]] == SDL_PRESSED)
							S9xExit();
					break;
					}
				}

				if(cursor==1) cursor=11;
				else if(cursor==12) cursor=2;
			break;
			}
		}
	} while(exit_loop!=TRUE && keyssnes[sfc_key[B_1]] != SDL_PRESSED);

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

//	if(ippu->RenderedScreenHeight == 224)
//		yoffset = 8;

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

void show_screenshot(){
	int s=0;
	for(int y=126;y<126+80;y++){
		for(int x=248; x<248+107*2; x+=2){
			uint8 *d = GFX.Screen + y*320*2 + x;
			*d++ = snapscreen[s++];
			*d++ = snapscreen[s++];
		}
	}
}

int batt_level(void){
	FILE *FP;
	int mvolts;
	char buf[6]={};
	FP = fopen("/proc/jz/battery", "rb");
	if (!FP) return(0);
	fgets(buf, 5, FP);
	fclose(FP);
 	sscanf(buf, "%d", &mvolts);
	return (mvolts);
}

int chk_hold(void){
	FILE *FP;
	uint32 hold=0;
	char buf[12]={};
	FP = fopen("/proc/jz/gpio3_pxpin", "rb");
	if (!FP) return(0);
	fgets(buf, 11, FP);
	fclose(FP);
 	sscanf(buf, "%x", &hold);
	if((hold & 0x400000) == 0)
		return (1);
	else
		return (0);
}

int get_lcd_backlight(void){
	FILE *FP;
	int backlight=0;
	char buf[12];
	FP = fopen("/proc/jz/lcd_backlight", "rb");
	if (!FP) return(0);
	fgets(buf, 12, FP);
	fclose(FP);
 	sscanf(buf, "%d", &backlight);
	return (backlight);
}

void set_lcd_backlight(int bk){

	char buf[128];
	sprintf(buf,"echo %d >/proc/jz/lcd_backlight",bk);
	system(buf);
	//sync();
}

void ShowCredit(){
	uint8 *keyssnes;
	int line=0,ypix=0;
	char disptxt[100][256]={
	"",
	"",
	"",
	"",
	" Snes9x for DINGUX",
	"                                     ",
	" Thank you using Snes9X !          ",
	"                                     ",
	" Key Configurations,     ",
	"  State Save: START + R ",
	"  State Load: START + L  ",
	"  Exit      : START + SEL + X ",
	"  Reset Game: START + SEL + B "
	"",
	"",
	" by SiENcE",
	" crankgaming.blogspot.com",
	"",
	""
	};

	do{
		SDL_Event event;
		SDL_PollEvent(&event);
		keyssnes = SDL_GetKeyState(NULL);
		
		for(int y=12; y<=212; y++){
			for(int x=10; x<246*2; x+=2){
				memset(GFX.Screen + 320*y*2+x,0x11,2);
			}	
		}
		
		for(int i=0;i<=16;i++){
			int j=i+line;
			if(j>=20) j-=20;
			S9xDisplayString (disptxt[j], GFX.Screen, 640,i*10+80-ypix);
		}
		
		ypix+=2;
		if(ypix==12) {
			line++;
			ypix=0;
		}
		if(line == 20) line = 0;
		S9xDeinitUpdate (320, 240);
		usleep(20000);
	}while(keyssnes[sfc_key[B_1]] != SDL_PRESSED);
	return;
}

