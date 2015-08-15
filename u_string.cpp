#define __STDC_LIMIT_MACROS
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#ifndef NDEBUG
#include <stdio.h>
#endif

#include "u_algorithm.h"
#include "u_string.h"
#include "u_hash.h"
#include "u_new.h"
#include "u_memory.h"
#include "u_misc.h"

namespace u {

///! stringMemory
struct stringMemory {
    static constexpr size_t kMemorySize = 32 << 20; // 32MB pool
    static constexpr size_t kMinChunkSize = 0x20; // 32 byte smallest chunk

    stringMemory();
    ~stringMemory();

    char *allocate(size_t size);
    void deallocate(char *ptr);
    char *reallocate(char *ptr, size_t size);

    void print();
protected:
    struct region {
        void setSize(uint32_t size);
        void setFree(bool free);
        void resize();
        uint32_t size() const;
        bool free() const;
    private:
        int32_t m_store; ///< Sign bit indicates if region is free or not, stores
                         ///  the size of the given region in both cases.
    };

    static region *nextRegion(region *reg);
    region *findAvailable(size_t size);
    bool mergeFree();
    region *divideRegion(region *reg, size_t size);
    size_t getSize(size_t size);

private:
    region *m_head;
    region *m_tail;
};

inline void stringMemory::region::setSize(uint32_t size) {
    assert(size < INT32_MAX);
    const bool used = free();
    m_store = int32_t(size);
    setFree(used);
}

inline void stringMemory::region::setFree(bool free) {
    m_store = free ? size() : -int32_t(size());
}

inline uint32_t stringMemory::region::size() const {
    // Remove sign bit
    const uint32_t mask = m_store >> ((sizeof(uint32_t) * CHAR_BIT) - 1);
    return (m_store + mask) ^ mask;
}

inline bool stringMemory::region::free() const {
    return m_store > 0;
}

inline void stringMemory::region::resize() {
    setSize(size() * 2);
}

inline stringMemory::stringMemory()
    : m_head(neoMalloc(kMemorySize))
{
    memset(m_head, 0, kMemorySize);
    m_head->setSize(kMemorySize);
    m_head->setFree(true);
    m_tail = nextRegion(m_head);
}

inline stringMemory::~stringMemory() {
#ifndef NDEBUG
    print();
#endif
    neoFree(m_head);
}

char *stringMemory::allocate(size_t size) {
    assert(m_head);
    if (size == 0)
        return allocate(1);
    const size_t totalSize = getSize(size);
    region *available = findAvailable(totalSize);
    if (available) {
        available->setFree(false);
        return (char *)(available + 1);
    } else {
        // Merge free blocks until they're all merged
        while (mergeFree())
            ;
        // Now try and find a free block
        available = findAvailable(totalSize);
        // Got a free block
        if (available) {
            available->setFree(false);
            return (char *)(available + 1);
        }
    }
    abort();
}

inline void stringMemory::deallocate(char *ptr) {
    if (!ptr)
        return;
    // Mark the block as free
    region *freeRegion = ((region *)ptr) - 1;
    assert(freeRegion >= m_head);
    assert(freeRegion <= (m_tail - 1));
    freeRegion->setFree(true);
}

char *stringMemory::reallocate(char *ptr, size_t size) {
    if (!ptr)
        return allocate(size);

    region *reg = ((region *)ptr) - 1;

    assert(reg >= m_head);
    assert(reg <= (m_tail - 1));

    if (size == 0) {
        deallocate(ptr);
        return nullptr;
    }

    const size_t totalSize = getSize(size);
    if (totalSize <= reg->size())
        return ptr;

    char *block = allocate(size);
    memcpy(block, ptr, reg->size() - sizeof(region));
    deallocate(ptr);

    return block;
}

inline stringMemory::region *stringMemory::nextRegion(region *reg) {
    return (region *)(((unsigned char *)reg) + reg->size());
}

// Finds a free block that matches the size passed in.
// If it cannot find one of that size, but there is a larger free block, then
// it divides the larger block into smaller ones. If there is no larger block
// then it returns nullptr.
//
// Merges adjacent free blocks as it searches the list.
stringMemory::region *stringMemory::findAvailable(size_t size) {
    assert(m_head);

    region *reg = m_head;
    region *buddy = nextRegion(reg);
    region *closest = nullptr;

    if (buddy == m_tail && reg->free())
        return divideRegion(reg, size);

    // Find minimum-sized match within the area of memory available
    while (reg < m_tail && buddy < m_tail) {
        // When both the region and buddy are free
        if (reg->free() && buddy->free() && reg->size() == buddy->size()) {
            reg->resize();
            if (size <= reg->size() && (!closest || reg->size() <= closest->size()))
                closest = reg;
            reg = nextRegion(buddy);
            if (reg < m_tail)
                buddy = nextRegion(reg);
        } else {
            if (reg->free() && size <= reg->size() && (!closest || reg->size() <= closest->size()))
                closest = reg;
            if (buddy->free() && size <= buddy->size() && (!closest || buddy->size() <= closest->size()))
                closest = buddy;
            // Buddy has been split up into smaller chunks
            if (reg->size() > buddy->size()) {
                reg = buddy;
                buddy = nextRegion(buddy);
            } else {
                // Jump ahead two regions
                reg = nextRegion(buddy);
                if (reg < m_tail)
                    buddy = nextRegion(reg);
            }
        }
    }

    if (closest) {
        if (closest->size() == size)
            return closest;
        return divideRegion(closest, size);
    }
    return nullptr;
}

// Single level merge of adjacent free blocks
bool stringMemory::mergeFree() {
    assert(m_head);

    region *reg = m_head;
    region *buddy = nextRegion(reg);
    bool modified = false;

    while (reg < m_tail && buddy < m_tail) {
        // Both region and buddy are free
        if (reg->free() && buddy->free() && reg->size() == buddy->size()) {
            reg->resize();
            reg = nextRegion(buddy);
            if (reg < m_tail)
                buddy = nextRegion(reg);
            modified = true;
        } else if (reg->size() > buddy->size()) {
            // Buddy has been split up into smaller chunks
            reg = buddy;
            buddy = nextRegion(buddy);
        } else {
            // Jump ahead two regions
            reg = nextRegion(buddy);
            if (reg < m_tail)
                buddy = nextRegion(reg);
        }
    }
    return modified;
}

// Given a region of free memory and a size, split it in half repeatedly until the
// desired size is reached and return a pointer to that new free region.
inline stringMemory::region *stringMemory::divideRegion(region *reg, size_t size) {
    assert(reg);

    while (reg->size() > size) {
        const size_t regionSize = reg->size() / 2;
        reg->setSize(regionSize);
        reg = nextRegion(reg);
        reg->setSize(regionSize);
        reg->setFree(true);
    }
    return reg;
}

inline size_t stringMemory::getSize(size_t size) {
    size += sizeof(region);
    size_t totalSize = kMinChunkSize;
    while (size > totalSize)
        totalSize *= 2;
    return totalSize;
}

void stringMemory::print() {
    auto escapeString = [](const char *str) {
        size_t length = strlen(str);
        // The longest possible sequence would be a string with "\x[0-F][0-F]"
        // for all, which is 4x as large as the input sequence, we also need + 1
        // for null terminator.
        u::unique_ptr<char> data(new char[length * 4 + 1]);
        char *put = data.get();
        for (const char *s = str; *s; s++) {
            unsigned char ch = *s;
            if (ch >= ' ' && ch <= '~' && ch != '\\' && ch != '"')
                *put++ = ch;
            else {
                *put++ = '\\';
                switch (ch) {
                case '"':  *put++ = '"';  break;
                case '\\': *put++ = '\\'; break;
                case '\t': *put++ = 't';  break;
                case '\r': *put++ = 'r';  break;
                case '\n': *put++ = 'n';  break;
                default:
                    *put++ = '\\';
                    *put++ = 'x';
                    *put++ = "0123456789ABCDEF"[ch >> 4];
                    *put++ = "0123456789ABCDEF"[ch & 0xF];
                    break;
                }
            }
        }
        *put++ = '\0';
        return data;
    };

    for (region *reg = m_head; reg < m_tail; reg = nextRegion(reg)) {
        if (reg->free()) {
            printf("Free (%p) [ size: %" PRIu32 " ]\n", (void *)reg, reg->size());
        } else {
            auto escape = escapeString((const char *)(reg + 1));
            printf("Used (%p) [ size: %" PRIu32 " contents: \"%.50s...\" ]\n",
                (void *)reg,
                reg->size(),
                escape.get()
            );
        }
    }
}

// We utilize deferred data here due to C++ lacking any sort of primitives for
// specifying initialization order of statics.
static deferred_data<stringMemory, false> gStringMemory;

#define STR_MALLOC(SIZE) \
    gStringMemory()->allocate((SIZE))

#define STR_REALLOC(PTR, SIZE) \
    gStringMemory()->reallocate((PTR), (SIZE))

#define STR_FREE(PTR) \
    gStringMemory()->deallocate((PTR))

///! string
string::string()
    : m_first(nullptr)
    , m_last(nullptr)
    , m_capacity(nullptr)
{
}

string::string(const string &other)
    : m_first(nullptr)
    , m_last(nullptr)
    , m_capacity(nullptr)
{
    reserve(other.size());
    append(other.m_first, other.m_last);
}

string::string(string &&other)
    : m_first(other.m_first)
    , m_last(other.m_last)
    , m_capacity(other.m_capacity)
{
    other.m_first = nullptr;
    other.m_last = nullptr;
    other.m_capacity = nullptr;
}

string::string(const char* sz)
    : m_first(nullptr)
    , m_last(nullptr)
    , m_capacity(nullptr)
{
    const size_t len = strlen(sz);
    reserve(len);
    append(sz, sz + len);
}

string::string(const char *sz, size_t len)
    : m_first(nullptr)
    , m_last(nullptr)
    , m_capacity(nullptr)
{
    reserve(len);
    append(sz, sz + len);
}

string::~string() {
    STR_FREE(m_first);
}

string &string::operator=(const string &other) {
    string(other).swap(*this);
    return *this;
}

string &string::operator=(string &&other) {
    assert(this != &other);
    m_first = other.m_first;
    m_last = other.m_last;
    m_capacity = other.m_capacity;
    other.m_first = nullptr;
    other.m_last = nullptr;
    other.m_capacity = nullptr;
    return *this;
}

const char *string::c_str() const {
    return m_first;
}

size_t string::size() const {
    return m_last - m_first;
}

bool string::empty() const {
    return m_last - m_first == 0;
}

char *string::copy() const {
    const size_t length = size() + 1;
    return (char *)memcpy(STR_MALLOC(length), m_first, length);
}

void string::reserve(size_t capacity) {
    if (m_first + capacity + 1 <= m_capacity)
        return;

    const size_t size = m_last - m_first;

    char *newfirst = STR_REALLOC(m_first, capacity + 1);
    m_first = newfirst;
    m_last = newfirst + size;
    m_capacity = m_first + capacity;
}

void string::resize(size_t size) {
    reserve(size);
    for (char *it = m_last, *end = m_first + size + 1; it < end; ++it)
        *it = '\0';
    m_last += size;
}

string &string::append(const char *first, const char *last) {
    const size_t newsize = m_last - m_first + last - first + 1;
    if (m_first + newsize > m_capacity)
        reserve((newsize * 3) / 2);

    assert(m_first);
    assert(m_last);

    for (; first != last; ++m_last, ++first)
        *m_last = *first;
    *m_last = '\0';
    return *this;
}

string &string::append(const char *str, size_t len) {
    return append(str, str + len);
}

string &string::append(const char *str) {
    return append(str, strlen(str));
}

string &string::append(const char ch) {
    return append(&ch, 1);
}

string &string::replace_all(const char *before, const char *after) {
    const size_t beforeLength = strlen(before);
    string result;
    const_iterator end_ = end();
    const_iterator current = begin();
    const_iterator next = search(current, end_, before, before + beforeLength);
    while (next != end_) {
        result.append(current, next);
        result.append(after);
        current = next + beforeLength;
        next = search(current, end_, before, before + beforeLength);
    }
    result.append(current, next);
    swap(result);
    return *this;
}

char string::pop_back() {
    if (m_last == m_first)
        return *m_first;
    m_last--;
    char last = *m_last;
    *m_last = '\0';
    return last;
}

char string::pop_front() {
    char front = *m_first;
    erase(0, 1);
    return front;
}

string::iterator string::begin() {
    return m_first;
}

string::iterator string::end() {
    return m_last;
}

string::const_iterator string::begin() const {
    return m_first;
}

string::const_iterator string::end() const {
    return m_last;
}

char &string::operator[](size_t index) {
    return m_first[index];
}

const char &string::operator[](size_t index) const {
    return m_first[index];
}

size_t string::find(char ch) const {
    char *search = strchr(m_first, ch);
    return search ? search - m_first : npos;
}

void string::erase(size_t beg, size_t end) {
    char *mbeg = m_first + beg;
    char *mend = m_first + end;
    const size_t len = m_last - mend;
    moveMemory(mbeg, mend, len);
    m_last = &mbeg[len];
    m_last[0] = '\0';
}

void string::swap(string &other) {
    u::swap(m_first, other.m_first);
    u::swap(m_last, other.m_last);
    u::swap(m_capacity, other.m_capacity);
}

void string::reset() {
    m_last = m_first;
    *m_first = '\0';
    m_capacity = m_first;
}

size_t hash(const string &str) {
    return detail::fnv1a(str.c_str(), str.size());
}

}
