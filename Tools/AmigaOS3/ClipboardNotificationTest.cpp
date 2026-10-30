// Exercise the production device hook, signal lifetime and MUI dispatch.
#include <libraries/mui.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <pthread.h>
#include <stdio.h>
#include "amigaos3funcs.h"
#include "include/macros/vapor.h"
#include "AmigaOS3ClipboardNotification.h"

namespace {
constexpr ULONG notificationMethod = 0xfecd0002;
AmigaOS3ClipboardNotification notification;
Task* owner;
unsigned deliveries;
unsigned wrongTask;

BEGINMTABLE
case notificationMethod:
    if (FindTask(nullptr) != owner)
        ++wrongTask;
    if (notification.takePending())
        ++deliveries;
    return 0;
ENDMTABLE

IOClipReq* openClipboard()
{
    auto* port = CreateMsgPort();
    if (!port)
        return nullptr;
    auto* request = reinterpret_cast<IOClipReq*>(CreateIORequest(port, sizeof(IOClipReq)));
    if (request && !OpenDevice("clipboard.device", 0, reinterpret_cast<IORequest*>(request), 0))
        return request;
    if (request)
        DeleteIORequest(reinterpret_cast<IORequest*>(request));
    DeleteMsgPort(port);
    return nullptr;
}

void closeClipboard(IOClipReq* request)
{
    auto* port = request->io_Message.mn_ReplyPort;
    CloseDevice(reinterpret_cast<IORequest*>(request));
    DeleteIORequest(reinterpret_cast<IORequest*>(request));
    DeleteMsgPort(port);
}

struct Writer { unsigned iterations; unsigned failures; };
void* writeClipboard(void* argument)
{
    auto& writer = *static_cast<Writer*>(argument);
    auto* request = openClipboard();
    if (!request) {
        ++writer.failures;
        return nullptr;
    }
    // Empty but complete FORM FTXT; the disposable guest owns this clipboard.
    unsigned char form[] = { 'F', 'O', 'R', 'M', 0, 0, 0, 4, 'F', 'T', 'X', 'T' };
    for (unsigned i = 0; i < writer.iterations; ++i) {
        request->io_ClipID = 0;
        request->io_Offset = 0;
        request->io_Data = reinterpret_cast<char*>(form);
        request->io_Length = sizeof(form);
        request->io_Command = CMD_WRITE;
        if (DoIO(reinterpret_cast<IORequest*>(request)))
            ++writer.failures;
        request->io_Command = CMD_UPDATE;
        if (DoIO(reinterpret_cast<IORequest*>(request)))
            ++writer.failures;
        // A foreign task must not consume the browser's pending state.
        if (FindTask(nullptr) != owner && notification.takePending())
            ++writer.failures;
    }
    closeClipboard(request);
    return nullptr;
}
}

unsigned checkAmigaOS3ClipboardNotifications()
{
    unsigned failures = 0;
#define CHECK_CLIP(condition) do { if (!(condition)) { \
    printf("FAIL clipboard notification line %u: %s\n", __LINE__, #condition); ++failures; } } while (0)
    owner = FindTask(nullptr);
    deliveries = wrongTask = 0;
    auto* custom = MUI_CreateCustomClass(nullptr, MUIC_Area, nullptr, 0,
        reinterpret_cast<APTR>(DISPATCHERREF));
    CHECK_CLIP(custom);
    if (!custom)
        return failures;
    auto* target = static_cast<Object*>(NewObject(custom->mcc_Class, nullptr, TAG_DONE));
    auto* application = MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, "Odyssey clipboard notification test",
        MUIA_Application_UseCommodities, FALSE,
        MUIA_Application_UseRexx, FALSE, TAG_DONE);
    CHECK_CLIP(target && application);
    auto* request = openClipboard();
    CHECK_CLIP(request);
    bool started = notification.start();
    CHECK_CLIP(started);
    if (target && application && request && started) {
        Hook hook = { };
        notification.configureHook(hook);
        MUI_InputHandlerNode input = { };
        input.ihn_Object = target;
        input.ihn_Method = notificationMethod;
        input.ihn_Signals = notification.signalMask();
        DoMethod(application, MUIM_Application_AddInputHandler, &input);
        request->io_Data = reinterpret_cast<char*>(&hook);
        request->io_Length = 1;
        request->io_Command = CBD_CHANGEHOOK;
        bool installed = !DoIO(reinterpret_cast<IORequest*>(request));
        CHECK_CLIP(installed);
        if (installed) {
            for (unsigned round = 0; round < 2; ++round) {
                Writer writer = { 2, 0 };
                pthread_t thread;
                int created = pthread_create(&thread, nullptr, writeClipboard, &writer);
                CHECK_CLIP(!created);
                if (!created)
                    CHECK_CLIP(!pthread_join(thread, nullptr));
                CHECK_CLIP(!writer.failures);
                CHECK_CLIP(deliveries == round); // Not delivered in writer/hook.
                ULONG signals = SetSignal(0, input.ihn_Signals) & input.ihn_Signals;
                CHECK_CLIP(signals == input.ihn_Signals);
                DoMethod(application, MUIM_Application_NewInput, &signals);
                CHECK_CLIP(signals & input.ihn_Signals); // Included in Wait mask.
                CHECK_CLIP(deliveries == round + 1); // Two writes coalesced.
                CHECK_CLIP(!notification.takePending());
            }
            Writer localWriter = { 1, 0 };
            writeClipboard(&localWriter);
            CHECK_CLIP(!localWriter.failures);
            // Local copy drains after device I/O and before caching rich data.
            CHECK_CLIP(notification.takePending());
            ULONG signals = input.ihn_Signals;
            DoMethod(application, MUIM_Application_NewInput, &signals);
            CHECK_CLIP(deliveries == 2);
            request->io_Length = 0;
            CHECK_CLIP(!DoIO(reinterpret_cast<IORequest*>(request)));
        }
        CHECK_CLIP(!wrongTask);
        DoMethod(application, MUIM_Application_RemInputHandler, &input);
        ULONG oldMask = notification.signalMask();
        unsigned oldBit = 0;
        while (!(oldMask & (1UL << oldBit)))
            ++oldBit;
        notification.stop();
        CHECK_CLIP(AllocSignal(oldBit) == static_cast<LONG>(oldBit));
        SetSignal(0, oldMask);
        // An inactive hook cannot signal a recycled bit, even under Forbid.
        Forbid();
        CallHookA(&hook, nullptr, nullptr);
        Permit();
        CHECK_CLIP(!(SetSignal(0, oldMask) & oldMask));
        FreeSignal(oldBit);
        CHECK_CLIP(!notification.takePending());
        CHECK_CLIP(notification.start());
        notification.stop();
        // Allocation failure must not publish an owner or leave pending state.
        LONG bits[32];
        unsigned count = 0;
        LONG bit;
        while (count < 32 && (bit = AllocSignal(-1)) >= 0)
            bits[count++] = bit;
        CHECK_CLIP(!notification.start());
        for (unsigned i = 0; i < count; ++i)
            FreeSignal(bits[i]);
        CHECK_CLIP(notification.start());
        notification.stop();
    } else if (started)
        notification.stop();
    if (request)
        closeClipboard(request);
    if (application)
        MUI_DisposeObject(application);
    if (target)
        MUI_DisposeObject(target);
    MUI_DeleteCustomClass(custom);
    printf("Clipboard foreign-task notification and MUI dispatch: %u failures\n", failures);
    return failures;
#undef CHECK_CLIP
}
