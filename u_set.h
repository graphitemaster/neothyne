#ifndef U_SET_HDR
#define U_SET_HDR

#include "u_hash.h"
#include "u_buffer.h"
#include "u_pair.h"

namespace u {

template <typename K>
struct set {
    set();
    set(const set &other);
    ~set();

    set& operator=(const set& other);

    typedef hash_iterator<const hash_node<K, void>> const_iterator;
    typedef const_iterator iterator;

    iterator begin() const;
    iterator end() const;

    void clear();
    bool empty() const;
    size_t size() const;

    iterator find(const K &key) const;
    pair<iterator, bool> insert(const K &key);
    void erase(iterator where);
    size_t erase(const K &key);

    void swap(set &other);

private:
    typedef hash_node<K, void> *pointer;

    size_t m_size;
    buffer<pointer> m_buckets;
};

template <typename K>
set<K>::set()
    : m_size(0)
{
    m_buckets.resize(9, 0);
}

template <typename K>
set<K>::set(const set &other)
    : m_size(other.m_size)
{
    const size_t nbuckets = (size_t)(other.m_buckets.last - other.m_buckets.first);
    m_buckets.resize(9, 0);

    for (pointer* it = *other.m_buckets.first; it; it = it->next) {
        unsigned char *data = neoMalloc(sizeof(hash_node<K, void>));
        hash_node<K, void> *newnode = new(data) hash_node<K, void>(*it);
        newnode->next = newnode->prev = 0;
        hash_node_insert(newnode, hash(*it), m_buckets.first, nbuckets - 1);
    }
}

template <typename K>
set<K>::~set() {
    clear();
}

template <typename K>
set<K>& set<K>::operator=(const set<K> &other) {
    set<K>(other).swap(*this);
    return *this;
}

template <typename K>
inline typename set<K>::iterator set<K>::begin() const {
    iterator cit;
    cit.node = *m_buckets.first;
    return cit;
}

template <typename K>
inline typename set<K>::iterator set<K>::end() const {
    iterator cit;
    cit.node = 0;
    return cit;
}

template <typename K>
inline bool set<K>::empty() const {
    return m_size == 0;
}

template <typename K>
inline size_t set<K>::size() const {
    return m_size;
}

template <typename K>
inline void set<K>::clear() {
    pointer it = *m_buckets.first;
    while (it) {
        const pointer next = it->next;
        it->~hash_node<K, void>();
        neoFree(it);
        it = next;
    }
    m_buckets.last = m_buckets.first;
    m_buckets.resize(9, 0);
    m_size = 0;
}

template <typename K>
inline typename set<K>::iterator set<K>::find(const K &key) const {
    iterator result;
    result.node = hash_find(key, m_buckets.first, size_t(m_buckets.last - m_buckets.first));
    return result;
}

template <typename K>
inline pair<typename set<K>::iterator, bool> set<K>::insert(const K &key) {
    pair<iterator, bool> result(find(key), false);
    if (get<0>(result).node != 0)
        return result;

    unsigned char *data = neoMalloc(sizeof(hash_node<K, void>));
    hash_node<K, void>* newnode = new(data) hash_node<K, void>(key);
    newnode->next = newnode->prev = 0;

    const size_t nbuckets = size_t(m_buckets.last - m_buckets.first);
    hash_node_insert(newnode, hash(key), m_buckets.first, nbuckets - 1);

    ++m_size;
    if (m_size + 1 > 4 * nbuckets) {
        pointer root = *m_buckets.first;

        const size_t newnbuckets = (size_t(m_buckets.last - m_buckets.first) - 1) * 8;
        m_buckets.last = m_buckets.first;
        m_buckets.resize(newnbuckets + 1, 0);
        hash_node<K, void>* *buckets = m_buckets.first;

        while (root) {
            const pointer next = root->next;
            root->next = root->prev = 0;
            hash_node_insert(root, hash(root->first), buckets, newnbuckets);
            root = next;
        }
    }

    result.first.node = newnode;
    result.second = true;
    return result;
}

template <typename K>
inline void set<K>::erase(iterator where) {
    hash_node_erase(where.node, hash(where.node->first),
        m_buckets.first, size_t(m_buckets.last - m_buckets.first) - 1);

    where.node->~hash_node<K, void>();
    neoFree(where.node);
    --m_size;
}

template <typename K>
inline size_t set<K>::erase(const K &key) {
    const iterator it = find(key);
    if (it.node == 0)
        return 0;
    erase(it);
    return 1;
}

template <typename K>
void set<K>::swap(set &other) {
    size_t tsize = other.m_size;
    other.m_size = m_size;
    m_size = tsize;
    m_buckets.swap(other.m_buckets);
}

}
#endif
