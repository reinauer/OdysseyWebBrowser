// Exercise the browser's actual register-argument dispatcher/hook macros.
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <stdio.h>

#include "amigaos3funcs.h"
#include "include/macros/vapor.h"

namespace {
constexpr ULONG probeMethod = 0xfecd0001;
constexpr ULONG probeValue = 0x12345678;
struct Data { ULONG value; };
struct ProbeMessage { ULONG MethodID; ULONG value; };
static unsigned constructed;
static unsigned disposed;
static Object* expectedObject;

BEGINMTABLE
case OM_NEW: {
    Object* created = DoSuperNew(cl, obj, TAG_MORE,
        reinterpret_cast<opSet*>(msg)->ops_AttrList, TAG_DONE);
    if (created) {
        static_cast<Data*>(INST_DATA(cl, created))->value = probeValue;
        ++constructed;
    }
    return reinterpret_cast<ULONG>(created);
}
case OM_DISPOSE:
    ++disposed;
    break;
case probeMethod: {
    auto* probe = reinterpret_cast<ProbeMessage*>(msg);
    auto* data = static_cast<Data*>(INST_DATA(cl, obj));
    return obj == expectedObject && data->value == probeValue
        ? probe->value : 0;
}
ENDMTABLE

MUI_HOOK(probe, Object* object, ULONG* arguments)
{
    return h == &probe_hook && object == expectedObject
        && arguments[0] == probeValue ? 0x76543210 : 0;
}

static ULONG standardHook(Hook* hook, Object* object, ULONG* arguments)
{
    return hook->h_Data == &expectedObject && object == expectedObject
        && arguments[0] == probeValue ? 0x13572468 : 0;
}

static Hook trampolineHook = { { nullptr, nullptr },
    reinterpret_cast<HOOKFUNC>(HookEntry),
    reinterpret_cast<HOOKFUNC>(standardHook), &expectedObject };
}

unsigned checkAmigaOS3MUICallbacks()
{
    unsigned failures = 0;
    auto* custom = MUI_CreateCustomClass(nullptr, MUIC_Area, nullptr,
        sizeof(Data), reinterpret_cast<APTR>(DISPATCHERREF));
    if (!custom) {
        puts("FAIL MUI custom class creation");
        return 1;
    }
    expectedObject = static_cast<Object*>(NewObject(custom->mcc_Class, nullptr,
        MUIA_UserData, probeValue, TAG_DONE));
    if (!expectedObject)
        ++failures;
    else {
        ULONG value = 0;
        if (!GetAttr(MUIA_UserData, expectedObject, &value) || value != probeValue)
            ++failures;
        SetAttrs(expectedObject, MUIA_PointerType, MUIV_PointerType_Link, TAG_DONE);
        if (!GetAttr(MUIA_PointerType, expectedObject, &value)
            || value != MUIV_PointerType_Link
            || DoMethod(expectedObject, MUIM_WhichPointerType, 0, 0) != MUIV_PointerType_Link)
            ++failures;
        SetAttrs(expectedObject, MUIA_PointerType, MUIV_PointerType_Normal, TAG_DONE);
        if (DoMethod(expectedObject, MUIM_WhichPointerType, 0, 0) != MUIV_PointerType_Normal)
            ++failures;
        if (DoMethod(expectedObject, probeMethod, probeValue) != probeValue)
            ++failures;
        if (DoMethod(expectedObject, MUIM_CallHook, &probe_hook, probeValue) != 0x76543210)
            ++failures;
        if (DoMethod(expectedObject, MUIM_CallHook, &trampolineHook, probeValue) != 0x13572468)
            ++failures;
        MUI_DisposeObject(expectedObject);
    }
    expectedObject = nullptr;
    MUI_DeleteCustomClass(custom);
    if (constructed != 1 || disposed != 1)
        ++failures;
    printf("MUI dispatcher/hook callbacks: %u failures\n", failures);
    return failures;
}
