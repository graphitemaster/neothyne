#include "s_memory.h"

namespace s {

static constexpr size_t kMemoryBits = 7_z;
static constexpr size_t kPrime1 = 73_z;
static constexpr size_t kPrime2 = 5009_z;
static constexpr void *kTombstone = (void *)1;

void Memory::init(Memory *memory) {
    memory->m_numBits = 3u;
    memory->m_capacity = size_t(1_z << memory->m_numBits);
    memory->m_mask = memory->m_capacity - 1;
    memory->m_items = (void **)neoCalloc(memory->m_capacity, sizeof(void *));
    memory->m_numItems = 0;
    memory->m_numDeletedItems = 0;
}

void Memory::destroy(Memory *memory) {
    for (size_t i = 0; i < memory->m_capacity; i++) {
        void *data = memory->m_items[i];
        if (data && data != kTombstone) {
            neoFree(data);
        }
    }
    neoFree(memory->m_items);
}

U_MALLOC_LIKE void *Memory::allocate(Memory *memory, size_t size) {
    void *data = neoMalloc(size);
    add(memory, data);
    return data;
}

U_MALLOC_LIKE void *Memory::allocate(Memory *memory, size_t count, size_t size) {
    void *data = neoCalloc(count, size);
    add(memory, data);
    return data;
}

U_MALLOC_LIKE void *Memory::reallocate(Memory *memory, void *old, size_t resize) {
    void *data = neoRealloc(old, resize);
    if (U_LIKELY(data != old)) {
        del(memory, old);
        add(memory, data);
    }
    return data;
}

void Memory::free(Memory *memory, void *old) {
    neoFree(old);
    del(memory, old);
}

void Memory::maybeRehash(Memory *memory) {
    void **oldItems = nullptr;
    size_t oldCapacity = 0;
    if (memory->m_numItems + memory->m_numDeletedItems >= memory->m_capacity * 0.85f) {
        oldItems = memory->m_items;
        oldCapacity = memory->m_capacity;
        memory->m_numBits++;
        memory->m_capacity = size_t(1_z << memory->m_numBits);
        memory->m_mask = memory->m_capacity - 1;
        memory->m_items = (void **)neoCalloc(memory->m_capacity, sizeof(void*));
        memory->m_numItems = 0;
        memory->m_numDeletedItems = 0;
        for (size_t i = 0; i < oldCapacity; i++)
            addMember(memory, oldItems[i]);
        neoFree(oldItems);
    }
}

bool Memory::addMember(Memory *memory, void *member) {
    const size_t value = (size_t)member;
    size_t index = memory->m_mask & (kPrime1 * value);

    if (value == 0 || value == 1)
        return false;

    // linear probe
    while (memory->m_items[index] && memory->m_items[index] != kTombstone) {
        // already exists?
        if (memory->m_items[index] == member) {
            return false;
        } else {
            index = memory->m_mask & (index + kPrime2);
        }
    }
    memory->m_numItems++;
    if (memory->m_items[index] == kTombstone) {
        memory->m_numDeletedItems--;
    }
    memory->m_items[index] = member;
    return true;
}

bool Memory::add(Memory *memory, void *member) {
    const bool result = addMember(memory, member);
    maybeRehash(memory);
    return result;
}

bool Memory::del(Memory *memory, void *member) {
    const size_t value = (size_t)member;
    size_t index = memory->m_mask & (kPrime1 * value);

    while (memory->m_items[index]) {
        if (memory->m_items[index] == member) {
            memory->m_items[index] = kTombstone;
            memory->m_numItems--;
            memory->m_numDeletedItems++;
            return true;
        } else {
            index = memory->m_mask & (index + kPrime2);
        }
    }

    return false;
}

}
