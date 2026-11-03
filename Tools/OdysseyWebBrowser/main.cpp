/*
 * Copyright (C) 2009 Fabien Coeurjoly
 * Copyright (C) 2008 Pleyo.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Pleyo nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PLEYO AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PLEYO OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <cairo.h>

#include <proto/timer.h>
#include <dos/dos.h>
#include <dos/notify.h>
#include <proto/dos.h>

#ifdef __amigaos4__
#include <proto/application.h>
#include <proto/intuition.h>
#include <proto/codesets.h>
#include <proto/diskfont.h>
#include <proto/locale.h>

#include <classes/requester.h>
#include <diskfont/diskfonttag.h>
#endif

#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <proto/icon.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if !defined(__AROS__) && !defined(__amigaos4__)
#include <sys/signal.h>
#endif
#include <locale.h>
#include <setjmp.h>

#include <clib/debug_protos.h>

#include "gui.h"
#include "alocale.h"
#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
#include <proto/intuition.h>

#include "AmigaOS3CurlShutdown.h"

extern "C" void wtfAmigaOS3StopWorkQueues(void);
#endif

#undef accept

#define D(x)

void close_dictionary();
void destroy_application(void);
void close_libs(void);

#if !defined(__amigaos4__) && !defined(__amigaos3__) && !(defined(__amigaos__) && defined(__mc68000__))
extern "C"
{
	int raise(int sig)
	{
		kprintf("raise(%d) was called, dumping stackframe...\n", sig);

#if !defined(__AROS__)
		DumpTaskState(FindTask(NULL));
#endif

		return(0);
	}
	/*
	void abort(void)
	{
		kprintf("abort was called, dumping stackframe...\n");

        DumpTaskState(FindTask(NULL));

		Wait(0);
	} */
}
#endif

/* Globals */
/* Layout recursion has no stack check. Floats nested to WebCore's render depth
   limit of 512 use 1.6 MiB on OS3; other measured pages use under 400 KiB. */
unsigned long __stack = 2*1024*1024;
jmp_buf bailout_env;

#ifdef __amigaos4__
TEXT version[] __attribute__((section(".text"))) = "$VER: Odyssey Web Browser 3.2.2 (27.10.2026)";
static const char * __attribute__((used)) stackcookie = "$STACK: 20000000";
struct Library 			*IntuitionBase	= NULL;
struct IntuitionIFace	*IIntuition 	= NULL;

struct Library			*GfxBase		= NULL;
struct GraphicsIFace	*IGraphics 		= NULL;

struct Library			*WorkbenchBase	= NULL;
struct WorkbenchIFace	*IWorkbench		= NULL;

struct Library			*LocaleBase		= NULL;
struct LocaleIFace		*ILocale		= NULL;

struct Library			*RexxSysBase	= NULL;
struct RexxSysIFace		*IRexxSys		= NULL;

struct Library			*IFFParseBase	= NULL;
struct IFFParseIFace	*IIFFParse		= NULL;

struct Library			*LayersBase		= NULL;
struct LayersIFace		*ILayers		= NULL;

struct Library			*ApplicationBase	= NULL;
struct ApplicationIFace *IApplication		= NULL;

struct Library			*KeymapBase		= NULL;
struct KeymapIFace		*IKeymap		= NULL;

struct Library			*MUIMasterBase	= NULL;
struct MUIMasterIFace	*IMUIMaster		= NULL;

struct Library			*CodesetsBase	= NULL;
struct CodesetsIFace	*ICodesets		= NULL;

struct Library			*DiskfontBase	= NULL;
struct DiskfontIFace	*IDiskfont		= NULL;

struct Library			*IconBase		= NULL;
struct IconIFace		*IIcon			= NULL;

struct Library			*ExpatBase		= NULL;
struct ExpatIFace		*IExpat			= NULL;

struct Library			*OpenURLBase	= NULL;
struct OpenURLIFace		*IOpenURL		= NULL;

struct Device			*TimerBase		= NULL;
struct TimerIFace		*ITimer			= NULL;
struct MsgPort			*TimerMP		= NULL;
struct TimeRequest		*TimerIO		= NULL;

struct AHIIFace 		*IAHI 			= NULL;
#else
struct Library * IconBase = NULL;
struct Library * MUIMasterBase = NULL;
struct Library * KeymapBase = NULL;
struct Library * CharsetsBase = NULL;
struct Library * SpellCheckerBase = NULL;
struct Library * ExpatBase = NULL;
#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
struct Library * CyberGfxBase = NULL;
#endif
#endif

static ULONG __morphos2 = FALSE;

/* Dos notify stuff */

ULONG dosnotifysig=0;
LONG dosnotifybit=-1;
struct NotifyRequest nr;
#ifdef __amigaos4__
uint32 appID;
#endif

LONG dosnotify_init(void)
{
	dosnotifybit = AllocSignal(-1);
	if(dosnotifybit!=-1)
	{
		dosnotifysig = 1L << dosnotifybit;
		return (TRUE);
	}
	return (FALSE);
}

void dosnotify_cleanup(void)
{
	if(dosnotifybit)
	{
		FreeSignal(dosnotifybit);
	}
}

struct NotifyRequest *dosnotify_start(CONST_STRPTR name)
{
	struct NotifyRequest *nr;
	ULONG namelen;

	namelen = strlen(name);
	if((namelen) && (dosnotifysig))
	{
		if( (nr = (struct NotifyRequest *)malloc(sizeof(*nr) + namelen + 1)) )
		{
			nr->nr_Name= (char *) (nr + 1);
			memcpy((char *) nr->nr_Name, name, namelen + 1);
			nr->nr_stuff.nr_Signal.nr_Task   = FindTask(NULL);
			nr->nr_stuff.nr_Signal.nr_SignalNum = dosnotifysig;
			nr->nr_Flags           = NRF_SEND_SIGNAL;
			if(StartNotify(nr))
			{
				return (nr);
			}
			free(nr);
		}
	}
	return (NULL);
}


void dosnotify_stop(struct NotifyRequest *nr)
{
	if(nr)
	{
		EndNotify(nr);
		free(nr);
	}
}

ULONG is_morphos2(void)
{
	return __morphos2;
}

static void report_startup_failure(const char *text)
{
#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
	/* Workbench starts have no console, and MUI may be what is missing. */
	struct EasyStruct request = { sizeof(request), 0, (STRPTR)"Odyssey", (STRPTR)"%s", (STRPTR)"OK" };
	CONST_APTR arguments[] = { text };

	if(Output())
		fprintf(stderr, "%s\n", text);
	EasyRequestArgs(NULL, &request, NULL, arguments);
#else
	fprintf(stderr, "%s\n", text);
#endif
}

#ifdef __amigaos4__

ULONG open_libs(void)
{
    if(!(KeymapBase = OpenLibrary("keymap.library", 39))) {
        fprintf(stderr, "Failed to open keymap.library.\n");
        return FALSE;
    }
    IKeymap = (struct KeymapIFace*)GetInterface(KeymapBase, "main", 1, NULL);


    // handle mui nicely
    Object               *win_obj;
    Object               *requester_object;
    uint32                result = 0;

    if((MUIMasterBase = OpenLibrary(MUIMASTER_NAME, 20)) != NULL)
    {
        if(MUIMasterBase->lib_Version > 20 || (MUIMasterBase->lib_Version == 20 && MUIMasterBase->lib_Revision >= 6175))
        {
            // we have MUI4 with a working Title.mui
            IMUIMaster = (struct MUIMasterIFace *)GetInterface(MUIMasterBase, "main", 1, NULL);
        }
        else
        {
            // something older
            requester_object = NewObject(NULL, "requester.class",
                        REQ_Type,  REQTYPE_INFO,
                        REQ_Image, REQIMAGE_ERROR,
                        REQ_TitleText,  "old mui",
                        REQ_BodyText,   "Please install latest versions of mui4",
                        REQ_GadgetText, "OK",
                        TAG_END);

            SetAttrs(win_obj, WA_BusyPointer, TRUE, TAG_DONE);
            result = IDoMethod(requester_object, RM_OPENREQ, NULL, NULL, /*window,*/ NULL, TAG_DONE);
            DisposeObject(requester_object);
            requester_object = NULL;
            SetAttrs(win_obj, WA_BusyPointer, FALSE, TAG_DONE);

                return FALSE;
        }
    }
    else
    {
        // no MUI at all

        requester_object = NewObject(NULL, "requester.class",
                REQ_Type,  REQTYPE_INFO,
                REQ_Image, REQIMAGE_ERROR,
                REQ_TitleText,  "no mui",
                REQ_BodyText,   "Please install mui",
                REQ_GadgetText, "OK",
                TAG_END);

        SetAttrs(win_obj, WA_BusyPointer, TRUE, TAG_DONE);
        result = IDoMethod(requester_object, RM_OPENREQ, NULL, NULL, /*window,*/ NULL, TAG_DONE);
        DisposeObject(requester_object);
        requester_object = NULL;
        SetAttrs(win_obj, WA_BusyPointer, FALSE, TAG_DONE);

        return FALSE;
    }

    if(!(IntuitionBase = OpenLibrary("intuition.library", 37))) {
        fprintf(stderr, "Failed to open intuition.library.\n");
        return FALSE;
    }
    IIntuition = (struct IntuitionIFace *)GetInterface(IntuitionBase, "main", 1, NULL);

    if(!(GfxBase = OpenLibrary("graphics.library", 37))) {
        fprintf(stderr, "Failed to open graphics.library.\n");
        return FALSE;
    }
    IGraphics = (struct GraphicsIFace *)GetInterface(GfxBase, "main", 1, NULL);

    if(!(WorkbenchBase = OpenLibrary("workbench.library", 37))) {
        fprintf(stderr, "Failed to open workbench.library.\n");
        return FALSE;
    }
    IWorkbench = (struct WorkbenchIFace *)GetInterface(WorkbenchBase, "main", 1, NULL);

    if(!(LocaleBase = OpenLibrary("locale.library", 37))) {
        fprintf(stderr, "Failed to open locale.library.\n");
        return FALSE;
    }
    ILocale = (struct LocaleIFace *)GetInterface(LocaleBase, "main", 1, NULL);

    if(!(RexxSysBase = OpenLibrary("rexxsyslib.library", 37))) {
        fprintf(stderr, "Failed to open rexxsyslib.library.\n");
        return FALSE;
    }
    IRexxSys = (struct RexxSysIFace *)GetInterface(RexxSysBase, "main", 1, NULL);

    if(!(IFFParseBase = OpenLibrary("iffparse.library", 37))) {
        fprintf(stderr, "Failed to open iffparse.library.\n");
        return FALSE;
    }
    IIFFParse = (struct IFFParseIFace *)GetInterface(IFFParseBase, "main", 1, NULL);

    if(!(LayersBase = OpenLibrary("layers.library", 37))) {
        fprintf(stderr, "Failed to open layers.library.\n");
        return FALSE;
    }
    ILayers = (struct LayersIFace *)GetInterface(LayersBase, "main", 1, NULL);

    // application.library (to register app, to enable/disable blankers, etc).
    if (!(ApplicationBase = OpenLibrary("application.library", 52))) {
        fprintf(stderr,"Failed to open application.library\n");
        return FALSE;
    }
    IApplication  = (struct ApplicationIFace *)GetInterface(ApplicationBase, "application", 2, NULL);

    // register an app for aplication library
    appID = RegisterApplication("Odyssey",
                REGAPP_URLIdentifier, "none",
                REGAPP_Description, "Odyssey Web Browser",
                TAG_END);

    if(!(CodesetsBase = OpenLibrary(CODESETSNAME, CODESETSVER))) {
        fprintf(stderr, "Failed to open codesets.library.\n");
        return FALSE;
    }
    ICodesets = (struct CodesetsIFace *)GetInterface(CodesetsBase, "main", 1, NULL);

    if(!(DiskfontBase = OpenLibrary("diskfont.library", 37))) {
        fprintf(stderr, "Failed to open diskfont.library.\n");
        return FALSE;
    }
    IDiskfont = (struct DiskfontIFace *)GetInterface(DiskfontBase, "main", 1, NULL);

    if(!(IconBase = OpenLibrary("icon.library", 39))) {
        fprintf(stderr, "Failed to open icon.library.\n");
        return FALSE;
    }
    IIcon = (struct IconIFace *)GetInterface(IconBase, "main", 1, NULL);

    if(!(ExpatBase = OpenLibrary("expat.library", 0))) {
        fprintf(stderr, "Failed to open expat.library.\n");
        return FALSE;
    }
    IExpat = (struct ExpatIFace *)GetInterface(ExpatBase, "main", 1, NULL);

    // handle openurl differently: dind't exit if can't open, just skip it (use it only as fallback if URLOpen didn't works).
    if((OpenURLBase = OpenLibrary("openurl.library", 0))) {
        IOpenURL = (struct OpenURLIFace *)GetInterface(OpenURLBase, "main", 1, NULL);
    }

    //timer
    TimerMP = (struct MsgPort *)AllocSysObjectTags(ASOT_PORT, ASOPORT_Name, "Timer port", TAG_DONE);
    TimerIO = (struct TimeRequest *)AllocSysObjectTags(ASOT_IOREQUEST,ASOIOR_Size, sizeof(struct TimeRequest),ASOIOR_ReplyPort, TimerMP,TAG_DONE);

    if(!(OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)TimerIO, 0))) {
        TimerBase = TimerIO->Request.io_Device;
        ITimer = (struct TimerIFace *)GetInterface((struct Library *)TimerBase, "main", 1, NULL);
    } else {
        fprintf(stderr, "Failed to open timer device.\n");
        return FALSE;
    }

    extern void init_useragent();
    init_useragent();

    // Hack to make sure cairo mutex are initialized
    {
        cairo_surface_t *dummysurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4);
        cairo_surface_destroy(dummysurface);
    }

    return TRUE;
}

void close_libs(void)
{
    if(KeymapBase) {
        DropInterface((struct Interface*)IKeymap);
        CloseLibrary(KeymapBase);
        KeymapBase = NULL;
    }

    if(MUIMasterBase) {
        DropInterface((struct Interface*)IMUIMaster);
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
    }

    if (IntuitionBase) {
        DropInterface((struct Interface*)IIntuition);
        CloseLibrary(IntuitionBase);
        IntuitionBase = NULL;
    }

    if (GfxBase) {
        DropInterface((struct Interface*)IGraphics);
        CloseLibrary(GfxBase);
        GfxBase = NULL;
    }

    if (WorkbenchBase) {
        DropInterface((struct Interface*)IWorkbench);
        CloseLibrary(WorkbenchBase);
        WorkbenchBase = NULL;
    }

    if (LocaleBase) {
        DropInterface((struct Interface*)ILocale);
        CloseLibrary(LocaleBase);
        LocaleBase = NULL;
    }

    if (RexxSysBase) {
        DropInterface((struct Interface*)IRexxSys);
        CloseLibrary(RexxSysBase);
        RexxSysBase = NULL;
    }

    if (IFFParseBase) {
        DropInterface((struct Interface*)IIFFParse);
        CloseLibrary(IFFParseBase);
        IFFParseBase = NULL;
    }

    if (LayersBase) {
        DropInterface((struct Interface*)ILayers);
        CloseLibrary(LayersBase);
        LayersBase = NULL;
    }

    if (ApplicationBase) {
        UnregisterApplication(appID, NULL);
        DropInterface((struct Interface*)IApplication);
        CloseLibrary(ApplicationBase);
        ApplicationBase = NULL;
    }

    if(CodesetsBase) {
        DropInterface((struct Interface*)ICodesets);
        CloseLibrary(CodesetsBase);
        CodesetsBase = NULL;
    }

    if(DiskfontBase) {
        DropInterface((struct Interface*)IDiskfont);
        CloseLibrary(DiskfontBase);
        DiskfontBase = NULL;
    }

    if(IconBase) {
        DropInterface((struct Interface*)IIcon);
        CloseLibrary(IconBase);
        IconBase = NULL;
    }

    if(ExpatBase) {
        DropInterface((struct Interface*)IExpat);
        CloseLibrary(ExpatBase);
        ExpatBase = NULL;
    }

    if(OpenURLBase) {
        DropInterface((struct Interface*)IOpenURL);
        CloseLibrary(OpenURLBase);
        OpenURLBase = NULL;
    }

    if(TimerBase) {
        DropInterface((struct Interface *)ITimer);
        CloseDevice((struct IORequest *)TimerIO);
        FreeSysObject(ASOT_IOREQUEST, TimerIO);
        FreeSysObject(ASOT_PORT, TimerMP);
        TimerBase = NULL;
    }
}
#else
#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
/* MUI 3.9 reports version 20 without the Title class this browser subclasses,
   and Zune68's usual build reports 19. */
#define ODYSSEY_MUI_VERSION 21

static void report_mui_version(void)
{
	char text[128];
	struct Library *found = OpenLibrary(MUIMASTER_NAME, 0);

	if(found)
	{
		snprintf(text, sizeof(text), "Odyssey needs MUI 5, " MUIMASTER_NAME " %d or later.\nThis system has version %d.%d.",
			ODYSSEY_MUI_VERSION, found->lib_Version, found->lib_Revision);
		CloseLibrary(found);
	}
	else
	{
		snprintf(text, sizeof(text), "Odyssey needs MUI 5, " MUIMASTER_NAME " %d or later.\nNo " MUIMASTER_NAME " was found.",
			ODYSSEY_MUI_VERSION);
	}

	report_startup_failure(text);
}
#else
#define ODYSSEY_MUI_VERSION MUIMASTER_VMIN

static void report_mui_version(void)
{
	report_startup_failure("Failed to open " MUIMASTER_NAME ".");
}
#endif

ULONG open_libs(void)
{
	if(!(KeymapBase = OpenLibrary("keymap.library", 39)))
	{
		report_startup_failure("Failed to open keymap.library.");
		return FALSE;
	}

	if(!(MUIMasterBase = OpenLibrary(MUIMASTER_NAME, ODYSSEY_MUI_VERSION)))
	{
		report_mui_version();
		return FALSE;
	}

	if(!(IconBase = OpenLibrary("icon.library", 39)))
	{
		report_startup_failure("Failed to open icon.library.");
		return FALSE;
	}

#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
	// Expat is linked statically; RTG drawing uses the classic CGX ABI.
	if (!(CyberGfxBase = OpenLibrary("cybergraphics.library", 41)))
	{
		report_startup_failure("Failed to open cybergraphics.library V41.\nOdyssey needs an RTG system.");
		return FALSE;
	}
#else
	if(!(ExpatBase = OpenLibrary("expat.library", 0)))
	{
		report_startup_failure("Failed to open expat.library.");
		return FALSE;
	}
#endif

	struct Library * ibase = (struct Library *) IntuitionBase;
	if(ibase)
	{
		if(ibase->lib_Version > 50 || (ibase->lib_Version == 50 && ibase->lib_Revision >= 61))
		{
			if(KeymapBase->lib_Version > 50) // Henes 1.5 check
			{
				__morphos2 = TRUE;
			}
		}
	}

	extern void init_useragent();
	init_useragent();

	// Use charsets.library if available
	CharsetsBase = OpenLibrary("charsets.library", 51);

	// Use spellchecker if available
	SpellCheckerBase = OpenLibrary("spellchecker.library", 0);

	// BsdSocket check. Making it later is too risky.
	struct Library *mainSocketBase = OpenLibrary("bsdsocket.library", 4);

	if(!mainSocketBase)
	{
		report_startup_failure("Failed to open bsdsocket.library.\nOdyssey needs a running TCP/IP stack.");
		return FALSE;
	}
	else
	{
		CloseLibrary(mainSocketBase);
		mainSocketBase = NULL;
	}

	// Hack to make sure cairo mutex are initialized
	{
		cairo_surface_t *dummysurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4);
		cairo_surface_destroy(dummysurface);
	}

	return TRUE;
}

void close_libs(void)
{
#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
	if (CyberGfxBase)
	{
		CloseLibrary(CyberGfxBase);
		CyberGfxBase = NULL;
	}
#endif
	if(MUIMasterBase)
	{
		CloseLibrary(MUIMasterBase);
		MUIMasterBase = NULL;
	}

	if(ExpatBase)
	{
		CloseLibrary(ExpatBase);
		ExpatBase = NULL;
	}

	if(IconBase)
	{
		CloseLibrary(IconBase);
		IconBase = NULL;
	}

	if(KeymapBase)
	{
		CloseLibrary(KeymapBase);
		KeymapBase = NULL;
	}

	if(CharsetsBase)
	{
		CloseLibrary(CharsetsBase);
		CharsetsBase = NULL;
	}

	if(SpellCheckerBase)
	{
		close_dictionary();
		CloseLibrary(SpellCheckerBase);
		SpellCheckerBase = NULL;
	}
}
#endif

void destroy_application(void)
{
	if(app)
	{
		MUI_DisposeObject(app);
		methodstack_cleanup();
		classes_cleanup();
		app = NULL;
	}

	locale_cleanup();
}

// Safety checks
extern ULONG icu_check(void);

ULONG safety_check(void)
{
	ULONG passed = TRUE;

	passed &= icu_check();

	return passed;
}

Object *create_application(char *url)
{
	Object *obj = NULL;

	if (!locale_init())
	{
		report_startup_failure("Failed to open the locale.");
		return NULL;
	}

	if(classes_init())
	{
	    methodstack_init();
		obj = (Object *) NewObject(getowbappclass(), NULL, MA_OWBBrowser_URL, (IPTR) url, TAG_DONE);
		if (!obj)
		{
			report_startup_failure("Failed to create the application.\nCheck your MUI installation.");
			methodstack_cleanup();
			classes_cleanup();
		}
	}
	else
	{
		report_startup_failure("Failed to create internal classes.\nCheck that Title.mui, Listtree.mcc and Calltips.mcc are installed.");
		classes_cleanup();
	}

	return obj;
}

#if defined(__AROS__)
extern "C" {
void aros_register_trap_handler();
int aros_timer_open();
void aros_timer_close();
}
#endif

void main_loop(void)
{
	ULONG running = TRUE;
	ULONG signals;
#if defined(__amigaos3__)
	ULONG lowMemorySignal = watchLowMemory();
#else
	ULONG lowMemorySignal = 0;
#endif

	setIsSafeToQuit(TRUE);
	
	if(setjmp(bailout_env) == -1)
	{
	    running = FALSE;
	}

#if defined(__AROS__)
    if (running)
        aros_register_trap_handler();
#endif

	while (running)
	{
		LONG ret = DoMethod(app,MUIM_Application_NewInput,&signals);

		switch(ret)
		{
			case MUIV_Application_ReturnID_Quit:
			{
				setIsQuitting(TRUE);
				if(isSafeToQuit())
					running = FALSE;
				break;
			}
		}

		methodstack_check();

		/* Refresh each active browser if needed */
		DoMethod(app, MM_OWBApp_Expose);

		if(running && signals) signals = Wait(signals | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F | lowMemorySignal/* | dosnotifysig */);

#if defined(__amigaos3__)
		if(signals & lowMemorySignal)
			releaseMemoryAfterAllocationFailure();
#endif

		if((signals & SIGBREAKF_CTRL_C) || isQuitting())
		{
			setIsQuitting(TRUE);
			if(isSafeToQuit())
				running = FALSE;
		}

		if(signals & SIGBREAKF_CTRL_E)
		{
			/* Run webkit events for each active browser */
			DoMethod(app, MM_OWBApp_WebKitEvents);
		}
		
		/*
		if(signals & dosnotifysig )
		{
			//kprintf("DosNotify Trig\n");
			//DoMethod(getv(app, MA_OWBApp_BookmarkGroup), MM_Bookmarkgroup_DosNotify);
		}
		*/

		methodstack_check();
	}

#if defined(__amigaos3__)
	stopWatchingLowMemory();
#endif
}

#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
static int runBrowser(int argc, char* argv[])
#else
int main (int argc, char* argv[])
#endif
{
	int result = 20;
	#if defined(__amigaos4__) || defined(__amigaos3__)
	// save tc_UserData on run; WTF keeps each thread's Thread there.
	struct Task *CurrentTask = FindTask(NULL);
	APTR olduserdata = CurrentTask->tc_UserData;
	CurrentTask->tc_UserData = NULL;
	#endif

#if !defined(__AROS__)
	signal(SIGINT, SIG_IGN);
#endif
	atexit(close_libs);
#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
    atexit(wtfAmigaOS3StopWorkQueues);
    atexit(odysseyAmigaOS3CurlShutdown);
#endif
	atexit(destroy_application);
#if defined(__AROS__)
	atexit(aros_timer_close);
#endif

	setlocale(LC_TIME, "C");
	setlocale(LC_NUMERIC, "C");

#if defined(__AROS__)
   SetVar("FONTCONFIG_PATH", "PROGDIR:Conf/font", -1, LV_VAR | GVF_LOCAL_ONLY);
   aros_timer_open();
#endif

	if (!argc)
	{
		freopen("NIL:", "w", stderr);
		freopen("NIL:", "w", stdout);
#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
		/* newlib sizes a stream's buffer on its first write with ExamineFH,
		   which fails with error #80000008 on NIL:, as NIL: has no handler. */
		setvbuf(stderr, NULL, _IONBF, 0);
		setvbuf(stdout, NULL, _IONBF, 0);
#endif
	}

	//dosnotify_init();

	if(open_libs())
	{
		if(safety_check())
		{
			app = create_application(argc > 1 ? (char*)argv[1] : (char *)"");

			if(app)
			{
				main_loop();
				result = 0;
			}

			destroy_application();
		}
	}
#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
    odysseyAmigaOS3CurlShutdown();
#endif
	close_libs();

	//dosnotify_cleanup();

    #if defined(__amigaos4__) || defined(__amigaos3__)
	// restore tc_UserData at exit
	CurrentTask->tc_UserData = olduserdata;
	#endif

    return result;
}

#if defined(__amigaos3__) || (defined(__amigaos__) && defined(__mc68000__))
/* newlib's startup ignores __stack, and Workbench or the Shell may supply only
   4 KiB, which the browser overruns without any check. These are static because
   locals are unreachable once StackSwap has moved the stack pointer. */
static struct StackSwapStruct browserStack;
static int browserArgc, browserResult;
static char** browserArgv;

/* WTF records the launch stack during static construction, before main. */
extern "C" void wtfAmigaOS3StackSwapped(void);

/* Separate so that main's stack pointer is the same on both sides of the swap. */
static void __attribute__((noinline)) runOnBrowserStack(void)
{
	wtfAmigaOS3StackSwapped();
	browserResult = runBrowser(browserArgc, browserArgv);
}

int main (int argc, char* argv[])
{
	struct Task *task = FindTask(NULL);

	if((ULONG)task->tc_SPUpper - (ULONG)task->tc_SPLower >= __stack)
		return runBrowser(argc, argv);

	APTR stack = AllocVec(__stack, MEMF_ANY);

	if(!stack)
	{
		report_startup_failure("Not enough memory for the browser's 2 MiB stack.");
		return 20;
	}

	browserStack.stk_Lower = stack;
	browserStack.stk_Upper = (ULONG)stack + __stack;
	browserStack.stk_Pointer = (APTR)browserStack.stk_Upper;
	browserArgc = argc;
	browserArgv = argv;

	StackSwap(&browserStack);
	runOnBrowserStack();
	StackSwap(&browserStack);
	wtfAmigaOS3StackSwapped();

	FreeVec(browserStack.stk_Lower);
	return browserResult;
}
#endif
