#pragma once

#include <wtf/ExportMacros.h>

struct Task;

namespace WTF {

// Classic Exec is uniprocessor. A snapshot freezes task dispatch only while
// the caller copies roots. No allocation, waiting or locking is permitted
// between suspend() and resume(). Interrupts remain enabled.
class AmigaOS3ThreadContext {
public:
    WTF_EXPORT_PRIVATE void initialize();
    WTF_EXPORT_PRIVATE void didExit();
    WTF_EXPORT_PRIVATE bool suspend();
    WTF_EXPORT_PRIVATE void resume();
    WTF_EXPORT_PRIVATE void* stackPointer() const;

private:
    Task* m_task { nullptr };
    void* m_stackPointer { nullptr };
    unsigned m_suspendCount { 0 };
};

// The calling task's tc_UserData, which holds its WTF Thread once the thread
// is registered. The SDK pthread library passes thread IDs in pr_CIS and
// leaves tc_UserData to the task, so this avoids pthread_getspecific's
// semaphore and thread table search.
WTF_EXPORT_PRIVATE void* amigaOS3TaskUserData();
WTF_EXPORT_PRIVATE void setAmigaOS3TaskUserData(void*);

}
