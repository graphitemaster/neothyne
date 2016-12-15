#include "s_memory.h"

#include "u_set.h"

namespace s {

static constexpr size_t kMemoryBits = 7_z;
static constexpr size_t kPrime1 = 73_z;
static constexpr size_t kPrime2 = 5009_z;
static constexpr void *kTombstone = (void *)1;

size_t Memory::m_numBits;
size_t Memory::m_mask;
size_t Memory::m_capacity;
void **Memory::m_items;
size_t Memory::m_numItems;
size_t Memory::m_numDeletedItems;

void Memory::init() {
    m_numBits = 3u;
    m_capacity = size_t(1_z << m_numBits);
    m_mask = m_capacity - 1;
    m_items = (void **)neoCalloc(m_capacity, sizeof(void *));
    m_numItems = 0;
    m_numDeletedItems = 0;
}

void Memory::destroy() {
    for (size_t i = 0; i < m_capacity; i++) {
        void *data = m_items[i];
        if (data && data != kTombstone) {
            neoFree(data);
        }
    }
    neoFree(m_items);
}

U_MALLOC_LIKE void *Memory::allocate(size_t size) {
    void *data = neoMalloc(size);
    add(data);
    return data;
}

U_MALLOC_LIKE void *Memory::allocate(size_t count, size_t size) {
    void *data = neoCalloc(count, size);
    add(data);
    return data;
}

U_MALLOC_LIKE void *Memory::reallocate(void *old, size_t resize) {
    void *data = neoRealloc(old, resize);
    if (U_LIKELY(data != old)) {
        del(old);
        add(data);
    }
    return data;
}

void Memory::free(void *old) {
    del(old);
    neoFree(old);
}

void Memory::maybeRehash() {
    void **oldItems = nullptr;
    size_t oldCapacity = 0;
    if (m_numItems + m_numDeletedItems >= m_capacity * 0.85f) {
        oldItems = m_items;
        oldCapacity = m_capacity;
        m_numBits++;
        m_capacity = size_t(1_z << m_numBits);
        m_mask = m_capacity - 1;
        m_items = (void **)neoCalloc(m_capacity, sizeof(void*));
        m_numItems = 0;
        m_numDeletedItems = 0;
        for (size_t i = 0; i < oldCapacity; i++)
            addMember(oldItems[i]);
        neoFree(oldItems);
    }
}

bool Memory::addMember(void *member) {
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

bool Memory::add(void *member) {
    const bool result = addMember(member);
    maybeRehash();
    return result;
}

bool Memory::del(void *member) {
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
