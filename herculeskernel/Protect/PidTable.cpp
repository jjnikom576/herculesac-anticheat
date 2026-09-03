#include "PidTable.h"

namespace hac {

namespace {

    RTL_GENERIC_COMPARE_RESULTS NTAPI CompareRoutine(
        _In_ struct _RTL_GENERIC_TABLE* /*Table*/,
        _In_ PVOID FirstStruct,
        _In_ PVOID SecondStruct)
    {
        auto a = *static_cast<ULONG*>(FirstStruct);
        auto b = *static_cast<ULONG*>(SecondStruct);
        if (a < b) return GenericLessThan;
        if (a > b) return GenericGreaterThan;
        return GenericEqual;
    }

    PVOID NTAPI AllocateRoutine(
        _In_ struct _RTL_GENERIC_TABLE* /*Table*/,
        _In_ CLONG ByteSize)
    {
        return ExAllocatePool2(POOL_FLAG_NON_PAGED, ByteSize, 'PdiH');
    }

    VOID NTAPI FreeRoutine(
        _In_ struct _RTL_GENERIC_TABLE* /*Table*/,
        _In_ __drv_freesMem(Mem) _Post_invalid_ PVOID Buffer)
    {
        ExFreePoolWithTag(Buffer, 'PdiH');
    }

} // anonymous namespace

PidTable::PidTable()
{
    ExInitializeFastMutex(&m_mutex);
    RtlInitializeGenericTable(&m_table, CompareRoutine, AllocateRoutine, FreeRoutine, nullptr);
}

PidTable::~PidTable()
{
    ExAcquireFastMutex(&m_mutex);
    while (!RtlIsGenericTableEmpty(&m_table)) {
        PVOID elem = RtlGetElementGenericTable(&m_table, 0);
        if (elem) {
            RtlDeleteElementGenericTable(&m_table, elem);
        } else {
            break;
        }
    }
    ExReleaseFastMutex(&m_mutex);
}

NTSTATUS PidTable::Insert(ULONG pid)
{
    ExAcquireFastMutex(&m_mutex);
    BOOLEAN newElement = FALSE;
    PVOID inserted = RtlInsertElementGenericTable(&m_table, &pid, sizeof(pid), &newElement);
    ExReleaseFastMutex(&m_mutex);
    return inserted ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS PidTable::Remove(ULONG pid)
{
    ExAcquireFastMutex(&m_mutex);
    BOOLEAN removed = RtlDeleteElementGenericTable(&m_table, &pid);
    ExReleaseFastMutex(&m_mutex);
    return removed ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}

BOOLEAN PidTable::Contains(ULONG pid)
{
    ExAcquireFastMutex(&m_mutex);
    PVOID found = RtlLookupElementGenericTable(&m_table, &pid);
    ExReleaseFastMutex(&m_mutex);
    return found != nullptr;
}

ULONG PidTable::Count()
{
    ExAcquireFastMutex(&m_mutex);
    ULONG c = RtlNumberGenericTableElements(&m_table);
    ExReleaseFastMutex(&m_mutex);
    return c;
}

} // namespace hac
