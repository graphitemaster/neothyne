#ifndef U_LRU_HDR
#define U_LRU_HDR
#include <limits.h>

#include "u_map.h"

namespace u {

template <typename K>
struct lru {
    lru(size_t max = 128);
    ~lru();

    const K &insert(const K &data);

    const K &operator[](const K &key) const;
    K &operator[](const K &key);

    size_t size() const;

    K *find(const K &key);

protected:
    static constexpr size_t kWordBits = CHAR_BIT * sizeof(uint64_t);

    struct node {
        K data;
        size_t bit;
        node *prev;
        node *next;
        node(size_t bit, const K &data);
    };

    node *search(const K &key) const;
    void move_front(node *n);
    void remove(node *n);
    void insert_front(node *n);
    void remove_back();

    static size_t nodeIndex(size_t bit);
    static size_t nodeOffset(size_t bit);

    void nodeMark(size_t bit);
    void nodeClear(size_t bit);
    bool nodeTest(size_t bit);
    node *nodeNext(const K &data);

private:
    node *m_head;
    node *m_tail;
    map<K, node*> m_map;
    size_t m_size;
    size_t m_max;
    node *m_nodeData;
    uint64_t *m_nodeBits;
};

template <typename K>
inline lru<K>::node::node(size_t bit, const K &data)
    : data(data)
    , bit(bit)
    , prev(nullptr)
    , next(nullptr)
{
}

template <typename K>
inline lru<K>::lru(size_t max)
    : m_head(nullptr)
    , m_tail(nullptr)
    , m_size(0)
    , m_max(max)
    , m_nodeData(nullptr)
    , m_nodeBits(nullptr)
{
    size_t kNodeMemory = sizeof *m_nodeData * max;
    size_t kBitsMemory = sizeof *m_nodeBits * (max / (sizeof *m_nodeBits * CHAR_BIT) + 1);
    unsigned char *memory = neoMalloc(kNodeMemory + kBitsMemory);
    m_nodeData = (node *)memory;
    m_nodeBits = (uint64_t*)(memory + kNodeMemory);
}

template <typename K>
inline lru<K>::~lru() {
    for (node *current = m_head; current; ) {
        node *temp = current;
        current = current->next;
        temp->~node();
    }
    neoFree(m_nodeData); // will also free m_nodeBits
    m_size = 0;
}

template <typename K>
inline typename lru<K>::node *lru<K>::search(const K &key) const {
    const auto it = m_map.find(key);
    return it != m_map.end() ? it->second : nullptr;
}

template <typename K>
inline void lru<K>::move_front(node *n) {
    if (n == m_head)
        return;
    remove(n);
    insert_front(n);
}

template <typename K>
inline void lru<K>::remove(node *n) {
    if (n->prev)
        n->prev->next = n->next;
    else
        m_head = n->next;

    if (n->next)
        n->next->prev = n->prev;
    else
        m_tail = n->prev;

    m_size--;
}

template <typename K>
inline void lru<K>::insert_front(node *n) {
    if (m_head) {
        n->next = m_head;
        m_head->prev = n;
        n->prev = nullptr;
        m_head = n;
    } else {
        m_head = n;
        m_tail = n;
    }
    m_size++;
}

template <typename K>
inline void lru<K>::remove_back() {
    assert(m_tail);
    node *temp = m_tail;
    m_tail = m_tail->prev;
    if (m_tail) {
        m_tail->next = nullptr;
    } else {
        m_head = nullptr;
        m_tail = nullptr;
    }
    m_map.erase(m_map.find(temp->data));
    nodeClear(temp->bit);
    temp->~node();
    m_size--;
}

template <typename K>
inline size_t lru<K>::nodeIndex(size_t bit) {
    return bit / kWordBits;
}

template <typename K>
inline size_t lru<K>::nodeOffset(size_t bit) {
    return bit % kWordBits;
}

template <typename K>
inline void lru<K>::nodeMark(size_t bit) {
    m_nodeBits[nodeIndex(bit)] |= 1 << nodeOffset(bit);
}

template <typename K>
inline void lru<K>::nodeClear(size_t bit) {
    m_nodeBits[nodeIndex(bit)] &= ~(1 << nodeOffset(bit));
}

template <typename K>
inline bool lru<K>::nodeTest(size_t bit) {
    return m_nodeBits[nodeIndex(bit)] & (1 << nodeOffset(bit));
}

template <typename K>
inline typename lru<K>::node *lru<K>::nodeNext(const K &data) {
    for (size_t i = 0; i < m_max; i++) {
        if (nodeTest(i))
            continue;
        nodeMark(i);
        return new (&m_nodeData[i]) node(i, data);
    }
    // Ran out of nodes: evict the least recently used one
    remove_back();
    return nodeNext(data);
}

template <typename K>
inline const K &lru<K>::insert(const K &data) {
    const auto n = search(data);
    if (n) {
        n->data = data;
        move_front(n);
        return n->data;
    } else {
        node *next = nodeNext(data);
        insert_front(next);
        m_map.insert(u::make_pair(data, next));
        return next->data;
    }
}

template <typename K>
inline const K &lru<K>::operator[](const K &key) const {
    const auto n = m_map.find(key);
    move_front(n->second);
    return n->second->data;
}

template <typename K>
inline K &lru<K>::operator[](const K &key) {
    const auto n = m_map.find(key);
    move_front(n->second);
    return n->second->data;
}

template <typename K>
inline size_t lru<K>::size() const {
    return m_size;
}

template <typename K>
inline K *lru<K>::find(const K &key) {
    node *const n = search(key);
    if (n) {
        move_front(n);
        return &n->data;
    }
    return nullptr;
}

}

#endif
