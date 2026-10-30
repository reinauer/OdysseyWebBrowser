#ifndef ODYSSEY_AMIGAOS3_CLIPBOARD_NOTIFICATION_H
#define ODYSSEY_AMIGAOS3_CLIPBOARD_NOTIFICATION_H

#include <clib/alib_protos.h>
#include <devices/clipboard.h>
#include <proto/exec.h>
#include <utility/hooks.h>

// clipboard.device calls its hook in the writer's task, with Forbid active
// and its change semaphore held. Do not enter WebCore, MUI or an allocator
// there. The owner consumes the coalesced notification from its MUI input
// handler, including when a modal window runs a nested NewInput loop.
class AmigaOS3ClipboardNotification {
public:
    bool start()
    {
        if (m_owner)
            return false;
        LONG bit = AllocSignal(-1);
        if (bit < 0)
            return false;
        m_signalBit = bit;
        SetSignal(0, signalMask());
        m_pending = false;
        m_owner = FindTask(nullptr);
        return true;
    }

    void configureHook(Hook& hook)
    {
        hook.h_Entry = reinterpret_cast<HOOKFUNC>(HookEntry);
        hook.h_SubEntry = reinterpret_cast<HOOKFUNC>(changed);
        hook.h_Data = this;
    }

    ULONG signalMask() const { return m_signalBit < 0 ? 0 : 1UL << m_signalBit; }

    bool takePending()
    {
        bool pending = false;
        Forbid();
        if (m_owner && FindTask(nullptr) == m_owner) {
            pending = m_pending;
            m_pending = false;
            SetSignal(0, signalMask());
        }
        Permit();
        return pending;
    }

    // Called by the owner when removing the monitor. An already queued
    // signal is discarded and a late callback cannot signal a freed
    // (and possibly reused) signal bit. Forbid also balances a nested caller.
    void stop()
    {
        Forbid();
        if (m_owner && FindTask(nullptr) == m_owner) {
            m_owner = nullptr;
            m_pending = false;
            SetSignal(0, signalMask());
            FreeSignal(m_signalBit);
            m_signalBit = -1;
        }
        Permit();
    }

private:
    static ULONG changed(Hook* hook, void*, ClipHookMsg*)
    {
        auto& notification = *static_cast<AmigaOS3ClipboardNotification*>(hook->h_Data);
        Forbid();
        if (notification.m_owner) {
            notification.m_pending = true;
            Signal(notification.m_owner, notification.signalMask());
        }
        Permit();
        return 0;
    }

    Task* m_owner { nullptr };
    LONG m_signalBit { -1 };
    bool m_pending { false };
};

#endif
