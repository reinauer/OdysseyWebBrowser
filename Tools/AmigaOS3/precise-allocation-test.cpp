/* Exercise production PreciseAllocation with every native malloc residue. */
#include "config.h"
#include "AlignedMemoryAllocator.h"
#include "CompleteSubspace.h"
#include "HeapCellInlines.h"
#include "HeapCellType.h"
#include "InitializeThreading.h"
#include "JSLock.h"
#include "PreciseAllocation.h"
#include "VM.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>

using namespace JSC;

static unsigned failures;
static unsigned checks;
static void check(bool passed)
{
    ++checks;
    if (!passed)
        ++failures;
}

class ResidueAllocator final : public AlignedMemoryAllocator {
public:
    unsigned nextResidue { 0 };
    bool failReallocation { false };
    unsigned allocations { 0 };
    unsigned frees { 0 };

    void* tryAllocateMemory(size_t size) final
    {
        check(!m_pointer && size + nextResidue <= sizeof(m_buffers[0]));
        memset(m_buffers[m_buffer], 0xcc, sizeof(m_buffers[0]));
        m_pointer = m_buffers[m_buffer] + nextResidue;
        m_size = size;
        memset(m_pointer, 0xa5, size);
        ++allocations;
        return m_pointer;
    }

    void* tryReallocateMemory(void* pointer, size_t size) final
    {
        check(pointer == m_pointer);
        checkBounds();
        if (failReallocation)
            return nullptr;
        check(size + nextResidue <= sizeof(m_buffers[0]));
        m_buffer ^= 1;
        memset(m_buffers[m_buffer], 0xcc, sizeof(m_buffers[0]));
        void* result = m_buffers[m_buffer] + nextResidue;
        memset(result, 0xa5, size);
        memcpy(result, pointer, std::min(size, m_size));
        m_pointer = result;
        m_size = size;
        return result;
    }

    void freeMemory(void* pointer) final
    {
        check(pointer == m_pointer);
        checkBounds();
        m_pointer = nullptr;
        ++frees;
    }

    void* tryAllocateAlignedMemory(size_t, size_t) final { RELEASE_ASSERT_NOT_REACHED(); }
    void freeAlignedMemory(void*) final { RELEASE_ASSERT_NOT_REACHED(); }
    void dump(WTF::PrintStream&) const final { }

private:
    void checkBounds()
    {
        auto* bytes = static_cast<unsigned char*>(m_pointer);
        for (unsigned i = 0; i < 16; ++i)
            check(bytes[m_size + i] == 0xcc);
    }

    alignas(16) unsigned char m_buffers[2][2048];
    unsigned m_buffer { 0 };
    void* m_pointer { nullptr };
    size_t m_size { 0 };
};

static void verify(PreciseAllocation* allocation, VM& vm, size_t size, size_t initialized)
{
    check(!(reinterpret_cast<uintptr_t>(allocation) & 15));
    check((reinterpret_cast<uintptr_t>(allocation->cell()) & 15) == 8);
    check(PreciseAllocation::fromCell(allocation->cell()) == allocation);
    check(allocation->cell()->isPreciseAllocation());
    check(&allocation->cell()->vm() == &vm);
    check(allocation->cellSize() == size);
    check(allocation->indexInSpace() == 0);
    auto* bytes = reinterpret_cast<unsigned char*>(allocation->cell());
    for (size_t i = 0; i < initialized; ++i)
        check(bytes[i] == static_cast<unsigned char>(i ^ 0x5a));
}

int main()
{
    JSC::initialize();
    // Like the shell, the test keeps its VM and registered subspace alive
    // through process exit. Individual precise allocations are all freed.
    VM& vm = VM::create(LargeHeap).leakRef();
    JSLockHolder lock(vm);
    auto* allocator = new ResidueAllocator;
    auto* type = new HeapCellType(CellAttributes(DoesNotNeedDestruction, HeapCell::Auxiliary));
    auto* subspace = new CompleteSubspace("alignment test", vm.heap, type, allocator);
    printf("Checking precise allocation alignment, growth, shrink, failure and reuse\n");
    fflush(stdout);
    for (unsigned initial = 0; initial < 16; initial += 4) {
        for (unsigned resized = 0; resized < 16; resized += 4) {
            allocator->nextResidue = initial;
            auto* allocation = PreciseAllocation::tryCreate(vm.heap, 128, subspace, 0);
            check(!!allocation);
            for (unsigned i = 0; i < 128; ++i)
                reinterpret_cast<unsigned char*>(allocation->cell())[i] = i ^ 0x5a;
            verify(allocation, vm, 128, 128);
            allocator->failReallocation = true;
            check(!allocation->tryReallocate(512, subspace));
            verify(allocation, vm, 128, 128);
            allocator->failReallocation = false;
            check(!allocation->tryReallocate(SIZE_MAX, subspace));
            verify(allocation, vm, 128, 128);
            allocator->nextResidue = resized;
            allocation = allocation->tryReallocate(512, subspace);
            verify(allocation, vm, 512, 128);
            allocator->nextResidue = initial;
            allocation = allocation->tryReallocate(64, subspace);
            verify(allocation, vm, 64, 64);
            allocation->destroy();
        }
        allocator->nextResidue = initial;
        auto* lower = PreciseAllocation::createForLowerTier(vm.heap, 128, subspace, 3);
        verify(lower, vm, 128, 0);
        auto* reused = lower->reuseForLowerTier();
        check(reused == lower && reused->isLowerTier());
        verify(reused, vm, 128, 0);
        reused->destroy();
    }
    unsigned beforeOverflow = allocator->allocations;
    check(!PreciseAllocation::tryCreate(vm.heap, SIZE_MAX, subspace, 0));
    check(allocator->allocations == beforeOverflow);
    check(allocator->allocations == allocator->frees);
    printf("Precise allocation: %u failures (%u checks)\n", failures, checks);
    return failures ? 20 : 0;
}
