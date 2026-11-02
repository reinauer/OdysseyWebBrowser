/* Record Exec free memory and a task's stack use, or hold memory back to
   simulate a smaller system. */
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Hold the largest blocks first, leaving the smaller blocks a system of this
   size would be likely to offer. Allocations must outlive this process. */
static int reserveLeaving(ULONG leave)
{
    ULONG available = AvailMem(MEMF_FAST);
    if (available <= leave) {
        printf("Cannot leave %lu bytes: free=%lu\n", leave, available);
        return 20;
    }
    for (ULONG remaining = available - leave; remaining;) {
        ULONG block = AvailMem(MEMF_FAST | MEMF_LARGEST) & ~(ULONG)(MEM_BLOCKSIZE - 1);
        if (block > remaining) block = remaining;
        if (!block || !AllocMem(block, MEMF_FAST | MEMF_REVERSE)) {
            printf("Allocation of %lu bytes failed\n", block);
            return 20;
        }
        remaining -= block;
    }
    printf("Reserved %lu bytes: free=%lu largest=%lu\n", available - leave,
        AvailMem(MEMF_FAST), AvailMem(MEMF_FAST | MEMF_LARGEST));
    return 0;
}

/* Exec does not clear stacks, but unused fast memory normally reads as zero,
   so the lowest nonzero long is the deepest the stack has reached. Old contents
   can only overstate the result. DOS keeps a few longs at the base of the stack. */
static void measureStack(const char* name, ULONG* used, ULONG* size)
{
    const ULONG* lower = NULL;
    const ULONG* upper = NULL;
    Forbid();
    struct Task* task = FindTask((CONST_STRPTR)name);
    if (task) {
        lower = (const ULONG*)(((ULONG)task->tc_SPLower + 3) & ~3UL);
        upper = (const ULONG*)task->tc_SPUpper;
    }
    Permit();
    if (!lower) return;
    const ULONG* deepest = lower + 16;
    while (deepest < upper && !*deepest) deepest++;
    *used = (ULONG)upper - (ULONG)deepest;
    *size = (ULONG)upper - (ULONG)lower;
}

/* Reopen for each line so a crashing guest cannot lose buffered samples. */
static int sample(const char* path, LONG ticks, const char* task)
{
    ULONG stackUsed = 0, stackSize = 0;
    SetTaskPri(FindTask(NULL), 1);
    for (ULONG count = 0; !(SetSignal(0, 0) & SIGBREAKF_CTRL_C); count++) {
        struct DateStamp now;
        char line[128];
        if (task && !(count % 10)) measureStack(task, &stackUsed, &stackSize);
        DateStamp(&now);
        int length = snprintf(line, sizeof line, "%ld %ld %ld %lu %lu %lu %lu %lu\n",
            now.ds_Days, now.ds_Minute, now.ds_Tick, AvailMem(MEMF_CHIP),
            AvailMem(MEMF_FAST), AvailMem(MEMF_FAST | MEMF_LARGEST), stackUsed, stackSize);
        BPTR output = Open((CONST_STRPTR)path, MODE_READWRITE);
        if (!output) return 20;
        Seek(output, 0, OFFSET_END);
        Write(output, line, length);
        Close(output);
        Delay(ticks);
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 3 && !strcmp(argv[1], "--leave"))
        return reserveLeaving(strtoul(argv[2], NULL, 10));
    if ((argc == 4 || argc == 5) && !strcmp(argv[1], "--sample"))
        return sample(argv[2], strtol(argv[3], NULL, 10), argc == 5 ? argv[4] : NULL);
    printf("Usage: %s --leave BYTES | --sample PATH TICKS [TASK]\n", argv[0]);
    return 20;
}
