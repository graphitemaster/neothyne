#include "s_memory.h"

#include "u_set.h"

namespace s {

static constexpr size_t kMemoryBits = 7_z;
static constexpr size_t kPrime1 = 73_z;
static constexpr size_t kPrime2 = 5009_z;
static constexpr uintptr_t kTombstone = 1u;

size_t Memory::m_numBits;
size_t Memory::m_mask;
size_t Memory::m_capacity;
uintptr_t *Memory::m_items;
size_t Memory::m_numItems;
size_t Memory::m_numDeletedItems;

void Memory::init() {
    m_numBits = 3u;
    m_capacity = size_t(1_z << m_numBits);
    m_mask = m_capacity - 1;
    m_items = (uintptr_t *)neoCalloc(m_capacity, sizeof *m_items);
    m_numItems = 0;
    m_numDeletedItems = 0;
}

void Memory::destroy() {
    for (size_t i = 0; i < m_capacity; i++) {
        uintptr_t address = m_items[i];
        if (address && address != kTombstone) {
            neoFree((void *)address);
        }
    }
    neoFree(m_items);
}

U_MALLOC_LIKE void *Memory::allocate(size_t size) {
    void *data = neoMalloc(size);
    add((uintptr_t)data);
    return data;
}

U_MALLOC_LIKE void *Memory::allocate(size_t count, size_t size) {
    void *data = neoCalloc(count, size);
    add((uintptr_t)data);
    return data;
}

U_MALLOC_LIKE void *Memory::reallocate(void *old, size_t resize) {
    uintptr_t oldaddr = (uintptr_t)old;
    uintptr_t newaddr = (uintptr_t)neoRealloc(old, resize);
    if (U_LIKELY(newaddr != oldaddr)) {
        del(oldaddr);
        add(newaddr);
    }
    return (void*)newaddr;
}

void Memory::free(void *old) {
    del((uintptr_t)old);
    neoFree(old);
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
