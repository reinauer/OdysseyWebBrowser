/* Verify the installed native browser fixture before launching Odyssey. */
#define __USE_NEW_TIMEVAL__ 1
#include <cybergraphx/cybergraphics.h>
#include <dos/dos.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <libraries/locale.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>
#include <libraries/mui.h>
#include <prefs/screenmode.h>
#include <proto/cybergraphics.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/locale.h>
#include <proto/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *CyberGfxBase, *MUIMasterBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;

static int setFixtureClock(void)
{
    FILE* input = fopen("SYS:browser-test-utc.txt", "r");
    unsigned long utc = 0;
    if (!input) return 20;
    int parsed = fscanf(input, "%lu", &utc);
    fclose(input);
    if (parsed != 1 || utc < 252460800UL) return 20;
    LONG minutesWest = 0;
    struct LocaleBase* LocaleBase = (struct LocaleBase*)OpenLibrary("locale.library", 38);
    if (LocaleBase) {
        struct Locale* locale = OpenLocale(NULL);
        if (locale) {
            minutesWest = locale->loc_GMTOffset;
            CloseLocale(locale);
        }
        CloseLibrary((struct Library*)LocaleBase);
    }
    const char* dstText = getenv("DST_OFFSET");
    LONG dst = dstText ? strtol(dstText, NULL, 10) : 0;
    struct MsgPort* port = CreateMsgPort();
    if (!port) return 20;
    struct TimeRequest request = { 0 };
    request.tr_node.io_Message.mn_ReplyPort = port;
    request.tr_node.io_Message.mn_Length = sizeof(request);
    int result = 20;
    if (!OpenDevice(TIMERNAME, UNIT_MICROHZ, &request.tr_node, 0)) {
        struct Device* TimerBase = request.tr_node.io_Device;
        struct TimeVal now;
        GetSysTime(&now);
        printf("Fixture clock: original UTC=%lu offset=%ld DST=%ld host UTC=%lu\n",
            now.tv_secs + 252460800UL + minutesWest * 60 + dst, minutesWest, dst, utc);
        request.tr_node.io_Command = TR_SETSYSTIME;
        request.tr_time.tv_secs = utc - 252460800UL - minutesWest * 60 - dst;
        if (!DoIO(&request.tr_node)) {
            GetSysTime(&now);
            ULONG after = now.tv_secs + 252460800UL + minutesWest * 60 + dst;
            if (after >= utc && after - utc <= 1) {
                printf("PASS disposable guest clock UTC=%lu\n", after);
                result = 0;
            }
        }
        CloseDevice(&request.tr_node);
    }
    DeleteMsgPort(port);
    return result;
}

static int setScreenMode(void)
{
    struct ScreenModePrefs preferences = { { 0 } };
    ULONG mode = BestCModeIDTags(CYBRBIDTG_Depth, 24,
        CYBRBIDTG_NominalWidth, 800, CYBRBIDTG_NominalHeight, 600, TAG_DONE);
    ULONG form[] = { 0x464f524d, 54, 0x50524546, 0x50524844, 6 };
    UBYTE header[6] = { 0 };
    ULONG chunk[] = { ID_SCRM, sizeof(preferences) };
    BPTR output;
    int ok;
    if (mode == ~0UL || !IsCyberModeID(mode))
        return 1;
    preferences.smp_DisplayID = mode;
    preferences.smp_Width = 800;
    preferences.smp_Height = 600;
    preferences.smp_Depth = 24;
    preferences.smp_Control = SMF_AUTOSCROLL;
    output = Open("ENV:Sys/screenmode.prefs", MODE_NEWFILE);
    if (!output)
        return 2;
    ok = Write(output, form, sizeof(form)) == sizeof(form)
        && Write(output, header, sizeof(header)) == sizeof(header)
        && Write(output, chunk, sizeof(chunk)) == sizeof(chunk)
        && Write(output, &preferences, sizeof(preferences)) == sizeof(preferences);
    Close(output);
    printf("Fixture screen mode: %08lx 800x600x24\n", mode);
    return ok ? 0 : 3;
}

static int checkFixture(void)
{
    const char *classes[] = { "Application.mui", "Window.mui", "Group.mui",
        "String.mui", "List.mui", "Title.mui", "Listtree.mcc", "Calltips.mcc" };
    unsigned i, failures = 0;
    printf("muimaster.library %u.%u\n", MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision);
    struct Screen *screen = LockPubScreen(NULL);
    if (!screen)
        return 4;
    printf("Workbench: %ux%u depth=%lu cyber=%lu\n", screen->Width, screen->Height,
        GetBitMapAttr(screen->RastPort.BitMap, BMA_DEPTH),
        GetCyberMapAttr(screen->RastPort.BitMap, CYBRMATTR_ISCYBERGFX));
    if (GetBitMapAttr(screen->RastPort.BitMap, BMA_DEPTH) < 24
        || !GetCyberMapAttr(screen->RastPort.BitMap, CYBRMATTR_ISCYBERGFX))
        ++failures;
    UnlockPubScreen(NULL, screen);
    for (i = 0; i < sizeof(classes) / sizeof(classes[0]); ++i) {
        if (strstr(classes[i], ".mcc")) {
            char path[128];
            struct Library *library;
            snprintf(path, sizeof(path), "LIBS:MUI/%s", classes[i]);
            library = OpenLibrary(path, 0);
            printf("OpenLibrary %s: %p IoErr=%ld\n", path, library, IoErr());
            if (library) CloseLibrary(library);
        }
        struct MUI_CustomClass *cl = MUI_CreateCustomClass(NULL, (STRPTR)classes[i], NULL, 0, NULL);
        printf("Superclass %s: %s\n", classes[i], cl ? "available" : "MISSING");
        if (cl)
            MUI_DeleteCustomClass(cl);
        else
            ++failures;
    }
    /* Opening the window runs Dtpic's datatype setup and layout. Its fixed
     * intrinsic size must match Dist/Resource/back.png (26 by 21 pixels). */
    Object* picture = MUI_NewObject(MUIC_Dtpic,
        MUIA_Dtpic_Name, "SYS:Odyssey/Resource/back.png",
        MUIA_Dtpic_Alpha, 255,
        MUIA_Dtpic_DarkenSelState, TRUE, TAG_DONE);
    Object* group = picture ? MUI_NewObject(MUIC_Group,
        MUIA_Group_Child, picture, TAG_DONE) : NULL;
    Object* window = group ? MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, "Odyssey toolbar PNG fixture",
        MUIA_Window_RootObject, group, TAG_DONE) : NULL;
    Object* application = window ? MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, "Odyssey fixture",
        MUIA_Application_Base, "ODYSSEYFIXTURE",
        MUIA_Application_Window, window, TAG_DONE) : NULL;
    if (application) {
        ULONG opened = FALSE, width = 0, height = 0;
        SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
        GetAttr(MUIA_Window_Open, window, &opened);
        GetAttr(MUIA_Width, picture, &width);
        GetAttr(MUIA_Height, picture, &height);
        printf("Dtpic Resource/back.png: open=%lu size=%lux%lu expected=26x21\n",
            opened, width, height);
        if (!opened || width != 26 || height != 21)
            ++failures;
        SetAttrs(window, MUIA_Window_Open, FALSE, TAG_DONE);
        MUI_DisposeObject(application);
    } else {
        puts("Dtpic Resource/back.png: object creation FAILED");
        ++failures;
        if (window) MUI_DisposeObject(window);
        else if (group) MUI_DisposeObject(group);
        else if (picture) MUI_DisposeObject(picture);
    }
    printf("Browser fixture: %u failures\n", failures);
    return failures ? 5 : 0;
}

static int checkDisk(void)
{
    const char* name = "DH1:Odyssey-native-long-filename-mount-test-0123456789.txt";
    const char* payload = "native FFS data\n";
    char buffer[32] = { 0 };
    BPTR lock = Lock("DH1:", SHARED_LOCK);
    struct InfoData* info = AllocVec(sizeof(*info), MEMF_PUBLIC | MEMF_CLEAR);
    int result = 20;
    if (!lock || !info || !Info(lock, info))
        goto done;
    printf("DH1 filesystem=%08lx blocks=%ld bytes/block=%ld free=%ld\n",
        info->id_DiskType, info->id_NumBlocks, info->id_BytesPerBlock,
        (info->id_NumBlocks - info->id_NumBlocksUsed) * info->id_BytesPerBlock);
    if (info->id_DiskType != 0x444f5307
        /* Info excludes the two reserved boot blocks from this raw image. */
        || (info->id_NumBlocks + 2) * info->id_BytesPerBlock < 256 * 1024 * 1024)
        goto done;
    BPTR file = Open(name, MODE_NEWFILE);
    if (!file)
        goto done;
    int written = Write(file, payload, strlen(payload)) == strlen(payload) && Flush(file);
    Close(file);
    if (!written)
        goto done;
    file = Open(name, MODE_OLDFILE);
    if (!file)
        goto done;
    int read = Read(file, buffer, sizeof(buffer));
    Close(file);
    if (read != strlen(payload) || memcmp(buffer, payload, read) || !DeleteFile(name))
        goto done;
    file = Open("DH1:Odyssey/Fonts/NotoSans-Regular.ttf", MODE_OLDFILE);
    if (!file)
        goto done;
    read = Read(file, buffer, 4);
    Close(file);
    if (read != 4 || memcmp(buffer, "\0\1\0\0", 4))
        goto done;
    puts("PASS native read of staged browser font");
    puts("PASS native DOS7 mount, long filename, flush and readback");
    result = 0;
done:
    if (info) FreeVec(info);
    if (lock) UnLock(lock);
    return result;
}

int main(int argc, char **argv)
{
    int result = 10;
    setvbuf(stdout, NULL, _IONBF, 0);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (GfxBase && IntuitionBase && CyberGfxBase && MUIMasterBase) {
        if (argc > 1 && !strcmp(argv[1], "--screen-mode")) result = setScreenMode();
        else if (argc > 1 && !strcmp(argv[1], "--disk")) result = checkDisk();
        else if (argc > 1 && !strcmp(argv[1], "--clock")) result = setFixtureClock();
        else result = checkFixture();
    }
    else
        puts("Missing graphics/intuition/cybergraphics/MUI runtime");
    if (MUIMasterBase) CloseLibrary(MUIMasterBase);
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    return result;
}
