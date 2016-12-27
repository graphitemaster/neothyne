#include "engine.h"

#include "s_memory.h"
#include "s_util.h"

#include "u_set.h"
#include "u_log.h"

#include "c_variable.h"

VAR(int, s_memory_max, "maximum scripting memory allowed in MiB", 64, 4096, 1024);
VAR(int, s_memory_dump, "dump active memory", 0, 1, 1);

namespace s {

static constexpr size_t kPrime1 = 73_z;
static constexpr size_t kPrime2 = 5009_z;
static constexpr uintptr_t kTombstone = 1u;

size_t Memory::m_numBits;
size_t Memory::m_mask;
size_t Memory::m_capacity;
uintptr_t *Memory::m_items;
size_t Memory::m_numItems;
size_t Memory::m_numDeletedItems;
size_t Memory::m_bytesAllocated;

[[noreturn]]
void Memory::oom(size_t requested) {
    const size_t totalSize = (size_t)s_memory_max*1024*1024;
    u::Log::err("[script] => \e[1m\e[31mOut of memory:\e[0m %s requested but only %s left (%s in use)\n",
        u::sizeMetric(requested),
        u::sizeMetric(totalSize >= m_bytesAllocated ? totalSize - m_bytesAllocated : 0_z),
        u::sizeMetric(m_bytesAllocated));
    neoFatal("Out of memory");
}

#define CHECK_OOM(SIZE) \
    do { \
        if (m_bytesAllocated + (SIZE) >= (size_t)s_memory_max*1024*1024) { \
            oom(SIZE); \
        } \
    } while (0)

void Memory::init() {
    m_numBits = 3_z;
    m_capacity = size_t(1_z << m_numBits);
    m_mask = m_capacity - 1;
    m_items = (uintptr_t *)neoCalloc(m_capacity, sizeof *m_items);
    m_numItems = 0;
    m_numDeletedItems = 0;
}

void Memory::destroy() {
    size_t allocations = 0;
    if (s_memory_dump)
        u::Log::out("[script] => active memory\n");
    for (size_t i = 0; i < m_capacity; i++) {
        uintptr_t address = m_items[i];
        if (address && address != kTombstone) {
            if (s_memory_dump) {
                Header *const actual = (Header *)address;
                dumpMemory(actual + 1, actual->size);
            }
            neoFree((void *)address);
            allocations++;
        }
    }
    u::Log::out("[script] => freed %s of active memory (from %zu allocations)\n",
        u::sizeMetric(m_bytesAllocated), allocations);
    neoFree(m_items);
}

U_MALLOC_LIKE void *Memory::allocate(size_t size) {
    CHECK_OOM(size);
    Header *data = (Header *)neoMalloc(size + sizeof *data);
    add((uintptr_t)data);
    data->size = size;
    m_bytesAllocated += size;
    return (void *)(data + 1);
}

U_MALLOC_LIKE void *Memory::allocate(size_t count, size_t size) {
    U_ASSERT(!(count && size > -1_z/count)); // overflow
    const size_t length = count * size;
    CHECK_OOM(length);
    // Note: we use neoCalloc here to take advantage of the zero-page
    // optimization, even though we write the size to the first page;
    // if the length > PAGE_SIZE then only the first page faults for
    // the header data
    Header *data = (Header *)neoCalloc(length + sizeof *data, 1);
    add((uintptr_t)data);
    data->size = length;
    m_bytesAllocated += length;
    return (void *)(data + 1);
}

U_MALLOC_LIKE void *Memory::reallocate(void *current, size_t size) {
    if (current) {
        // do the oom check inside since allocate for the other case handles itself
        CHECK_OOM(size);
        Header *const oldData = (Header *)current - 1;
        const size_t oldSize = oldData->size;
        const uintptr_t oldAddr = (uintptr_t)oldData;
        Header *const newData = (Header *)neoRealloc(oldData, size + sizeof *newData);
        const uintptr_t newAddr = (uintptr_t)newData;
        if (U_LIKELY(newAddr != oldAddr)) {
            del(oldAddr);
            add(newAddr);
        }
        // updates the size if it's an old block
        newData->size = size;
        // update the allocated memory
        m_bytesAllocated = m_bytesAllocated - oldSize + size;
        return (void *)(newData + 1);
    }
    // just allocate a new block
    return allocate(size);
}

void Memory::free(void *what) {
    if (what) {
        Header *const header = (Header *)what - 1;
        m_bytesAllocated -= header->size;
        del((uintptr_t)header);
        neoFree(header);
    }
}

void Memory::maybeRehash() {
    uintptr_t *oldItems = nullptr;
    size_t oldCapacity = 0;
    if (m_numItems + m_numDeletedItems >= m_capacity * 0.85f) {
        oldItems = m_items;
        oldCapacity = m_capacity;
        m_numBits++;
        m_capacity = size_t(1_z << m_numBits);
        m_mask = m_capacity - 1;
        m_items = (uintptr_t*)neoCalloc(m_capacity, sizeof *m_items);
        m_numItems = 0;
        m_numDeletedItems = 0;
        for (size_t i = 0; i < oldCapacity; i++)
            addMember(oldItems[i]);
        neoFree(oldItems);
    }
}

bool Memory::addMember(uintptr_t member) {
    const size_t value = (size_t)member;
    size_t index = m_mask & (kPrime1 * value);

    if (value == 0 || value == 1)
        return false;

    // linear probe
    while (m_items[index] && m_items[index] != kTombstone) {
        // already exists?
        if (m_items[index] == member) {
            return false;
        } else {
            index = m_mask & (index + kPrime2);
        }
    }
    m_numItems++;
    if (m_items[index] == kTombstone) {
        m_numDeletedItems--;
    }
    m_items[index] = member;
    return true;
}

bool Memory::add(uintptr_t member) {
    const bool result = addMember(member);
    maybeRehash();
    return result;
}

bool Memory::del(uintptr_t member) {
    const size_t value = (size_t)member;
    size_t index = m_mask & (kPrime1 * value);

    while (m_items[index]) {
        if (m_items[index] == member) {
            m_items[index] = kTombstone;
            m_numItems--;
            m_numDeletedItems++;
            return true;
        } else {
            index = m_mask & (index + kPrime2);
        }
    }

    return false;
}

}
