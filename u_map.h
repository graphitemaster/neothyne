#ifndef U_MAP_HDR
#define U_MAP_HDR

#include "u_hash.h"
#include "u_pair.h"
#include "u_buffer.h"

namespace u {

template <typename K, typename V>
class map {
public:
    map();
    map(const map &other);
    ~map();

    map& operator=(const map &other);


    typedef pair<K, V> value_type;

    typedef hash_iterator<const hash_node<K, V>> const_iterator;
    typedef hash_iterator<hash_node<K, V>> iterator;

    iterator begin();
    iterator end();

    const_iterator begin() const;
    const_iterator end() const;

    void clear();
    bool empty() const;
    size_t size() const;

    const_iterator find(const K &key) const;
    iterator find(const K &key);
    pair<iterator, bool> insert(const pair<K, V> &p);
    void erase(const_iterator where);

    V& operator[](const K &key);

    void swap(map &other);

private:
    typedef hash_node<K, V> *pointer;
    size_t m_size;
    buffer<pointer> m_buckets;
};

template <typename K, typename V>
map<K, V>::map()
    : m_size(0)
{
    m_buckets.resize(9, 0);
}

template <typename K, typename V>
map<K, V>::map(const map& other)
    : m_size(other.m_size)
{
    const size_t nbuckets = size_t(other.m_buckets.last - other.m_buckets.first);
    m_buckets.resize(nbuckets, 0);

    for (pointer it = *other.m_buckets.first; it; it = it->next) {
        unsigned char *data = (unsigned char *)malloc(sizeof(hash_node<K, V>));
        hash_node<K, V>* newnode = new(data) hash_node<K, V>(it->first, it->second);
        newnode->next = 0;
        newnode->prev = 0;
        hash_node_insert(newnode, hash(it->first), m_buckets.first, nbuckets - 1);
    }
}

template <typename K, typename V>
map<K, V>::~map() {
    clear();
}

template <typename K, typename V>
map<K, V> &map<K, V>::operator=(const map<K, V> &other) {
    map<K, V>(other).swap(*this);
    return *this;
}

template <typename K, typename V>
inline typename map<K, V>::iterator map<K, V>::begin() {
    iterator it;
    it.node = *m_buckets.first;
    return it;
}

template <typename K, typename V>
inline typename map<K, V>::iterator map<K, V>::end() {
    iterator it;
    it.node = 0;
    return it;
}

template <typename K, typename V>
inline typename map<K, V>::const_iterator map<K, V>::begin() const {
    const_iterator cit;
    cit.node = *m_buckets.first;
    return cit;
}

template <typename K, typename V>
inline typename map<K, V>::const_iterator map<K, V>::end() const {
    const_iterator cit;
    cit.node = 0;
    return cit;
}

template <typename K, typename V>
inline bool map<K, V>::empty() const {
    return m_size == 0;
}

template <typename K, typename V>
inline size_t map<K, V>::size() const {
    return m_size;
}

template <typename K, typename V>
inline void map<K, V>::clear() {
    pointer it = *m_buckets.first;
    while (it) {
        const pointer next = it->next;
        it->~hash_node<K, V>();
        free(it);
        it = next;
    }
    m_buckets.last = m_buckets.first;
    m_buckets.resize(9, 0);
    m_size = 0;
}

template <typename K, typename V>
inline typename map<K, V>::iterator map<K, V>::find(const K &key) {
    iterator result;
    result.node = hash_find(key, m_buckets.first, size_t(m_buckets.last - m_buckets.first));
    return result;
}

template <typename K, typename V>
inline typename map<K, V>::const_iterator map<K, V>::find(const K &key) const {
    iterator result;
    result.node = hash_find(key, m_buckets.first, size_t(m_buckets.last - m_buckets.first));
    return result;
}

template <typename K, typename V>
inline pair<typename map<K, V>::iterator, bool> map<K, V>::insert(const pair<K, V> &p) {
    pair<iterator, bool> result;
    result.second = false;

    result.first = find(p.first);
    if (result.first.node != 0)
        return result;

    unsigned char *data = (unsigned char *)malloc(sizeof(hash_node<K, V>));
    hash_node<K, V> *newnode = new(data) hash_node<K, V>(p.first, p.second);
    newnode->next = 0;
    newnode->prev = 0;

    const size_t nbuckets = size_t(m_buckets.last - m_buckets.first);
    hash_node_insert(newnode, hash(p.first), m_buckets.first, nbuckets - 1);

    ++m_size;
    if (m_size + 1 > 4 * nbuckets) {
        pointer root = *m_buckets.first;

        const size_t newnbuckets = (size_t(m_buckets.last - m_buckets.first) - 1) * 8;
        m_buckets.last = m_buckets.first;
        m_buckets.resize(newnbuckets + 1, 0);
        hash_node<K, V> **buckets = m_buckets.first;

        while (root) {
            const pointer next = root->next;
            root->next = 0;
            root->prev = 0;
            hash_node_insert(root, hash(root->first), buckets, newnbuckets);
            root = next;
        }
    }

    result.first.node = newnode;
    result.second = true;
    return result;
}

template <typename K, typename V>
void map<K, V>::erase(const_iterator where) {
    hash_node_erase(where.node, hash(where->first), m_buckets.first,
        size_t(m_buckets.last - m_buckets.first) - 1);
    where->~hash_node<K, V>();
    free(where.node);
    --m_size;
}

template <typename K, typename V>
V &map<K, V>::operator[](const K &key) {
    return insert(pair<K, V>(key, V())).first->second;
}

template <typename K, typename V>
void map<K, V>::swap(map &other) {
    size_t tsize = other.m_size;
    other.m_size = m_size;
    m_size = tsize;
    m_buckets.swap(other.m_buckets);
}

}

#endif
