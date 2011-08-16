/* Copyright (c) NetHack PC Development Team 1993, 1994 */
/* NetHack may be freely redistributed.  See license for details. */

/*
 *  WIN32 system functions.
 *
 *  Initial Creation: Michael Allison - January 31/93
 *
 */

#include "hack.h"
#include <dos.h>
#include <ctype.h>
#include "win32api.h"
#ifdef WIN32CON
#include "wintty.h"
#endif
#ifdef WIN32


/*
 * The following WIN32 API routines are used in this file.
 *
 * GetDiskFreeSpace
 * GetVolumeInformation
 * GetUserName
 * FindFirstFile
 * FindNextFile
 * FindClose
 *
 */


/* globals required within here */
HANDLE ffhandle = (HANDLE)0;
WIN32_FIND_DATA ffd;

/* The function pointer nt_kbhit contains a kbhit() equivalent
 * which varies depending on which window port is active.
 * For the tty port it is tty_kbhit() [from nttty.c]
 * For the win32 port it is win32_kbhit() [from winmain.c]
 * It is initialized to point to def_kbhit [in here] for safety.
 */

int def_kbhit(void);
int (*nt_kbhit)() = def_kbhit;

char switchar(void)
{
 /* Could not locate a WIN32 API call for this- MJA */
	return '-';
}

long freediskspace(char *path)
{
	char tmppath[4];
	DWORD SectorsPerCluster = 0;
	DWORD BytesPerSector = 0;
	DWORD FreeClusters = 0;
	DWORD TotalClusters = 0;

	tmppath[0] = *path;
	tmppath[1] = ':';
	tmppath[2] = '\\';
	tmppath[3] = '\0';
	GetDiskFreeSpace(tmppath, &SectorsPerCluster,
			&BytesPerSector,
			&FreeClusters,
			&TotalClusters);
	return (long)(SectorsPerCluster * BytesPerSector *
			FreeClusters);
}

/*
 * Functions to get filenames using wildcards
 */
int findfirst(char *path)
{
	if (ffhandle){
		 FindClose(ffhandle);
		 ffhandle = (HANDLE)0;
	}
	ffhandle = FindFirstFile(path,&ffd);
	return 
	  (ffhandle == INVALID_HANDLE_VALUE) ? 0 : 1;
}

int findnext(void) 
{
	return FindNextFile(ffhandle,&ffd) ? 1 : 0;
}

char *foundfile_buffer(void)
{
	return &ffd.cFileName[0];
}

long filesize(char *file)
{
	if (findfirst(file)) {
		return (long)ffd.nFileSizeLow;
	} else
		return -1L;
}

/*
 * Chdrive() changes the default drive.
 */
void chdrive(char *str)
{
	char *ptr;
	char drive;
	if ((ptr = strchr(str, ':')) != NULL) 
	{
		drive = toupper(*(ptr - 1));
		_chdrive((drive - 'A') + 1);
	}
}

static int max_filename(void)
{
	DWORD maxflen;
	int status=0;
	
	status = GetVolumeInformation((LPTSTR)0,(LPTSTR)0, 0
			,(LPDWORD)0,&maxflen,(LPDWORD)0,(LPTSTR)0,0);
	if (status) return maxflen;
	else return 0;
}

int def_kbhit(void)
{
	return 0;
}


/*
 * This is used in nhlan.c to implement some of the LAN_FEATURES.
 */
char *get_username(int *lan_username_size)
{
	static TCHAR username_buffer[BUFSZ];
	unsigned int status;
	DWORD i = BUFSZ - 1;

	/* i gets updated with actual size */
	status = GetUserName(username_buffer, &i);		
	if (status) username_buffer[i] = '\0';
	else strcpy(username_buffer, "NetHack");
	if (lan_username_size) *lan_username_size = strlen(username_buffer);
	return username_buffer;
}

# if 0
char *getxxx()
{
char     szFullPath[MAX_PATH] = "";
HMODULE  hInst = NULL;  	/* NULL gets the filename of this module */

GetModuleFileName(hInst, szFullPath, sizeof(szFullPath));
return &szFullPath[0];
}
# endif

#ifndef WIN32CON
/* fatal error */
/*VARARGS1*/
void error (const char *s, ...)
{
	va_list the_args;
	char buf[BUFSZ];
	va_start(the_args, s);
	/* error() may get called before tty is initialized */
	if (iflags2.window_inited) end_screen();
	if (!strncmpi(windowprocs.name, "tty", 3)) {
		buf[0] = '\n';
		vsprintf(&buf[1], s, the_args);
		strcat(buf, "\n");
		msmsg(buf);
	} else {
		vsprintf(buf, s, the_args);
		strcat(buf, "\n");
		raw_printf(buf);
	}
	va_end(the_args);
	exit(EXIT_FAILURE);
}
#endif

void Delay(int ms)
{
	Sleep(ms);
}

#ifdef WIN32CON
extern void backsp(void);
#endif

void win32_abort(void)
{
   	if (wizard) {
# ifdef WIN32CON
	    int c, ci, ct;

   	    if (!iflags2.window_inited)
		c = 'n';
		ct = 0;
		msmsg("Execute debug breakpoint wizard?");
		while ((ci=nhgetch()) != '\n') {
		    if (ct > 0) {
			backsp();       /* \b is visible on NT */
			putchar(' ');
			backsp();
			ct = 0;
			c = 'n';
		    }
		    if (ci == 'y' || ci == 'n' || ci == 'Y' || ci == 'N') {
		    	ct = 1;
		        c = ci;
		        msmsg("%c",c);
		    }
		}
		if (c == 'y')
			DebugBreak();
# endif
	}
	abort();
}

static char interjection_buf[INTERJECTION_TYPES][1024];
static int interjection[INTERJECTION_TYPES];

void interject_assistance(int num, int interjection_type, void * ptr1, void * ptr2)
{
	switch(num) {
	    case 1: {
		char *panicmsg = (char *)ptr1;
		char *datadir =  (char *)ptr2;
		char *tempdir = nh_getenv("TEMP");
		interjection_type = INTERJECT_PANIC;
		interjection[INTERJECT_PANIC] = 1;
		/*
		 * ptr1 = the panic message about to be delivered.
		 * ptr2 = the directory prefix of the dungeon file
		 *        that failed to open.
		 * Check to see if datadir matches tempdir or a
		 * common windows temp location. If it does, inform
		 * the user that they are probably trying to run the
		 * game from within their unzip utility, so the required
		 * files really don't exist at the location. Instruct
		 * them to unpack them first.
		 */
		if (panicmsg && datadir) {
		    if (!strncmpi(datadir, "C:\\WINDOWS\\TEMP", 15) ||
			    strstri(datadir, "TEMP")   ||
			    (tempdir && strstri(datadir, tempdir))) {
			strncpy(interjection_buf[INTERJECT_PANIC],
			"\nOne common cause of this error is attempting to execute\n"
			"the game by double-clicking on it while it is displayed\n"
			"inside an unzip utility.\n\n"
			"You have to unzip the contents of the zip file into a\n"
			"folder on your system, and then run \"NetHack.exe\" or \n"
			"\"NetHackW.exe\" from there.\n\n"
			"If that is not the situation, you are encouraged to\n"
			"report the error as shown above.\n\n", 1023);
		    }
		}
	    }
	    break;
	}
}

void interject(int interjection_type)
{
	if (interjection_type >= 0 && interjection_type < INTERJECTION_TYPES)
		msmsg(interjection_buf[interjection_type]);
}
#endif /* WIN32 */

/*winnt.c*/
