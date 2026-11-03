#include "config.h"
#include <wtf/amigaos3/ThreadContext.h>

#include <stdint.h>
#include <wtf/Assertions.h>
#include <proto/exec.h>

namespace WTF {

// Use one Exec nesting level for an arbitrary number of target threads;
// Exec's own scheduling-disable count is only a signed byte.
static unsigned activeSnapshots;
static Task* snapshotOwner;

static void beginSnapshot()
{
    if (!activeSnapshots) {
        Forbid();
        snapshotOwner = FindTask(nullptr);
    }
    RELEASE_ASSERT(snapshotOwner == FindTask(nullptr));
    ++activeSnapshots;
}

static void endSnapshot()
{
    RELEASE_ASSERT(activeSnapshots && snapshotOwner == FindTask(nullptr));
    if (!--activeSnapshots) {
        snapshotOwner = nullptr;
        Permit();
    }
}

void* amigaOS3TaskUserData()
{
    return SysBase->ThisTask->tc_UserData;
}

void setAmigaOS3TaskUserData(void* data)
{
    SysBase->ThisTask->tc_UserData = data;
}

void AmigaOS3ThreadContext::initialize()
{
    RELEASE_ASSERT(!m_task);
    m_task = FindTask(nullptr);
#if defined(__HAVE_68881__)
    // The FPU rounds to extended precision by default and Exec starts each
    // task with that setting. JavaScript requires IEEE double results, so
    // round every operation to 53 bits, to nearest, with no traps.
    __asm__ volatile("fmove%.l %0,%!" : : "d"(0x80UL));
#endif
}

void AmigaOS3ThreadContext::didExit()
{
    Forbid();
    RELEASE_ASSERT(!m_suspendCount);
    RELEASE_ASSERT(m_task == FindTask(nullptr));
    m_task = nullptr;
    Permit();
}

bool AmigaOS3ThreadContext::suspend()
{
    beginSnapshot();
    if (!m_task) {
        endSnapshot();
        return false;
    }
    RELEASE_ASSERT(m_task != snapshotOwner);
    if (!m_suspendCount) {
        // Exec Switch saves PC, SR and D0-D7/A0-A6 on the task's user
        // stack before publishing tc_SPReg. Scanning that stack includes
        // all integer registers, even when preemption interrupted a leaf
        // function. Do not substitute a stale VM-entry stack pointer.
        m_stackPointer = m_task->tc_SPReg;
        uintptr_t top = reinterpret_cast<uintptr_t>(m_stackPointer);
        uintptr_t lower = reinterpret_cast<uintptr_t>(m_task->tc_SPLower);
        uintptr_t upper = reinterpret_cast<uintptr_t>(m_task->tc_SPUpper);
        RELEASE_ASSERT(!(top & 1) && top >= lower && top < upper);
        RELEASE_ASSERT(upper - top >= 66); // PC + SR + 15 registers.
    }
    ++m_suspendCount;
    return true;
}

void AmigaOS3ThreadContext::resume()
{
    RELEASE_ASSERT(m_suspendCount && snapshotOwner == FindTask(nullptr));
    if (!--m_suspendCount)
        m_stackPointer = nullptr;
    endSnapshot();
}

void* AmigaOS3ThreadContext::stackPointer() const
{
    RELEASE_ASSERT(m_suspendCount && snapshotOwner == FindTask(nullptr));
    return m_stackPointer;
}

}
